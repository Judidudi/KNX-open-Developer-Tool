#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QList>

class IKnxInterface;
class DeviceInstance;
struct Manifest;
class QTimer;

// Runs the classic KNX programming sequence for a single device:
//
//   1. Wait for programming-mode (user presses prog-button on the device)
//   2. A_IndividualAddress_Write  → sets the physical address
//   3. A_Memory_Write             → address table (PA + GAs)
//   4. A_Memory_Write             → association table (ComObject ↔ GA)
//   5. A_Memory_Write             → parameter block
//   6. A_Restart                  → device boots with new config
//
// The programmer emits a signal per step so the UI can show progress.
// Between steps a short delay is inserted to give slow targets time to ACK.
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
        StepRestart,
        StepDone,
    };
    Q_ENUM(Step)

    DeviceProgrammer(IKnxInterface *iface,
                     DeviceInstance *device,
                     const Manifest *manifest,
                     QObject *parent = nullptr);

    // Starts the programming sequence. Non-blocking; result delivered via signals.
    void start();
    void cancel();

    static QString stepLabel(Step s);

signals:
    void stepStarted(int step, const QString &description);
    void stepCompleted(int step);
    void progressUpdated(int percent);
    void finished(bool success, const QString &message);

private slots:
    void runNextStep();

private:
    void sendAndAdvance(const QByteArray &frame, int delayMs = 400);

    IKnxInterface  *m_iface    = nullptr;
    DeviceInstance *m_device   = nullptr;
    const Manifest *m_manifest = nullptr;
    QTimer         *m_timer    = nullptr;

    int   m_step    = StepWaitProgMode;
    bool  m_running = false;
};
