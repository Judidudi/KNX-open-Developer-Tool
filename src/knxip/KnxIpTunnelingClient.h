#pragma once

#include "IKnxInterface.h"
#include <QHostAddress>
#include <QByteArray>

class QUdpSocket;
class QTimer;

// KNXnet/IP Tunneling connection (KNX spec 03_08_02).
//
// Reliability features:
//  • TUNNEL_REQUEST is retransmitted up to 3× if no ACK arrives within 1 s.
//  • CONNECTIONSTATE heartbeat is sent every 60 s; if the response is missing
//    for 10 s the connection is declared dead and disconnected() is emitted.
//  • A 5-second timeout on the initial CONNECT_RESPONSE emits errorOccurred()
//    if the router does not answer.
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
    void onAckTimeout();
    void onConnectTimeout();
    void onHeartbeatResponseTimeout();

private:
    void sendConnectionRequest();
    void sendDisconnectRequest();
    void sendTunnelRequest(const QByteArray &cemi);
    void doRetransmit();

    void handleConnectResponse(const QByteArray &data);
    void handleTunnelAck(const QByteArray &data);
    void handleTunnelRequest(const QByteArray &data);
    void handleConnectionStateResponse(const QByteArray &data);

    QUdpSocket  *m_socket             = nullptr;
    QTimer      *m_heartbeat          = nullptr;  // 60 s periodic
    QTimer      *m_ackTimer           = nullptr;  // 1 s one-shot ACK timeout
    QTimer      *m_connectTimer       = nullptr;  // 5 s one-shot connect timeout
    QTimer      *m_heartbeatRespTimer = nullptr;  // 10 s one-shot heartbeat-response timeout

    QHostAddress m_remoteHost;
    quint16      m_remotePort   = 3671;
    quint8       m_channelId    = 0;
    quint8       m_seqCounter   = 0;    // next sequence number to send
    quint8       m_pendingSeq   = 0;    // sequence number awaiting ACK
    QByteArray   m_pendingPacket;       // full TUNNEL_REQUEST awaiting ACK (for retransmit)
    int          m_retryCount   = 0;
    bool         m_connected    = false;
};
