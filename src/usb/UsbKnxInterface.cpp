#include "UsbKnxInterface.h"

// USB KNX interface – stub implementation.
// Full implementation (KNX USB HID protocol, CEMI encapsulation) will be
// added in Phase 6. The stub allows the rest of the application to compile
// and link against IKnxInterface without hardware dependency.

UsbKnxInterface::UsbKnxInterface(QObject *parent)
    : IKnxInterface(parent)
{}

UsbKnxInterface::~UsbKnxInterface()
{
    disconnectFromInterface();
}

bool UsbKnxInterface::connectToInterface()
{
    emit errorOccurred(tr("USB KNX Interface: Not yet implemented."));
    return false;
}

void UsbKnxInterface::disconnectFromInterface()
{
    if (m_connected) {
        m_connected = false;
        emit disconnected();
    }
}

bool UsbKnxInterface::isConnected() const
{
    return m_connected;
}

void UsbKnxInterface::sendCemiFrame(const QByteArray &)
{
    emit errorOccurred(tr("USB KNX Interface: Not yet implemented."));
}
