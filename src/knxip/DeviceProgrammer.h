#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QQueue>

class IKnxInterface;
class DeviceInstance;
class KnxApplicationProgram;
class TransportConnection;
class QTimer;

// ETS-compatible programming sequence (KNX spec 03_05_01 + 03_03_07).
//
//   1. Detect a device in programming mode    (A_IndividualAddress_Read)
//   2. Set the new physical address            (A_IndividualAddress_Write, broadcast)
//   3. Open transport connection               (T_Connect to new PA)
//   4. For each Application Object (0/1/2):
//        a. PropertyValue_Write LoadState=Loading  (PID 5, value 1)
//        b. A_Memory_Write chunks (12 bytes/frame, sequence 0..15)
//        c. PropertyValue_Write LoadState=Loaded   (PID 5, value 2)
//   5. Verify parameter block                  (A_Memory_Read + compare)
//   6. A_Restart                               (still inside transport connection)
//   7. T_Disconnect
//
// Each connection-oriented frame is sent through TransportConnection which
// handles T_Data_Connected sequence numbers (0..15 wrapping), T_ACK validation,
// and retransmits on T_NAK or ACK-timeout.
//
// setLoadStateMachineEnabled(false) — skip step 4a/4c for legacy firmware
// that does not implement the Application Layer Property service.
class DeviceProgrammer : public QObject
{
    Q_OBJECT

public:
    enum Step {
        StepWaitProgMode,
        StepWritePhysAddress,
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

    // Tunables
    void setProgModeTimeout(int ms)         { m_progModeTimeoutMs = ms; }
    void setLoadStateMachineEnabled(bool e) { m_useLoadState = e; }
    void setVerifyEnabled(bool e)           { m_verifyEnabled = e; }
    // Underlying transport ACK timeout (default 3000 ms per KNX spec)
    void setTransportAckTimeoutMs(int ms);

    static QString stepLabel(Step s);

signals:
    void stepStarted(int step, const QString &description);
    void stepCompleted(int step);
    void progressUpdated(int percent);
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
    void doStepConnect();
    void doStepLoadStart(uint8_t objIdx, Step ownStep);
    void doStepLoadEnd(uint8_t objIdx, Step ownStep);
    void doStepWriteMemory(uint16_t baseAddr, const QByteArray &block, Step ownStep);
    void doStepVerify();
    void doStepRestart();
    void doStepDisconnect();
    void doStepDone();

    IKnxInterface              *m_iface         = nullptr;
    DeviceInstance             *m_device        = nullptr;
    const KnxApplicationProgram *m_appProgram   = nullptr;
    TransportConnection        *m_transport     = nullptr;
    QTimer                     *m_timer         = nullptr;

    int      m_step                  = StepWaitProgMode;
    bool     m_running               = false;
    int      m_progModeTimeoutMs     = 5000;
    bool     m_useLoadState          = true;
    bool     m_verifyEnabled         = true;
    int      m_progResponseCount     = 0;
    bool     m_verifyAwaiting        = false;
    QByteArray m_expectedParamBlock;
};
