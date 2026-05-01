#pragma once

#include <QObject>
#include <QList>
#include <QString>

class IKnxInterface;
class QTimer;

// Probes individual KNX addresses on a given area/line by sending
// A_DeviceDescriptor_Read and watching for A_DeviceDescriptor_Response.
// Results are delivered via signals; the scan is non-blocking.
class LineScanRunner : public QObject
{
    Q_OBJECT

public:
    explicit LineScanRunner(IKnxInterface *iface, QObject *parent = nullptr);

    // Start scanning area.line, probing device numbers 1..maxDevice.
    // timeoutMs: how long to wait per address before moving on (default 200 ms).
    void startScan(int area, int line, int maxDevice = 64, int timeoutMs = 200);
    void cancel();

    bool isRunning() const { return m_running; }

signals:
    void deviceFound(const QString &physAddr);
    void progress(int current, int total);
    void finished();

private slots:
    void onCemiReceived(const QByteArray &cemi);
    void onProbeTimeout();

private:
    void probeNext();

    IKnxInterface *m_iface      = nullptr;
    QTimer        *m_timer      = nullptr;

    int  m_area       = 1;
    int  m_line       = 1;
    int  m_current    = 1;   // device number currently being probed
    int  m_maxDevice  = 64;
    int  m_timeoutMs  = 200;
    bool m_running    = false;
    bool m_gotResponse = false;
};
