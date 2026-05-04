#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QList>
#include <QSet>

class IKnxInterface;
class DeviceInstance;
class KnxApplicationProgram;
class TransportConnection;
class QTimer;

// ETS-compatible programming sequence (KNX spec 03_05_01 + 03_03_07).
//
// Three programming modes:
//
//   Full            – detect prog-mode device → write PA → check descriptor →
//                     download tables+params → verify → restart → wait for device
//
//   ApplicationOnly – check descriptor → download tables+params → verify →
//                     restart → wait for device  (PA stays unchanged)
//
//   PhysicalAddress – detect prog-mode device → write PA → done
//
//   VerifyOnly      – connect → read-back parameters → disconnect
//
// The sequence of steps is variable: use stepSequence() to get the actual list
// for a given mode (before calling start()), then build the UI from that list.
// Signals stepStarted/stepCompleted carry a sequence index (0-based), NOT the
// Step enum value, so ProgramDialog can index directly into the step list.
class DeviceProgrammer : public QObject
{
    Q_OBJECT

public:
    // ── Programming mode ───────────────────────────────────────────────────────
    enum class Mode {
        Full,               // PA-write + full application download
        ApplicationOnly,    // Application download only; PA stays as-is
        PhysicalAddressOnly,// PA-write only; no application
        VerifyOnly,         // Read-back verify; no writes
    };
    Q_ENUM(Mode)

    // ── Individual steps ───────────────────────────────────────────────────────
    enum Step {
        StepWaitProgMode,
        StepWritePhysAddress,
        StepCheckDescriptor,     // BB1: read device descriptor, verify mask version
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
        StepWaitRestart,         // BB3: poll device until it comes back online
        StepDisconnect,
        StepDone,
    };
    Q_ENUM(Step)

    DeviceProgrammer(IKnxInterface               *iface,
                     DeviceInstance              *device,
                     const KnxApplicationProgram *appProgram,
                     QObject *parent = nullptr);
    ~DeviceProgrammer() override;

    void start();
    void cancel();

    // ── Tunables ───────────────────────────────────────────────────────────────
    void setMode(Mode mode)              { m_mode = mode; }
    Mode mode() const                    { return m_mode; }
    void setProgModeTimeout(int ms)      { m_progModeTimeoutMs = ms; }
    void setLoadStateMachineEnabled(bool e) { m_useLoadState = e; }
    void setVerifyEnabled(bool e)        { m_verifyEnabled = e; }
    void setTransportAckTimeoutMs(int ms);
    // Tunable timeouts for new steps (useful for tests)
    void setDescriptorCheckTimeoutMs(int ms) { m_descriptorCheckTimeoutMs = ms; }
    void setRestartSettleMs(int ms)          { m_restartSettleMs = ms; }
    void setRestartPollIntervalMs(int ms)    { m_restartPollIntervalMs = ms; }

    // Returns the step sequence for the current mode.
    // Call after setMode() and before start() to build the UI step list.
    QList<Step> stepSequence() const;

    static QString stepLabel(Step s);
    static QString modeLabel(Mode m);

signals:
    // step carries the Step enum value (cast to int); use stepSequence().indexOf()
    // in ProgramDialog to map it to a list row.
    void stepStarted(int step, const QString &description);
    void stepCompleted(int step);
    void progressUpdated(int percent);
    void warningOccurred(const QString &message);   // non-fatal; programming continues
    void finished(bool success, const QString &message);

private slots:
    void runStep();
    void onCemiReceivedGlobal(const QByteArray &cemi);
    void onTransportIdle();
    void onTransportOpened();
    void onTransportClosed();
    void onTransportError(const QString &msg);
    void onTransportApdu(const QByteArray &apdu);

private:
    void advance();
    void fail(const QString &msg);

    void doStepWaitProgMode();
    void doStepWritePhysAddress();
    void doStepCheckDescriptor();    // BB1
    void doStepConnect();
    void doStepLoadStart(uint8_t objIdx);
    void doStepLoadEnd(uint8_t objIdx);
    void doStepWriteMemory(uint16_t baseAddr, const QByteArray &block);
    void doStepVerify();
    void doStepRestart();
    void doStepWaitRestart();        // BB3
    void doStepDisconnect();
    void doStepDone();

    Step currentStep() const;

    IKnxInterface              *m_iface         = nullptr;
    DeviceInstance             *m_device        = nullptr;
    const KnxApplicationProgram *m_appProgram   = nullptr;
    TransportConnection        *m_transport     = nullptr;
    QTimer                     *m_timer         = nullptr;

    Mode             m_mode                = Mode::Full;
    QList<Step>      m_sequence;
    int              m_seqIdx              = 0;  // current position in m_sequence

    bool     m_running               = false;
    int      m_progModeTimeoutMs          = 5000;
    int      m_descriptorCheckTimeoutMs   = 3000;
    int      m_restartSettleMs            = 2000;
    int      m_restartPollIntervalMs      = 2000;
    bool     m_useLoadState          = true;
    bool     m_verifyEnabled         = true;
    int      m_progResponseCount     = 0;
    bool     m_verifyAwaiting        = false;
    QByteArray m_expectedParamBlock;
    int      m_restartPollCount      = 0;     // BB3: how many polls sent after restart
    QSet<uint8_t> m_loadingObjects;           // BB3: tracks LoadStart-but-not-LoadEnd objects
};
