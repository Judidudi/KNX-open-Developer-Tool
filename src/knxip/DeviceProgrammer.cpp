#include "DeviceProgrammer.h"

#include "IKnxInterface.h"
#include "CemiFrame.h"
#include "TableBuilder.h"
#include "../core/DeviceInstance.h"
#include "../core/KnxApplicationProgram.h"

#include <QTimer>
#include <QLoggingCategory>

// Maximum bytes per A_Memory_Write frame.  12 bytes is a safe value that all
// KNX TP devices must support regardless of their MaxAPDU configuration.
static constexpr int kChunkBytes = 12;

DeviceProgrammer::DeviceProgrammer(IKnxInterface *iface,
                                   DeviceInstance *device,
                                   const KnxApplicationProgram *appProgram,
                                   QObject *parent)
    : QObject(parent)
    , m_iface(iface)
    , m_device(device)
    , m_appProgram(appProgram)
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &DeviceProgrammer::runNextStep);
}

QString DeviceProgrammer::stepLabel(Step s)
{
    switch (s) {
    case StepWaitProgMode:          return tr("Auf Programmiermodus warten");
    case StepWritePhysAddress:      return tr("Physikalische Adresse schreiben");
    case StepWriteAddressTable:     return tr("Adresstabelle schreiben");
    case StepWriteAssociationTable: return tr("Assoziationstabelle schreiben");
    case StepWriteParameters:       return tr("Parameter schreiben");
    case StepVerifyParameters:      return tr("Parameter verifizieren");
    case StepRestart:               return tr("Gerät neu starten");
    case StepDone:                  return tr("Fertig");
    }
    return {};
}

void DeviceProgrammer::start()
{
    if (m_running)
        return;
    if (!m_iface || !m_device || !m_appProgram) {
        emit finished(false, tr("Ungültiger Programmer-Zustand"));
        return;
    }
    if (!m_iface->isConnected()) {
        emit finished(false, tr("Kein aktives KNX-Interface verbunden"));
        return;
    }
    m_running        = true;
    m_step           = StepWaitProgMode;
    m_verifyStarted  = false;
    m_memChunks.clear();
    emit stepStarted(m_step,
                     tr("Bitte Programmiertaster am Gerät drücken (Prog-LED muss leuchten)."));
    m_timer->start(m_progModeTimeoutMs);
}

void DeviceProgrammer::cancel()
{
    m_running = false;
    m_timer->stop();
    if (m_verifyConn)
        disconnect(m_verifyConn);
    m_memChunks.clear();
    emit finished(false, tr("Programmierung abgebrochen."));
}

// ─── Memory write helper ──────────────────────────────────────────────────────

void DeviceProgrammer::enqueueMemoryWrite(uint16_t destPa,
                                           uint16_t memBase,
                                           const QByteArray &data)
{
    for (int offset = 0; offset < data.size(); offset += kChunkBytes) {
        MemChunk c;
        c.destPa  = destPa;
        c.memAddr = static_cast<uint16_t>(memBase + offset);
        c.data    = data.mid(offset, kChunkBytes);
        m_memChunks.enqueue(c);
    }
}

void DeviceProgrammer::sendAndAdvance(const QByteArray &frame, int delayMs)
{
    m_iface->sendCemiFrame(frame);
    emit stepCompleted(m_step);
    ++m_step;
    emit progressUpdated(m_step * 100 / StepDone);
    m_timer->start(delayMs);
}

// ─── Main state machine ───────────────────────────────────────────────────────

void DeviceProgrammer::runNextStep()
{
    if (!m_running)
        return;

    // ── Drain pending memory write chunks before advancing to the next step ──
    if (!m_memChunks.isEmpty()) {
        const MemChunk c = m_memChunks.dequeue();
        m_iface->sendCemiFrame(CemiFrame::buildMemoryWrite(c.destPa, c.memAddr, c.data));
        if (m_memChunks.isEmpty()) {
            // Last chunk: now advance step
            emit stepCompleted(m_step);
            ++m_step;
            emit progressUpdated(m_step * 100 / StepDone);
        }
        m_timer->start(400);
        return;
    }

    const uint16_t pa  = CemiFrame::physAddrFromString(m_device->physicalAddress());
    const DeviceMemoryImage img = TableBuilder::build(*m_device, *m_appProgram);

    switch (m_step) {

    case StepWaitProgMode: {
        m_step = StepWritePhysAddress;
        emit stepStarted(m_step, stepLabel(StepWritePhysAddress));
        sendAndAdvance(CemiFrame::buildIndividualAddressWrite(pa));
        break;
    }

    case StepWriteAddressTable: {
        emit stepStarted(m_step, stepLabel(StepWriteAddressTable));
        enqueueMemoryWrite(pa, m_appProgram->memoryLayout.addressTable, img.addressTable);
        m_timer->start(400);
        break;
    }

    case StepWriteAssociationTable: {
        emit stepStarted(m_step, stepLabel(StepWriteAssociationTable));
        enqueueMemoryWrite(pa, m_appProgram->memoryLayout.associationTable, img.associationTable);
        m_timer->start(400);
        break;
    }

    case StepWriteParameters: {
        emit stepStarted(m_step, stepLabel(StepWriteParameters));
        enqueueMemoryWrite(pa, m_appProgram->memoryLayout.parameterBase, img.parameterBlock);
        m_timer->start(400);
        break;
    }

    case StepVerifyParameters: {
        if (!m_verifyStarted) {
            // First entry: send Memory_Read and wait for response
            m_verifyStarted = true;
            emit stepStarted(m_step, stepLabel(StepVerifyParameters));
            const uint32_t pSize = m_appProgram->memoryLayout.parameterSize;
            if (pSize == 0) {
                // Nothing to verify
                emit stepCompleted(m_step);
                ++m_step;
                m_timer->start(100);
                break;
            }
            const uint8_t readCount = static_cast<uint8_t>(qMin<uint32_t>(pSize, 12u));
            m_verifyConn = connect(m_iface, &IKnxInterface::cemiFrameReceived,
                                   this, &DeviceProgrammer::onCemiReceivedForVerify);
            m_iface->sendCemiFrame(CemiFrame::buildMemoryRead(
                pa, m_appProgram->memoryLayout.parameterBase, readCount));
            m_timer->start(2000); // 2 s timeout — non-fatal
        } else {
            // Timer fired = timeout; treat as non-fatal warning and continue
            if (m_verifyConn)
                disconnect(m_verifyConn);
            qWarning("DeviceProgrammer: Parameter-Verifikation Timeout – wird fortgesetzt");
            emit stepCompleted(m_step);
            ++m_step;
            m_timer->start(100);
        }
        break;
    }

    case StepRestart: {
        emit stepStarted(m_step, stepLabel(StepRestart));
        sendAndAdvance(CemiFrame::buildRestart(pa), 1000);
        break;
    }

    case StepDone: {
        m_running = false;
        emit progressUpdated(100);
        emit finished(true, tr("Programmierung erfolgreich abgeschlossen."));
        break;
    }

    default:
        m_running = false;
        emit finished(false, tr("Unbekannter Schritt: %1").arg(m_step));
        break;
    }
}

void DeviceProgrammer::onCemiReceivedForVerify(const QByteArray &cemi)
{
    const CemiFrame f = CemiFrame::fromBytes(cemi);
    if (!f.isMemoryResponse())
        return;

    uint16_t addr;
    QByteArray readData;
    if (!f.memoryResponseData(addr, readData))
        return;
    if (addr != m_appProgram->memoryLayout.parameterBase)
        return;

    // Got the response — stop timer and disconnect listener
    m_timer->stop();
    disconnect(m_verifyConn);

    // Compare against expected image
    const DeviceMemoryImage img = TableBuilder::build(*m_device, *m_appProgram);
    const QByteArray expected   = img.parameterBlock.left(readData.size());

    if (readData != expected) {
        m_running = false;
        emit finished(false,
            tr("Parameter-Verifikation fehlgeschlagen.\n"
               "Erwartet: %1\n"
               "Gelesen:  %2")
            .arg(QString::fromLatin1(expected.toHex(' ').toUpper()))
            .arg(QString::fromLatin1(readData.toHex(' ').toUpper())));
        return;
    }

    emit stepCompleted(m_step);
    ++m_step;
    emit progressUpdated(m_step * 100 / StepDone);
    m_timer->start(100);
}
