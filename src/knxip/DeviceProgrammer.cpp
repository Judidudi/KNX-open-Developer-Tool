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
static constexpr int kChunkBytes      = 12;
// Wait after A_IndividualAddress_Write so device can process the new PA (KNX spec guidance).
static constexpr int kPaWriteSettleMs = 400;
// Maximum number of restart-poll attempts before giving up and advancing (BB3).
static constexpr int kRestartMaxPolls = 5;

// ─── Step sequences per mode ─────────────────────────────────────────────────

QList<DeviceProgrammer::Step> DeviceProgrammer::stepSequence() const
{
    switch (m_mode) {
    case Mode::Full:
        return {
            StepWaitProgMode,
            StepWritePhysAddress,
            StepCheckDescriptor,
            StepConnect,
            StepLoadStartAddrTable,
            StepWriteAddressTable,
            StepLoadEndAddrTable,
            StepLoadStartAssocTable,
            StepWriteAssociationTable,
            StepLoadEndAssocTable,
            StepLoadStartAppProgram,
            StepWriteParameters,
            StepLoadEndAppProgram,
            StepVerifyParameters,
            StepRestart,
            StepWaitRestart,
            StepDisconnect,
            StepDone,
        };
    case Mode::ApplicationOnly:
        return {
            StepCheckDescriptor,
            StepConnect,
            StepLoadStartAddrTable,
            StepWriteAddressTable,
            StepLoadEndAddrTable,
            StepLoadStartAssocTable,
            StepWriteAssociationTable,
            StepLoadEndAssocTable,
            StepLoadStartAppProgram,
            StepWriteParameters,
            StepLoadEndAppProgram,
            StepVerifyParameters,
            StepRestart,
            StepWaitRestart,
            StepDisconnect,
            StepDone,
        };
    case Mode::PhysicalAddressOnly:
        return {
            StepWaitProgMode,
            StepWritePhysAddress,
            StepDone,
        };
    case Mode::VerifyOnly:
        return {
            StepConnect,
            StepVerifyParameters,
            StepDisconnect,
            StepDone,
        };
    }
    return {};
}

// ─── Labels ──────────────────────────────────────────────────────────────────

QString DeviceProgrammer::stepLabel(Step s)
{
    switch (s) {
    case StepWaitProgMode:          return tr("Auf Programmiermodus warten");
    case StepWritePhysAddress:      return tr("Physikalische Adresse schreiben");
    case StepCheckDescriptor:       return tr("Gerätedeskriptor prüfen");
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
    case StepWaitRestart:           return tr("Gerät warten (Neustart)");
    case StepDisconnect:            return tr("Verbindung trennen");
    case StepDone:                  return tr("Fertig");
    }
    return {};
}

QString DeviceProgrammer::modeLabel(Mode m)
{
    switch (m) {
    case Mode::Full:               return tr("Vollständig (PA + Anwendung)");
    case Mode::ApplicationOnly:    return tr("Nur Anwendung (PA bleibt)");
    case Mode::PhysicalAddressOnly:return tr("Nur Physikalische Adresse");
    case Mode::VerifyOnly:         return tr("Nur Verifizieren (kein Schreiben)");
    }
    return {};
}

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

    m_sequence            = stepSequence();
    m_seqIdx              = 0;
    m_running             = true;
    m_progResponseCount   = 0;
    m_verifyAwaiting      = false;
    m_restartPollCount    = 0;
    m_loadingObjects.clear();
    m_expectedParamBlock.clear();

    connect(m_iface, &IKnxInterface::cemiFrameReceived,
            this, &DeviceProgrammer::onCemiReceivedGlobal);

    emit stepStarted(static_cast<int>(m_sequence.first()), stepLabel(m_sequence.first()));
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

// ─── Internal: helpers ───────────────────────────────────────────────────────

DeviceProgrammer::Step DeviceProgrammer::currentStep() const
{
    return m_sequence.value(m_seqIdx, StepDone);
}

void DeviceProgrammer::advance()
{
    emit stepCompleted(static_cast<int>(currentStep()));
    ++m_seqIdx;
    const int total = m_sequence.size();
    emit progressUpdated(total > 1 ? (m_seqIdx * 100 / (total - 1)) : 100);
    if (m_seqIdx < total) {
        const Step next = m_sequence[m_seqIdx];
        emit stepStarted(static_cast<int>(next), stepLabel(next));
    }
    QMetaObject::invokeMethod(this, "runStep", Qt::QueuedConnection);
}

void DeviceProgrammer::fail(const QString &msg)
{
    m_running = false;
    m_timer->stop();
    m_timer->disconnect();
    m_loadingObjects.clear();
    disconnect(m_iface, &IKnxInterface::cemiFrameReceived,
               this, &DeviceProgrammer::onCemiReceivedGlobal);
    if (m_transport->isOpen())
        m_transport->close();
    emit finished(false, msg);
}

void DeviceProgrammer::runStep()
{
    if (!m_running) return;
    switch (currentStep()) {
    case StepWaitProgMode:          doStepWaitProgMode();    break;
    case StepWritePhysAddress:      doStepWritePhysAddress(); break;
    case StepCheckDescriptor:       doStepCheckDescriptor(); break;
    case StepConnect:               doStepConnect();          break;
    case StepLoadStartAddrTable:    doStepLoadStart(0);      break;
    case StepWriteAddressTable: {
        const auto img = TableBuilder::build(*m_device, *m_appProgram);
        doStepWriteMemory(m_appProgram->memoryLayout.addressTable, img.addressTable);
        break;
    }
    case StepLoadEndAddrTable:      doStepLoadEnd(0);        break;
    case StepLoadStartAssocTable:   doStepLoadStart(1);      break;
    case StepWriteAssociationTable: {
        const auto img = TableBuilder::build(*m_device, *m_appProgram);
        doStepWriteMemory(m_appProgram->memoryLayout.associationTable,
                          img.associationTable);
        break;
    }
    case StepLoadEndAssocTable:     doStepLoadEnd(1);        break;
    case StepLoadStartAppProgram:   doStepLoadStart(2);      break;
    case StepWriteParameters: {
        const auto img = TableBuilder::build(*m_device, *m_appProgram);
        doStepWriteMemory(m_appProgram->memoryLayout.parameterBase, img.parameterBlock);
        break;
    }
    case StepLoadEndAppProgram:     doStepLoadEnd(2);        break;
    case StepVerifyParameters:      doStepVerify();           break;
    case StepRestart:               doStepRestart();          break;
    case StepWaitRestart:           doStepWaitRestart();      break;
    case StepDisconnect:            doStepDisconnect();       break;
    case StepDone:                  doStepDone();             break;
    }
}

// ─── Step implementations ─────────────────────────────────────────────────────

void DeviceProgrammer::doStepWaitProgMode()
{
    m_progResponseCount = 0;
    m_iface->sendCemiFrame(CemiFrame::buildIndividualAddressRead());

    m_timer->disconnect();
    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_timer->disconnect();
        if (!m_running) return;
        if (m_progResponseCount == 0)
            fail(tr("Kein Gerät im Programmiermodus gefunden.\n"
                    "Bitte Programmiertaste am Gerät drücken und erneut versuchen."));
    });
    m_timer->start(m_progModeTimeoutMs);
}

void DeviceProgrammer::doStepWritePhysAddress()
{
    const uint16_t pa = CemiFrame::physAddrFromString(m_device->physicalAddress());
    m_iface->sendCemiFrame(CemiFrame::buildIndividualAddressWrite(pa));

    m_timer->disconnect();
    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_timer->disconnect();
        if (m_running) advance();
    });
    m_timer->start(kPaWriteSettleMs);
}

void DeviceProgrammer::doStepCheckDescriptor()
{
    // Send unicast unconnected A_DeviceDescriptor_Read(0) to the target PA.
    // The response tells us the BCU mask version so we can verify compatibility.
    const uint16_t pa = CemiFrame::physAddrFromString(m_device->physicalAddress());
    m_iface->sendCemiFrame(CemiFrame::buildDeviceDescriptorRead(pa));

    m_timer->disconnect();
    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_timer->disconnect();
        if (!m_running) return;
        // Device did not respond — it might not be reachable at this PA yet
        // (e.g. after fresh PA write, some devices need a moment).
        // Emit a warning and continue rather than aborting.
        emit warningOccurred(
            tr("Gerät %1 antwortete nicht auf Gerätedeskriptor-Anfrage.\n"
               "Möglicherweise ist es noch nicht bereit – Programmierung wird fortgesetzt.")
                .arg(m_device->physicalAddress()));
        advance();
    });
    m_timer->start(m_descriptorCheckTimeoutMs);
}

void DeviceProgrammer::doStepConnect()
{
    const uint16_t pa = CemiFrame::physAddrFromString(m_device->physicalAddress());
    m_transport->open(pa);
}

void DeviceProgrammer::doStepLoadStart(uint8_t objIdx)
{
    if (!m_useLoadState) { advance(); return; }
    m_loadingObjects.insert(objIdx);
    // PID 5 = LoadStateControl, value 1 = StartLoading
    m_transport->sendPropertyWrite(objIdx, 5, 1, 1, QByteArray(1, char(1)));
}

void DeviceProgrammer::doStepLoadEnd(uint8_t objIdx)
{
    if (!m_useLoadState) { advance(); return; }
    m_loadingObjects.remove(objIdx);
    // PID 5 = LoadStateControl, value 2 = LoadCompleted
    m_transport->sendPropertyWrite(objIdx, 5, 1, 1, QByteArray(1, char(2)));
}

void DeviceProgrammer::doStepWriteMemory(uint16_t baseAddr, const QByteArray &block)
{
    if (block.isEmpty()) { advance(); return; }
    for (int off = 0; off < block.size(); off += kChunkBytes) {
        const auto addr = static_cast<uint16_t>(baseAddr + off);
        m_transport->sendMemoryWrite(addr, block.mid(off, kChunkBytes));
    }
}

void DeviceProgrammer::doStepVerify()
{
    if (!m_verifyEnabled) { advance(); return; }

    const auto img       = TableBuilder::build(*m_device, *m_appProgram);
    m_expectedParamBlock = img.parameterBlock;

    if (m_appProgram->memoryLayout.parameterSize == 0) { advance(); return; }

    m_verifyAwaiting = true;

    m_timer->disconnect();
    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_timer->disconnect();
        if (!m_verifyAwaiting || !m_running) return;
        m_verifyAwaiting = false;
        fail(tr("Parameter-Verifikation Timeout – Gerät antwortete nicht auf Memory_Read.\n"
                "Bitte Programmierung wiederholen."));
    });
    m_timer->start(3000);

    const uint8_t cnt =
        static_cast<uint8_t>(qMin<uint32_t>(m_appProgram->memoryLayout.parameterSize, 12u));
    m_transport->sendMemoryRead(m_appProgram->memoryLayout.parameterBase, cnt);
}

void DeviceProgrammer::doStepRestart()
{
    m_transport->sendRestart();
    // Transport may close after restart (device sends T_Disconnect).
    // Both onTransportIdle and onTransportClosed advance to StepWaitRestart.
}

void DeviceProgrammer::doStepWaitRestart()
{
    // Transport is already closed at this point.
    // Wait a settle period, then poll with DeviceDescriptor_Read.
    m_restartPollCount = 0;
    const uint16_t pa  = CemiFrame::physAddrFromString(m_device->physicalAddress());

    m_timer->disconnect();
    connect(m_timer, &QTimer::timeout, this, [this, pa]() {
        if (!m_running) return;
        if (m_restartPollCount == 0) {
            // Settle period elapsed — start polling
            m_iface->sendCemiFrame(CemiFrame::buildDeviceDescriptorRead(pa));
            ++m_restartPollCount;
            m_timer->start(m_restartPollIntervalMs);
        } else if (m_restartPollCount < kRestartMaxPolls) {
            m_iface->sendCemiFrame(CemiFrame::buildDeviceDescriptorRead(pa));
            ++m_restartPollCount;
            m_timer->start(m_restartPollIntervalMs);
        } else {
            // Device took too long — advance anyway; restart likely happened
            m_timer->disconnect();
            emit warningOccurred(
                tr("Gerät %1 meldete sich nach dem Neustart nicht zurück.\n"
                   "Neustart wurde wahrscheinlich trotzdem durchgeführt.")
                    .arg(m_device->physicalAddress()));
            advance();
        }
    });
    m_timer->start(m_restartSettleMs);  // initial settle
}

void DeviceProgrammer::doStepDisconnect()
{
    if (m_transport->isOpen()) {
        m_transport->close();
        // onTransportClosed → advance
    } else {
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

    m_transport->handleIncoming(cemi);

    const CemiFrame f = CemiFrame::fromBytes(cemi);

    // ── StepWaitProgMode: count IndividualAddress_Response ────────────────────
    if (currentStep() == StepWaitProgMode) {
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
                    "bitte nur ein Gerät gleichzeitig aktivieren."));
        }
        return;
    }

    // ── StepCheckDescriptor: verify mask version ──────────────────────────────
    if (currentStep() == StepCheckDescriptor) {
        if (!f.isDeviceDescriptorResponse()) return;
        if (f.groupAddress) return;
        const uint16_t pa = CemiFrame::physAddrFromString(m_device->physicalAddress());
        if (f.sourceAddress != pa) return;

        m_timer->stop();
        m_timer->disconnect();

        const uint16_t deviceMask = f.deviceDescriptorMaskVersion();
        const uint16_t expectedMask = m_appProgram->maskVersion;
        if (expectedMask != 0 && deviceMask != 0 && deviceMask != expectedMask) {
            emit warningOccurred(
                tr("Maskenversion-Konflikt: Gerät meldet 0x%1, "
                   "Anwendungsprogramm erwartet 0x%2.\n"
                   "Das Gerät könnte inkompatibel sein – Programmierung wird trotzdem versucht.")
                    .arg(deviceMask, 4, 16, QLatin1Char('0'))
                    .arg(expectedMask, 4, 16, QLatin1Char('0')));
        }
        advance();
        return;
    }

    // ── StepWaitRestart: watch for device coming back online ──────────────────
    if (currentStep() == StepWaitRestart) {
        if (!f.isDeviceDescriptorResponse()) return;
        if (f.groupAddress) return;
        const uint16_t pa = CemiFrame::physAddrFromString(m_device->physicalAddress());
        if (f.sourceAddress != pa) return;
        // Device responded — it's back online
        m_timer->stop();
        m_timer->disconnect();
        advance();
    }
}

void DeviceProgrammer::onTransportOpened()
{
    if (!m_running || currentStep() != StepConnect) return;
    advance();
}

void DeviceProgrammer::onTransportIdle()
{
    if (!m_running) return;

    switch (currentStep()) {
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

    const Step s = currentStep();
    if (s == StepDisconnect) {
        advance();
    } else if (s == StepRestart) {
        // Device disconnected after restart — expected; proceed to WaitRestart
        advance();
    } else {
        const QString context = stepLabel(s);
        QString hint;
        if (!m_loadingObjects.isEmpty()) {
            hint = tr("\nHinweis: Der Ladevorgang war noch nicht abgeschlossen. "
                      "Das Gerät könnte in einem inkonsistenten Zustand sein. "
                      "Bitte Reset/Programmiertaste drücken und neu versuchen.");
        }
        fail(tr("KNX-Verbindung unerwartet getrennt (Schritt: %1).%2")
                 .arg(context, hint));
    }
}

void DeviceProgrammer::onTransportError(const QString &msg)
{
    if (!m_running) return;

    if (currentStep() == StepVerifyParameters) {
        m_verifyAwaiting = false;
        m_timer->stop();
        m_timer->disconnect();
        fail(tr("Parameter-Verifikation fehlgeschlagen: %1\n"
                "Bitte Programmierung wiederholen.").arg(msg));
    } else {
        const QString hint = m_loadingObjects.isEmpty()
            ? QString{}
            : tr("\nHinweis: Ladevorgang war aktiv. Gerät ggf. in inkonsistentem Zustand. "
                 "Reset/Programmiertaste drücken und erneut versuchen.");
        fail(tr("Transportfehler: %1%2").arg(msg, hint));
    }
}

void DeviceProgrammer::onTransportApdu(const QByteArray &apdu)
{
    if (!m_running) return;
    if (currentStep() != StepVerifyParameters || !m_verifyAwaiting) return;

    m_timer->stop();
    m_timer->disconnect();
    m_verifyAwaiting = false;

    // Memory_Response APDU (after TPCI is stripped):
    //   [0] = 0x40 | count   (APCI = 0x240, bits[9:8] in TPCI)
    //   [1] = addr_hi
    //   [2] = addr_lo
    //   [3..3+count-1] = data
    if (apdu.size() < 3) { advance(); return; }
    const uint8_t b0 = static_cast<uint8_t>(apdu[0]);
    if ((b0 & 0xC0) != 0x40) { advance(); return; }

    const int cnt = b0 & 0x3F;
    if (apdu.size() < 3 + cnt) { advance(); return; }

    const uint16_t addr = (static_cast<uint8_t>(apdu[1]) << 8)
                        |  static_cast<uint8_t>(apdu[2]);
    if (addr != m_appProgram->memoryLayout.parameterBase) { advance(); return; }

    const QByteArray data     = apdu.mid(3, cnt);
    const QByteArray expected = m_expectedParamBlock.left(data.size());

    if (data != expected) {
        fail(tr("Parameter-Verifikation fehlgeschlagen.\n"
                "Erwartet: %1\n"
                "Gelesen:  %2\n"
                "Bitte Programmierung wiederholen.")
             .arg(QString::fromLatin1(expected.toHex(' ').toUpper()),
                  QString::fromLatin1(data.toHex(' ').toUpper())));
        return;
    }
    advance();
}
