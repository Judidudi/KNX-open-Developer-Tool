#pragma once

#include <QObject>
#include <memory>

class IKnxInterface;

// Application-wide holder for the active KNX bus interface.
// The MainWindow creates one and hands a pointer to any widget that needs to
// send or receive CEMI frames (BusMonitor, DeviceProgrammer, ...).
//
// Exactly one interface is active at a time. Connecting a new interface
// transparently disconnects the previous one.
class InterfaceManager : public QObject
{
    Q_OBJECT

public:
    explicit InterfaceManager(QObject *parent = nullptr);
    ~InterfaceManager() override;

    // Takes ownership. If another interface is currently active it is
    // disconnected and destroyed first.
    void           setInterface(std::unique_ptr<IKnxInterface> iface);
    IKnxInterface *activeInterface() const { return m_iface.get(); }

    bool isConnected() const;

signals:
    // Forwarded from the active interface so subscribers never have to
    // re-bind when the underlying transport changes.
    void connected();
    void disconnected();
    void cemiFrameReceived(const QByteArray &cemi);
    void errorOccurred(const QString &message);

private:
    std::unique_ptr<IKnxInterface> m_iface;
};
