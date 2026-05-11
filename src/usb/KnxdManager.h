#pragma once

#include <QObject>
#include <QHostAddress>
#include <QProcess>
#include <QString>

// Manages a knxd subprocess used as a KNXnet/IP tunneling backend for USB
// KNX interfaces.  After start() succeeds, our existing KnxIpTunnelingClient
// connects to localhost:3671 and all USB protocol details are handled by knxd.
class KnxdManager : public QObject
{
    Q_OBJECT

public:
    explicit KnxdManager(QObject *parent = nullptr);
    ~KnxdManager() override;

    // Returns true when the "knxd" binary is found on the system.
    static bool    isInstalled();
    static QString binaryPath();   // full path, or empty if not found

    // Start knxd for the given USB HID device path (e.g. /dev/hidraw0).
    // Emits ready() after ~2 s if the process is still running.
    // Returns false immediately when knxd is not installed or fails to start.
    bool start(const QString &usbDevPath);

    // Terminate the knxd subprocess (if running).
    void stop();

    bool isRunning() const;

    QHostAddress host() const { return QHostAddress::LocalHost; }
    quint16      port() const { return m_port; }

signals:
    void ready();                       // knxd is up, connect your client
    void stopped();                     // process exited (intentional or crash)
    void errorOccurred(const QString &msg);
    void udevSetupNeeded();             // libusb permission denied — install udev rule

private:
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

    QProcess *m_process    = nullptr;
    QString   m_configPath;             // temp INI config file path
    quint16   m_port       = 3671;
};
