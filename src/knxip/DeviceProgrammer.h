#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QList>
#include <QQueue>

class IKnxInterface;
class DeviceInstance;
class KnxApplicationProgram;
class QTimer;

// Runs the classic KNX programming sequence for a single device:
//
//   1. Wait for programming-mode (user presses prog-button)
//   2. A_IndividualAddress_Write  → sets the physical address
//   3. A_Memory_Write (chunked)   → address table
//   4. A_Memory_Write (chunked)   → association table
//   5. A_Memory_Write (chunked)   → parameter block
//   6. A_Memory_Read              → verify written parameters against expected image
//   7. A_Restart                  → device boots with new config
//
// Large memory blocks are split into chunks of at most kChunkBytes bytes to
// ensure compatibility with devices that do not support extended APDUs.
// The verify step reads back the parameter block and emits finished(false,…)
// if the content does not match. Timeout during verify is non-fatal (warning only).
//
// setProgModeTimeout() allows tests to use a shorter wait without 5-second delays.
class DeviceProgrammer : public QObject
{
    Q_OBJECT

public:
    enum Step {
        StepWaitProgMode,
        StepWritePhysAddress,
        StepWriteAddressTable,
        StepWriteAssociationTable,
        StepWriteParameters,
        StepVerifyParameters,
        StepRestart,
        StepDone,
    };
    Q_ENUM(Step)

    DeviceProgrammer(IKnxInterface               *iface,
                     DeviceInstance              *device,
                     const KnxApplicationProgram *appProgram,
                     QObject *parent = nullptr);

    void start();
    void cancel();

    // Configurable prog-mode wait timeout (default 5000 ms). Mainly for tests.
    void setProgModeTimeout(int ms) { m_progModeTimeoutMs = ms; }

    static QString stepLabel(Step s);

signals:
    void stepStarted(int step, const QString &description);
    void stepCompleted(int step);
    void progressUpdated(int percent);
    void finished(bool success, const QString &message);

private slots:
    void runNextStep();
    void onCemiReceivedForVerify(const QByteArray &cemi);

private:
    struct MemChunk {
        uint16_t   destPa;
        uint16_t   memAddr;
        QByteArray data;
    };

    // Splits data into kChunkBytes-sized chunks and enqueues them.
    // Does NOT start the timer — caller must do so after enqueue.
    void enqueueMemoryWrite(uint16_t destPa, uint16_t memBase, const QByteArray &data);
    void sendAndAdvance(const QByteArray &frame, int delayMs = 400);

    IKnxInterface              *m_iface          = nullptr;
    DeviceInstance             *m_device         = nullptr;
    const KnxApplicationProgram *m_appProgram    = nullptr;
    QTimer                     *m_timer          = nullptr;

    int    m_step               = StepWaitProgMode;
    bool   m_running            = false;
    int    m_progModeTimeoutMs  = 5000;

    QQueue<MemChunk>         m_memChunks;       // pending write chunks
    QMetaObject::Connection  m_verifyConn;      // cemiFrameReceived → onCemiReceivedForVerify
    bool                     m_verifyStarted   = false;
};
