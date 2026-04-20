#include "ConnectInterfaceFactory.h"
#include "UsbKnxInterface.h"
#include "KnxIpTunnelingClient.h"

#include <QHostAddress>

std::unique_ptr<IKnxInterface> ConnectInterfaceFactory::createKnxIp(
    const QHostAddress &host, quint16 port)
{
    auto client = std::make_unique<KnxIpTunnelingClient>();
    client->setRemote(host, port);
    return client;
}

std::unique_ptr<IKnxInterface> ConnectInterfaceFactory::createUsb()
{
    return std::make_unique<UsbKnxInterface>();
}
