#include "DeviceProgrammer.h"
#include "TransportConnection.h"
#include "IKnxInterface.h"
#include "CemiFrame.h"
#include "TableBuilder.h"
#include "../core/DeviceInstance.h"
#include "../core/KnxApplicationProgram.h"

#include <QMetaObject>
#include <QTimer>

// Max bytes per A_Memory_Write frame (KNX spec minimum APDU, all TP devices must support 12 B).
static constexpr int kChunkBytes = 12;
// Wait after A_IndividualAddress_Write so device can process the new PA (KNX spec guidance).
static constexpr int kPaWriteSettleMs = 400;

// ─── Constructor / Destructor ─────────────────────────────────────────────────

DeviceProgrammer::DeviceProgrammer(IKnxInterface *iface,
                                   DeviceInstance *device,
                                   const KnxApplicationProgram *appProgram,
                                   QObject *parent)
    : QObject(parent)
    , m_iface(iface)
    , m_device(device)
    , m_appProgram(appProgram)
    , m_transport(new TransportConnection(iface, this))
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);

    connect(m_transport, &TransportConnection::opened,
            this, &DeviceProgrammer::onTransportOpened);
    connect(m_transport, &TransportConnection::closed,
            this, &DeviceProgrammer::onTransportClosed);
    connect(m_transport, &TransportConnection::idle,
            this, &DeviceProgrammer::onTransportIdle);
    connect(m_transport, &TransportConnection::apduReceived,
            this, &DeviceProgrammer::onTransportApdu);
    connect(m_transport, &TransportConnection::error,
            this, &DeviceProgrammer::onTransportError);
}

DeviceProgrammer::~DeviceProgrammer() = default;

// ─── Public API ───────────────────────────────────────────────────────────────

void DeviceProgrammer::start()
{
    if (m_running) return;
    if (!m_iface || !m_device || !m_appProgram) {
        emit finished(false, tr("Ungültiger Programmer-Zustand"));
        return;
    }
    if (!m_iface->isConnected()) {
        emit finished(false, tr("Kein aktives KNX-Interface verbunden"));
        return;
    }
    m_running           = true;
    m_step              = StepWaitProgMode;
    m_progResponseCount = 0;
    m_verifyAwaiting    = false;
    m_expectedParamBlock.clear();

    // Keep global listener active for the entire programming session so the
    // transport can receive frames (handleIncoming) at every step.
    connect(m_iface, &IKnxInterface::cemiFrameReceived,
            this, &DeviceProgrammer::onCemiReceivedGlobal);

    emit stepStarted(StepWaitProgMode, stepLabel(StepWaitProgMode));
    QMetaObject::invokeMethod(this, "runStep", Qt::QueuedConnection);
}

void DeviceProgrammer::cancel()
{
    if (!m_running) return;
    m_running = false;
    m_timer->stop();
    m_timer->disconnect();
    disconnect(m_iface, &IKnxInterface::cemiFrameReceived,
               this, &DeviceProgrammer::onCemiReceivedGlobal);
    if (m_transport->isOpen())
        m_transport->close();
    emit finished(false, tr("Programmierung abgebrochen."));
}

void DeviceProgrammer::setTransportAckTimeoutMs(int ms)
{
    m_transport->setAckTimeoutMs(ms);
}

// ─── Step label ───────────────────────────────────────────────────────────────

QString DeviceProgrammer::stepLabel(Step s)
{
    switch (s) {
    case StepWaitProgMode:          return tr("Auf Programmiermodus warten");
    case StepWritePhysAddress:      return tr("Physikalische Adresse schreiben");
    case StepConnect:               return tr("Verbindung aufbauen");
    case StepLoadStartAddrTable:    return tr("Adresstabelle: Laden starten");
    case StepWriteAddressTable:     return tr("Adresstabelle schreiben");
    case StepLoadEndAddrTable:      return tr("Adresstabelle: Laden beenden");
    case StepLoadStartAssocTable:   return tr("Assoziationstabelle: Laden starten");
    case StepWriteAssociationTable: return tr("Assoziationstabelle schreiben");
    case StepLoadEndAssocTable:     return tr("Assoziationstabelle: Laden beenden");
    case StepLoadStartAppProgram:   return tr("Applikationsprogramm: Laden starten");
    case StepWriteParameters:       return tr("Parameter schreiben");
    case StepLoadEndAppProgram:     return tr("Applikationsprogramm: Laden beenden");
    case StepVerifyParameters:      return tr("Parameter verifizieren");
    case StepRestart:               return tr("Gerät neu starten");
    case StepDisconnect:            return tr("Verbindung trennen");
    case StepDone:                  return tr("Fertig");
    }
    return {};
}

// ─── Internal: advance / fail / runStep ──────────────────────────────────────

void DeviceProgrammer::advance()
{
    emit stepCompleted(m_step);
    ++m_step;
    emit progressUpdated(m_step * 100 / static_cast<int>(StepDone));
    if (m_step <= static_cast<int>(StepDone))
        emit stepStarted(m_step, stepLabel(static_cast<Step>(m_step)));
    QMetaObject::invokeMethod(this, "runStep", Qt::QueuedConnection);
}

void DeviceProgrammer::fail(const QString &msg)
{
    m_running = false;
    m_timer->stop();
    m_timer->disconnect();
    disconnect(m_iface, &IKnxInterface::cemiFrameReceived,
               this, &DeviceProgrammer::onCemiReceivedGlobal);
    if (m_transport->isOpen())
        m_transport->close();
    emit finished(false, msg);
}

void DeviceProgrammer::runStep()
{
    if (!m_running) return;
    const auto s = static_cast<Step>(m_step);
    switch (s) {
    case StepWaitProgMode:          doStepWaitProgMode();    break;
    case StepWritePhysAddress:      doStepWritePhysAddress(); break;
    case StepConnect:               doStepConnect();          break;
    case StepLoadStartAddrTable:    doStepLoadStart(0, s);   break;
    case StepWriteAddressTable: {
        const auto img = TableBuilder::build(*m_device, *m_appProgram);
        doStepWriteMemory(m_appProgram->memoryLayout.addressTable, img.addressTable, s);
        break;
    }
    case StepLoadEndAddrTable:      doStepLoadEnd(0, s);     break;
    case StepLoadStartAssocTable:   doStepLoadStart(1, s);   break;
    case StepWriteAssociationTable: {
        const auto img = TableBuilder::build(*m_device, *m_appProgram);
        doStepWriteMemory(m_appProgram->memoryLayout.associationTable,
                          img.associationTable, s);
        break;
    }
    case StepLoadEndAssocTable:     doStepLoadEnd(1, s);     break;
    case StepLoadStartAppProgram:   doStepLoadStart(2, s);   break;
    case StepWriteParameters: {
        const auto img = TableBuilder::build(*m_device, *m_appProgram);
        doStepWriteMemory(m_appProgram->memoryLayout.parameterBase, img.parameterBlock, s);
        break;
    }
    case StepLoadEndAppProgram:     doStepLoadEnd(2, s);     break;
    case StepVerifyParameters:      doStepVerify();           break;
    case StepRestart:               doStepRestart();          break;
    case StepDisconnect:            doStepDisconnect();       break;
    case StepDone:                  doStepDone();             break;
    }
}

// ─── Step implementations ─────────────────────────────────────────────────────

void DeviceProgrammer::doStepWaitProgMode()
{
    m_progResponseCount = 0;
    // Ask all devices in programming mode to identify themselves
    m_iface->sendCemiFrame(CemiFrame::buildIndividualAddressRead());

    // The global cemi listener (onCemiReceivedGlobal) counts responses.
    // If timer fires before any response: no device → fail.
    m_timer->disconnect();
    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_timer->disconnect();
        if (!m_running) return;
        if (m_progResponseCount == 0)
            fail(tr("Kein Gerät im Programmiermodus gefunden. "
                    "Bitte Programmiertaste am Gerät drücken."));
        // else: already handled by onCemiReceivedGlobal
    });
    m_timer->start(m_progModeTimeoutMs);
}

void DeviceProgrammer::doStepWritePhysAddress()
{
    const uint16_t pa = CemiFrame::physAddrFromString(m_device->physicalAddress());
    m_iface->sendCemiFrame(CemiFrame::buildIndividualAddressWrite(pa));

    // Give the device time to store the new PA (KNX spec recommends ~300 ms).
    m_timer->disconnect();
    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_timer->disconnect();
        if (m_running) advance();
    });
    m_timer->start(kPaWriteSettleMs);
}

void DeviceProgrammer::doStepConnect()
{
    const uint16_t pa = CemiFrame::physAddrFromString(m_device->physicalAddress());
    m_transport->open(pa);
    // TransportConnection::opened() fires synchronously → onTransportOpened → advance
}

void DeviceProgrammer::doStepLoadStart(uint8_t objIdx, Step /*ownStep*/)
{
    if (!m_useLoadState) { advance(); return; }
    // PID 5 = LoadStateControl, value 1 = StartLoading
    m_transport->sendPropertyWrite(objIdx, 5, 1, 1, QByteArray(1, char(1)));
    // onTransportIdle → advance
}

void DeviceProgrammer::doStepLoadEnd(uint8_t objIdx, Step /*ownStep*/)
{
    if (!m_useLoadState) { advance(); return; }
    // PID 5 = LoadStateControl, value 2 = LoadCompleted
    m_transport->sendPropertyWrite(objIdx, 5, 1, 1, QByteArray(1, char(2)));
    // onTransportIdle → advance
}

void DeviceProgrammer::doStepWriteMemory(uint16_t baseAddr,
                                          const QByteArray &block,
                                          Step /*ownStep*/)
{
    if (block.isEmpty()) { advance(); return; }
    for (int off = 0; off < block.size(); off += kChunkBytes) {
        const auto addr  = static_cast<uint16_t>(baseAddr + off);
        m_transport->sendMemoryWrite(addr, block.mid(off, kChunkBytes));
    }
    // onTransportIdle (after last ACK) → advance
}

void DeviceProgrammer::doStepVerify()
{
    if (!m_verifyEnabled) { advance(); return; }

    const auto img       = TableBuilder::build(*m_device, *m_appProgram);
    m_expectedParamBlock = img.parameterBlock;

    if (m_appProgram->memoryLayout.parameterSize == 0) { advance(); return; }

    m_verifyAwaiting = true;

    // Non-fatal 2-second timeout — if device doesn't respond we continue anyway.
    m_timer->disconnect();
    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_timer->disconnect();
        if (!m_verifyAwaiting || !m_running) return;
        m_verifyAwaiting = false;
        qWarning("DeviceProgrammer: Parameter-Verifikation Timeout – wird fortgesetzt");
        advance();
    });
    m_timer->start(2000);

    const uint8_t cnt =
        static_cast<uint8_t>(qMin<uint32_t>(m_appProgram->memoryLayout.parameterSize, 12u));
    m_transport->sendMemoryRead(m_appProgram->memoryLayout.parameterBase, cnt);
}

void DeviceProgrammer::doStepRestart()
{
    m_transport->sendRestart();
    // onTransportIdle → advance; or onTransportClosed if device disconnects first
}

void DeviceProgrammer::doStepDisconnect()
{
    if (m_transport->isOpen()) {
        m_transport->close();
        // onTransportClosed → advance
    } else {
        // Transport already closed, e.g. device disconnected after restart
        advance();
    }
}

void DeviceProgrammer::doStepDone()
{
    m_running = false;
    disconnect(m_iface, &IKnxInterface::cemiFrameReceived,
               this, &DeviceProgrammer::onCemiReceivedGlobal);
    emit progressUpdated(100);
    emit finished(true, tr("Programmierung erfolgreich abgeschlossen."));
}

// ─── Signal handlers ──────────────────────────────────────────────────────────

void DeviceProgrammer::onCemiReceivedGlobal(const QByteArray &cemi)
{
    if (!m_running) return;

    // Forward every frame to the transport connection (handles T_ACK / T_Data_Connected).
    m_transport->handleIncoming(cemi);

    // During prog-mode detection: count IndividualAddress_Response frames.
    if (m_step != StepWaitProgMode) return;

    const CemiFrame f = CemiFrame::fromBytes(cemi);
    if (!f.isIndividualAddressResponse()) return;

    ++m_progResponseCount;
    if (m_progResponseCount == 1) {
        m_timer->stop();
        m_timer->disconnect();
        advance();
    } else {
        m_timer->stop();
        m_timer->disconnect();
        fail(tr("Mehrere Geräte im Programmiermodus erkannt – "
                "bitte nur ein Gerät aktivieren."));
    }
}

void DeviceProgrammer::onTransportOpened()
{
    if (!m_running || m_step != StepConnect) return;
    advance();
}

void DeviceProgrammer::onTransportIdle()
{
    if (!m_running) return;

    // Fired when the transport queue drains after the current batch.
    switch (static_cast<Step>(m_step)) {
    case StepLoadStartAddrTable:
    case StepWriteAddressTable:
    case StepLoadEndAddrTable:
    case StepLoadStartAssocTable:
    case StepWriteAssociationTable:
    case StepLoadEndAssocTable:
    case StepLoadStartAppProgram:
    case StepWriteParameters:
    case StepLoadEndAppProgram:
    case StepRestart:
        advance();
        break;
    default:
        break;
    }
}

void DeviceProgrammer::onTransportClosed()
{
    if (!m_running) return;

    const auto s = static_cast<Step>(m_step);
    if (s == StepDisconnect || s == StepRestart) {
        advance();
    } else {
        fail(tr("KNX-Verbindung unerwartet getrennt (Schritt: %1)").arg(stepLabel(s)));
    }
}

void DeviceProgrammer::onTransportError(const QString &msg)
{
    if (!m_running) return;

    if (m_step == StepVerifyParameters) {
        m_verifyAwaiting = false;
        m_timer->stop();
        m_timer->disconnect();
        qWarning("DeviceProgrammer: Verifikationsfehler (nicht fatal): %s", qPrintable(msg));
        advance();
    } else {
        fail(tr("Transportfehler: %1").arg(msg));
    }
}

void DeviceProgrammer::onTransportApdu(const QByteArray &apdu)
{
    if (!m_running) return;
    if (m_step != StepVerifyParameters || !m_verifyAwaiting) return;

    m_timer->stop();
    m_timer->disconnect();
    m_verifyAwaiting = false;

    // After TransportConnection strips the TPCI byte, a Memory_Response looks like:
    //   [0] = APCI[7:0] = 0x40 | count   (APCI = 0x240, bits[9:8] were in TPCI)
    //   [1] = addr_hi
    //   [2] = addr_lo
    //   [3..3+count-1] = data
    if (apdu.size() < 3) { advance(); return; }
    const uint8_t b0 = static_cast<uint8_t>(apdu[0]);
    if ((b0 & 0xC0) != 0x40) { advance(); return; }   // not a memory response

    const int      cnt  = b0 & 0x3F;
    if (apdu.size() < 3 + cnt) { advance(); return; }

    const uint16_t addr = (static_cast<uint8_t>(apdu[1]) << 8)
                        |  static_cast<uint8_t>(apdu[2]);
    if (addr != m_appProgram->memoryLayout.parameterBase) { advance(); return; }

    const QByteArray data     = apdu.mid(3, cnt);
    const QByteArray expected = m_expectedParamBlock.left(data.size());

    if (data != expected) {
        fail(tr("Parameter-Verifikation fehlgeschlagen.\n"
                "Erwartet: %1\n"
                "Gelesen:  %2")
             .arg(QString::fromLatin1(expected.toHex(' ').toUpper()))
             .arg(QString::fromLatin1(data.toHex(' ').toUpper())));
        return;
    }
    advance();
}
