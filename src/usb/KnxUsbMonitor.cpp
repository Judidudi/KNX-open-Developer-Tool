#include "KnxUsbMonitor.h"

#include <QTimer>
#include <QDir>
#include <QFile>
#include <QSet>

#include <unistd.h>  // ::access

// Known KNX USB HID vendor IDs.
// Insta GmbH makes the reference design used by Hager, Jung, Gira, Merten,
// Berker and Busch-Jaeger. The others are Siemens, MCS Electronics, and Atmel.
static const QSet<quint16> kKnxVendorIds = {
    0x135e,  // Insta GmbH (Hager, Jung, Gira, Merten, Berker, Busch-Jaeger)
    0x0681,  // Siemens Building Technologies
    0x16d0,  // MCS Electronics (various KNX USB sticks)
    0x03eb,  // Atmel (KNX development boards)
};

KnxUsbMonitor::KnxUsbMonitor(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &KnxUsbMonitor::poll);
}

void KnxUsbMonitor::start()
{
    poll();         // immediate first scan
    m_timer->start();
}

void KnxUsbMonitor::stop()
{
    m_timer->stop();
}

std::optional<KnxUsbMonitor::DeviceInfo>
KnxUsbMonitor::readSysfs(const QString &hidrawName)
{
    const QString ueventPath =
        QStringLiteral("/sys/class/hidraw/%1/device/uevent").arg(hidrawName);
    QFile f(ueventPath);
    if (!f.open(QIODevice::ReadOnly))
        return {};

    DeviceInfo info;
    info.path = QStringLiteral("/dev/") + hidrawName;

    for (const QByteArray &rawLine : f.readAll().split('\n')) {
        const QByteArray line = rawLine.trimmed();
        if (line.startsWith("HID_ID=")) {
            // Format: HID_ID=0003:0000135E:00000025
            const auto parts = QString::fromLatin1(line.mid(7)).split(u':');
            if (parts.size() >= 3) {
                info.vendorId  = parts[1].toUShort(nullptr, 16);
                info.productId = parts[2].toUShort(nullptr, 16);
            }
        } else if (line.startsWith("HID_NAME=")) {
            info.name = QString::fromLatin1(line.mid(9)).trimmed();
        }
    }

    if (info.vendorId == 0)
        return {};

    info.accessible = (::access(info.path.toLocal8Bit().constData(),
                                R_OK | W_OK) == 0);
    return info;
}

QList<KnxUsbMonitor::DeviceInfo> KnxUsbMonitor::scanDevices()
{
    QList<DeviceInfo> result;
    const QDir sysDir(QStringLiteral("/sys/class/hidraw"));
    for (const QString &entry :
         sysDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        auto info = readSysfs(entry);
        if (info && kKnxVendorIds.contains(info->vendorId))
            result.append(*info);
    }
    return result;
}

void KnxUsbMonitor::poll()
{
    const QList<DeviceInfo> current = scanDevices();

    // Emit deviceRemoved for anything that is gone
    for (const DeviceInfo &old : m_devices) {
        const bool stillPresent = std::any_of(
            current.begin(), current.end(),
            [&](const DeviceInfo &d) { return d.path == old.path; });
        if (!stillPresent)
            emit deviceRemoved(old.path);
    }

    // Emit deviceFound for new devices or accessibility changes
    for (const DeviceInfo &d : current) {
        const auto it = std::find_if(
            m_devices.begin(), m_devices.end(),
            [&](const DeviceInfo &o) { return o.path == d.path; });
        if (it == m_devices.end()) {
            emit deviceFound(d);
        } else if (it->accessible != d.accessible) {
            // Re-emit so MainWindow can trigger auto-connect after udev rule install
            emit deviceFound(d);
        }
    }

    m_devices = current;
}
