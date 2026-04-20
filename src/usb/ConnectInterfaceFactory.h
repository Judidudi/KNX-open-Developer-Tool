#pragma once

#include <memory>

class IKnxInterface;
class QHostAddress;

class ConnectInterfaceFactory
{
public:
    static std::unique_ptr<IKnxInterface> createKnxIp(const QHostAddress &host,
                                                       quint16 port = 3671);
    static std::unique_ptr<IKnxInterface> createUsb();
};
