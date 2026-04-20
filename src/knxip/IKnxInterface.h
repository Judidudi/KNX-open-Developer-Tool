#pragma once

#include <QObject>
#include <QByteArray>

// Abstract interface implemented by KnxIpTunnelingClient and UsbKnxInterface.
// Both bus access paths expose the same signals/slots, allowing UI code to be
// independent of the physical transport layer.
class IKnxInterface : public QObject
{
    Q_OBJECT
public:
    explicit IKnxInterface(QObject *parent = nullptr) : QObject(parent) {}
    ~IKnxInterface() override = default;

    virtual bool connectToInterface() = 0;
    virtual void disconnectFromInterface() = 0;
    [[nodiscard]] virtual bool isConnected() const = 0;
    virtual void sendCemiFrame(const QByteArray &cemi) = 0;

signals:
    void connected();
    void disconnected();
    void cemiFrameReceived(const QByteArray &cemi);
    void errorOccurred(const QString &message);
};
