#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <optional>

class QTimer;

// Polls /sys/class/hidraw/ every second to detect KNX USB HID interfaces.
// Does not require root or open any /dev node — only reads sysfs uevent files,
// which are world-readable. Emits deviceFound/deviceRemoved as devices appear
// and disappear. The accessible flag reflects whether /dev/hidrawN is currently
// openable without elevated privileges.
class KnxUsbMonitor : public QObject
{
    Q_OBJECT

public:
    struct DeviceInfo {
        QString  path;        // e.g. /dev/hidraw0
        quint16  vendorId;    // e.g. 0x135e
        quint16  productId;   // e.g. 0x0025
        QString  name;        // e.g. "Hager Electro KNX-USB Data Interface"
        bool     accessible;  // ::access(path, R_OK|W_OK) == 0
    };

    explicit KnxUsbMonitor(QObject *parent = nullptr);

    void start();
    void stop();

    QList<DeviceInfo> devices() const { return m_devices; }

signals:
    void deviceFound(KnxUsbMonitor::DeviceInfo info);
    void deviceRemoved(QString path);

private slots:
    void poll();

private:
    static QList<DeviceInfo>          scanDevices();
    static std::optional<DeviceInfo>  readSysfs(const QString &hidrawName);

    QTimer            *m_timer   = nullptr;
    QList<DeviceInfo>  m_devices;
};
