#pragma once

#include "IKnxInterface.h"
#include <QHostAddress>

class QUdpSocket;
class QTimer;

class KnxIpTunnelingClient : public IKnxInterface
{
    Q_OBJECT
public:
    explicit KnxIpTunnelingClient(QObject *parent = nullptr);
    ~KnxIpTunnelingClient() override;

    void setRemote(const QHostAddress &host, quint16 port = 3671);

    bool connectToInterface() override;
    void disconnectFromInterface() override;
    [[nodiscard]] bool isConnected() const override;
    void sendCemiFrame(const QByteArray &cemi) override;

private slots:
    void onReadyRead();
    void onHeartbeatTimeout();

private:
    void sendConnectionRequest();
    void sendDisconnectRequest();
    void sendTunnelRequest(const QByteArray &cemi);
    void handleConnectResponse(const QByteArray &data);
    void handleTunnelAck(const QByteArray &data);
    void handleTunnelRequest(const QByteArray &data);

    QUdpSocket  *m_socket   = nullptr;
    QTimer      *m_heartbeat = nullptr;
    QHostAddress m_remoteHost;
    quint16      m_remotePort   = 3671;
    quint8       m_channelId    = 0;
    quint8       m_seqCounter   = 0;
    bool         m_connected    = false;
};
