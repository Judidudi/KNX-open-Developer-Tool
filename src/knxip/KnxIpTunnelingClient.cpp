#include "KnxIpTunnelingClient.h"
#include "CemiFrame.h"

#include <QUdpSocket>
#include <QTimer>
#include <QNetworkDatagram>

// KNXnet/IP service type codes (KNX spec 03_08_02)
static constexpr uint16_t SVC_CONNECT_REQUEST     = 0x0205;
static constexpr uint16_t SVC_CONNECT_RESPONSE    = 0x0206;
static constexpr uint16_t SVC_DISCONNECT_REQUEST  = 0x0209;
static constexpr uint16_t SVC_TUNNEL_REQUEST      = 0x0420;
static constexpr uint16_t SVC_TUNNEL_ACK          = 0x0421;
static constexpr uint16_t SVC_CONNECTIONSTATE_REQUEST = 0x0207;

static QByteArray knxipHeader(uint16_t svc, uint16_t bodyLen)
{
    uint16_t total = 6 + bodyLen;
    QByteArray h;
    h.append(char(0x06)); h.append(char(0x10));
    h.append(static_cast<char>(svc >> 8));
    h.append(static_cast<char>(svc & 0xFF));
    h.append(static_cast<char>(total >> 8));
    h.append(static_cast<char>(total & 0xFF));
    return h;
}

KnxIpTunnelingClient::KnxIpTunnelingClient(QObject *parent)
    : IKnxInterface(parent)
    , m_socket(new QUdpSocket(this))
    , m_heartbeat(new QTimer(this))
{
    connect(m_socket, &QUdpSocket::readyRead, this, &KnxIpTunnelingClient::onReadyRead);
    m_heartbeat->setInterval(60000);
    connect(m_heartbeat, &QTimer::timeout, this, &KnxIpTunnelingClient::onHeartbeatTimeout);
}

KnxIpTunnelingClient::~KnxIpTunnelingClient()
{
    disconnectFromInterface();
}

void KnxIpTunnelingClient::setRemote(const QHostAddress &host, quint16 port)
{
    m_remoteHost = host;
    m_remotePort = port;
}

bool KnxIpTunnelingClient::connectToInterface()
{
    if (m_connected)
        return true;
    m_socket->bind(QHostAddress::AnyIPv4, 0);
    sendConnectionRequest();
    return true;
}

void KnxIpTunnelingClient::disconnectFromInterface()
{
    if (!m_connected)
        return;
    m_heartbeat->stop();
    sendDisconnectRequest();
    m_connected = false;
    m_socket->close();
    emit disconnected();
}

bool KnxIpTunnelingClient::isConnected() const
{
    return m_connected;
}

void KnxIpTunnelingClient::sendCemiFrame(const QByteArray &cemi)
{
    if (m_connected)
        sendTunnelRequest(cemi);
}

void KnxIpTunnelingClient::sendConnectionRequest()
{
    // CONNECT_REQUEST body: 2x HPAI (data endpoint + control endpoint)
    QByteArray hpai;
    hpai.append(char(0x08)); hpai.append(char(0x01)); // UDP
    hpai.append(char(0)); hpai.append(char(0));        // IP 0.0.0.0
    hpai.append(char(0)); hpai.append(char(0));
    hpai.append(static_cast<char>(m_socket->localPort() >> 8));
    hpai.append(static_cast<char>(m_socket->localPort() & 0xFF));

    QByteArray cri;
    cri.append(char(0x04)); cri.append(char(0x04)); // Tunneling connection
    cri.append(char(0x02)); cri.append(char(0x00)); // TUNNEL_LINKLAYER

    QByteArray body = hpai + hpai + cri;
    QByteArray pkt  = knxipHeader(SVC_CONNECT_REQUEST, body.size()) + body;
    m_socket->writeDatagram(pkt, m_remoteHost, m_remotePort);
}

void KnxIpTunnelingClient::sendDisconnectRequest()
{
    QByteArray body;
    body.append(static_cast<char>(m_channelId));
    body.append(char(0)); // reserved
    // Control endpoint HPAI
    body.append(char(0x08)); body.append(char(0x01));
    body.append(char(0)); body.append(char(0));
    body.append(char(0)); body.append(char(0));
    body.append(static_cast<char>(m_socket->localPort() >> 8));
    body.append(static_cast<char>(m_socket->localPort() & 0xFF));

    QByteArray pkt = knxipHeader(SVC_DISCONNECT_REQUEST, body.size()) + body;
    m_socket->writeDatagram(pkt, m_remoteHost, m_remotePort);
}

void KnxIpTunnelingClient::sendTunnelRequest(const QByteArray &cemi)
{
    QByteArray body;
    body.append(char(0x04));                               // structure length
    body.append(static_cast<char>(m_channelId));
    body.append(static_cast<char>(m_seqCounter++));
    body.append(char(0x00));                               // reserved
    body.append(cemi);

    QByteArray pkt = knxipHeader(SVC_TUNNEL_REQUEST, body.size()) + body;
    m_socket->writeDatagram(pkt, m_remoteHost, m_remotePort);
}

void KnxIpTunnelingClient::handleConnectResponse(const QByteArray &data)
{
    if (data.size() < 8)
        return;
    uint8_t status = static_cast<uint8_t>(data[7]);
    if (status != 0x00) {
        emit errorOccurred(QStringLiteral("CONNECT_RESPONSE error: 0x%1").arg(status, 2, 16, QLatin1Char('0')));
        return;
    }
    m_channelId = static_cast<uint8_t>(data[6]);
    m_connected = true;
    m_heartbeat->start();
    emit connected();
}

void KnxIpTunnelingClient::handleTunnelRequest(const QByteArray &data)
{
    if (data.size() < 10)
        return;
    // Send ACK
    QByteArray ackBody;
    ackBody.append(char(0x04));
    ackBody.append(static_cast<char>(m_channelId));
    ackBody.append(data[8]); // sequence counter from request
    ackBody.append(char(0x00));
    QByteArray pkt = knxipHeader(SVC_TUNNEL_ACK, ackBody.size()) + ackBody;
    m_socket->writeDatagram(pkt, m_remoteHost, m_remotePort);

    // Forward CEMI payload
    QByteArray cemi = data.mid(10);
    if (!cemi.isEmpty())
        emit cemiFrameReceived(cemi);
}

void KnxIpTunnelingClient::handleTunnelAck(const QByteArray &)
{
    // Nothing to do for now – retransmit logic can be added later
}

void KnxIpTunnelingClient::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram dg = m_socket->receiveDatagram();
        QByteArray data = dg.data();
        if (data.size() < 6)
            continue;
        uint16_t svc = (static_cast<uint8_t>(data[2]) << 8) | static_cast<uint8_t>(data[3]);
        QByteArray payload = data.mid(6);
        switch (svc) {
        case SVC_CONNECT_RESPONSE:  handleConnectResponse(payload); break;
        case SVC_TUNNEL_REQUEST:    handleTunnelRequest(payload);   break;
        case SVC_TUNNEL_ACK:        handleTunnelAck(payload);       break;
        default: break;
        }
    }
}

void KnxIpTunnelingClient::onHeartbeatTimeout()
{
    // CONNECTIONSTATE_REQUEST
    QByteArray body;
    body.append(static_cast<char>(m_channelId));
    body.append(char(0));
    body.append(char(0x08)); body.append(char(0x01));
    body.append(char(0)); body.append(char(0));
    body.append(char(0)); body.append(char(0));
    body.append(static_cast<char>(m_socket->localPort() >> 8));
    body.append(static_cast<char>(m_socket->localPort() & 0xFF));
    QByteArray pkt = knxipHeader(SVC_CONNECTIONSTATE_REQUEST, body.size()) + body;
    m_socket->writeDatagram(pkt, m_remoteHost, m_remotePort);
}
