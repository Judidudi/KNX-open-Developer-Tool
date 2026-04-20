#pragma once

#include "IKnxInterface.h"

// KNX USB interface – implements IKnxInterface via the KNX USB HID protocol
// (KNX specification 07_01_01 "KNX USB Transfer Protocol").
// Requires Qt6::SerialPort or libhidapi (optional, compile-time flag).
class UsbKnxInterface : public IKnxInterface
{
    Q_OBJECT
public:
    explicit UsbKnxInterface(QObject *parent = nullptr);
    ~UsbKnxInterface() override;

    bool connectToInterface() override;
    void disconnectFromInterface() override;
    [[nodiscard]] bool isConnected() const override;
    void sendCemiFrame(const QByteArray &cemi) override;

private:
    bool m_connected = false;
};
