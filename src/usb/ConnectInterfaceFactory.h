#pragma once

#include "UsbKnxInterface.h"
#include <memory>

class IKnxInterface;
class QHostAddress;

class ConnectInterfaceFactory
{
public:
    // Creates a KNXnet/IP tunneling client for the given host:port.
    static std::unique_ptr<IKnxInterface> createKnxIp(const QHostAddress &host,
                                                       quint16 port = 3671);

    // Creates a USB KNX interface.
    // transport: Serial (CDC-ACM) or HID (/dev/hidrawN)
    // devicePath: system path such as /dev/ttyACM0 or /dev/hidraw0
    static std::unique_ptr<IKnxInterface> createUsb(UsbKnxInterface::Transport transport,
                                                     const QString &devicePath);
};
