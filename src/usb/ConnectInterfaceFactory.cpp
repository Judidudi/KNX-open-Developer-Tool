#include "ConnectInterfaceFactory.h"
#include "KnxdManager.h"
#include "KnxIpTunnelingClient.h"
#include <QHostAddress>

std::unique_ptr<IKnxInterface> ConnectInterfaceFactory::createKnxIp(
    const QHostAddress &host, quint16 port)
{
    auto client = std::make_unique<KnxIpTunnelingClient>();
    client->setRemote(host, port);
    return client;
}

std::unique_ptr<IKnxInterface> ConnectInterfaceFactory::createUsb(
    UsbKnxInterface::Transport transport, const QString &devicePath)
{
    return std::make_unique<UsbKnxInterface>(transport, devicePath);
}

std::unique_ptr<IKnxInterface> ConnectInterfaceFactory::createViaKnxd(KnxdManager *mgr)
{
    auto client = std::make_unique<KnxIpTunnelingClient>();
    client->setRemote(mgr->host(), mgr->port());
    return client;
}
