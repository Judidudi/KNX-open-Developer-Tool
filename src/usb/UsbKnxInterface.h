#pragma once

#include "IKnxInterface.h"
#include <QString>
#include <QByteArray>
#include <memory>

// KNX USB interface implementing IKnxInterface.
//
// Transport::Serial — cEMI framed over USB CDC-ACM (QSerialPort).
//   Frame format: [0xAB][len_hi][len_lo][cemi_bytes…]
//   Requires Qt6::SerialPort. Disabled (link-time) if KNXODT_NO_SERIAL is set.
//
// Transport::HID — KNX USB HID protocol (spec 07_01_01).
//   64-byte reports via Linux /dev/hidrawN (POSIX I/O + QSocketNotifier).
//   Only available on Linux (Q_OS_LINUX).
class UsbKnxInterface : public IKnxInterface
{
    Q_OBJECT
public:
    enum class Transport { Serial, HID };

    explicit UsbKnxInterface(Transport transport = Transport::Serial,
                              const QString &devicePath = {},
                              QObject *parent = nullptr);
    ~UsbKnxInterface() override;

    void setDevicePath(const QString &path);
    void setTransport(Transport t);
    [[nodiscard]] Transport transport() const;
    [[nodiscard]] QString   devicePath() const;

    bool connectToInterface() override;
    void disconnectFromInterface() override;
    [[nodiscard]] bool isConnected() const override;
    void sendCemiFrame(const QByteArray &cemi) override;

    // Returns available serial port paths (e.g. /dev/ttyACM0) on the system.
    static QStringList availableSerialPorts();
    // Returns available HID device paths (e.g. /dev/hidraw0) on Linux.
    static QStringList availableHidDevices();

private:
    struct Priv;
    std::unique_ptr<Priv> d;
};
