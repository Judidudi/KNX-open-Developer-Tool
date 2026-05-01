#include "KnxIpTunnelingClient.h"
#include "CemiFrame.h"

#include <QUdpSocket>
#include <QTimer>
#include <QNetworkDatagram>

// KNXnet/IP service type codes (KNX spec 03_08_02)
static constexpr uint16_t SVC_CONNECT_REQUEST          = 0x0205;
static constexpr uint16_t SVC_CONNECT_RESPONSE         = 0x0206;
static constexpr uint16_t SVC_CONNECTIONSTATE_REQUEST  = 0x0207;
static constexpr uint16_t SVC_CONNECTIONSTATE_RESPONSE = 0x0208;
static constexpr uint16_t SVC_DISCONNECT_REQUEST       = 0x0209;
static constexpr uint16_t SVC_TUNNEL_REQUEST           = 0x0420;
static constexpr uint16_t SVC_TUNNEL_ACK               = 0x0421;

static constexpr int kAckTimeoutMs           = 1000;
static constexpr int kConnectTimeoutMs       = 5000;
static constexpr int kHeartbeatIntervalMs    = 60000;
static constexpr int kHeartbeatResponseMs    = 10000;
static constexpr int kMaxRetries             = 3;

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

// Build the 8-byte HPAI for the local UDP socket (NAT-friendly: 0.0.0.0:localPort)
static QByteArray localHpai(quint16 port)
{
    QByteArray hpai;
    hpai.append(char(0x08)); hpai.append(char(0x01)); // length + UDP
    hpai.append(char(0)); hpai.append(char(0));        // IP 0.0.0.0
    hpai.append(char(0)); hpai.append(char(0));
    hpai.append(static_cast<char>(port >> 8));
    hpai.append(static_cast<char>(port & 0xFF));
    return hpai;
}

// ─────────────────────────────────────────────────────────────────────────────

KnxIpTunnelingClient::KnxIpTunnelingClient(QObject *parent)
    : IKnxInterface(parent)
    , m_socket(new QUdpSocket(this))
    , m_heartbeat(new QTimer(this))
    , m_ackTimer(new QTimer(this))
    , m_connectTimer(new QTimer(this))
    , m_heartbeatRespTimer(new QTimer(this))
{
    connect(m_socket, &QUdpSocket::readyRead, this, &KnxIpTunnelingClient::onReadyRead);

    m_heartbeat->setInterval(kHeartbeatIntervalMs);
    connect(m_heartbeat, &QTimer::timeout, this, &KnxIpTunnelingClient::onHeartbeatTimeout);

    m_ackTimer->setSingleShot(true);
    m_ackTimer->setInterval(kAckTimeoutMs);
    connect(m_ackTimer, &QTimer::timeout, this, &KnxIpTunnelingClient::onAckTimeout);

    m_connectTimer->setSingleShot(true);
    m_connectTimer->setInterval(kConnectTimeoutMs);
    connect(m_connectTimer, &QTimer::timeout, this, &KnxIpTunnelingClient::onConnectTimeout);

    m_heartbeatRespTimer->setSingleShot(true);
    m_heartbeatRespTimer->setInterval(kHeartbeatResponseMs);
    connect(m_heartbeatRespTimer, &QTimer::timeout,
            this, &KnxIpTunnelingClient::onHeartbeatResponseTimeout);
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
    if (!m_socket->bind(QHostAddress::AnyIPv4, 0))
        return false;
    sendConnectionRequest();
    m_connectTimer->start();
    return true;
}

void KnxIpTunnelingClient::disconnectFromInterface()
{
    if (!m_connected && m_pendingPacket.isEmpty()) {
        m_connectTimer->stop();
        m_socket->close();
        return;
    }
    m_heartbeat->stop();
    m_ackTimer->stop();
    m_connectTimer->stop();
    m_heartbeatRespTimer->stop();
    m_pendingPacket.clear();
    if (m_connected) {
        sendDisconnectRequest();
        m_connected = false;
        m_socket->close();
        emit disconnected();
    }
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

// ─── Private send helpers ─────────────────────────────────────────────────────

void KnxIpTunnelingClient::sendConnectionRequest()
{
    const QByteArray hpai = localHpai(m_socket->localPort());
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
    body.append(localHpai(m_socket->localPort()));
    QByteArray pkt = knxipHeader(SVC_DISCONNECT_REQUEST, body.size()) + body;
    m_socket->writeDatagram(pkt, m_remoteHost, m_remotePort);
}

void KnxIpTunnelingClient::sendTunnelRequest(const QByteArray &cemi)
{
    QByteArray body;
    body.append(char(0x04));
    body.append(static_cast<char>(m_channelId));
    body.append(static_cast<char>(m_seqCounter)); // don't increment yet
    body.append(char(0x00));
    body.append(cemi);

    m_pendingSeq    = m_seqCounter;
    m_pendingPacket = knxipHeader(SVC_TUNNEL_REQUEST, body.size()) + body;
    m_retryCount    = 0;
    doRetransmit();
}

void KnxIpTunnelingClient::doRetransmit()
{
    m_socket->writeDatagram(m_pendingPacket, m_remoteHost, m_remotePort);
    m_ackTimer->start();
}

// ─── Incoming message handlers ────────────────────────────────────────────────

void KnxIpTunnelingClient::handleConnectResponse(const QByteArray &data)
{
    m_connectTimer->stop();
    if (data.size() < 8) {
        emit errorOccurred(tr("CONNECT_RESPONSE zu kurz"));
        return;
    }
    const uint8_t status = static_cast<uint8_t>(data[7]);
    if (status != 0x00) {
        emit errorOccurred(tr("CONNECT_RESPONSE Fehler: 0x%1").arg(status, 2, 16, QLatin1Char('0')));
        return;
    }
    m_channelId = static_cast<uint8_t>(data[6]);
    m_connected = true;
    m_seqCounter = 0;
    m_heartbeat->start();
    emit connected();
}

void KnxIpTunnelingClient::handleTunnelAck(const QByteArray &data)
{
    if (data.size() < 4)
        return;
    const quint8 ackSeq = static_cast<quint8>(data[2]);
    if (ackSeq != m_pendingSeq)
        return; // stale or duplicate ACK — ignore

    m_ackTimer->stop();
    m_pendingPacket.clear();
    ++m_seqCounter; // advance only after confirmed ACK
}

void KnxIpTunnelingClient::handleTunnelRequest(const QByteArray &data)
{
    if (data.size() < 10)
        return;
    // Send ACK back to server
    QByteArray ackBody;
    ackBody.append(char(0x04));
    ackBody.append(static_cast<char>(m_channelId));
    ackBody.append(data[8]); // echo server's sequence counter
    ackBody.append(char(0x00));
    m_socket->writeDatagram(knxipHeader(SVC_TUNNEL_ACK, ackBody.size()) + ackBody,
                            m_remoteHost, m_remotePort);

    const QByteArray cemi = data.mid(10);
    if (!cemi.isEmpty())
        emit cemiFrameReceived(cemi);
}

void KnxIpTunnelingClient::handleConnectionStateResponse(const QByteArray &data)
{
    m_heartbeatRespTimer->stop();
    if (data.size() < 2)
        return;
    const uint8_t status = static_cast<uint8_t>(data[1]);
    if (status != 0x00) {
        emit errorOccurred(tr("CONNECTIONSTATE_RESPONSE Fehler: 0x%1 – Verbindung getrennt")
                           .arg(status, 2, 16, QLatin1Char('0')));
        disconnectFromInterface();
    }
}

// ─── Slot: incoming UDP datagrams ─────────────────────────────────────────────

void KnxIpTunnelingClient::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        const QByteArray data = m_socket->receiveDatagram().data();
        if (data.size() < 6)
            continue;
        const uint16_t svc     = (static_cast<uint8_t>(data[2]) << 8)
                                 | static_cast<uint8_t>(data[3]);
        const QByteArray payload = data.mid(6);
        switch (svc) {
        case SVC_CONNECT_RESPONSE:          handleConnectResponse(payload);          break;
        case SVC_TUNNEL_REQUEST:            handleTunnelRequest(payload);            break;
        case SVC_TUNNEL_ACK:                handleTunnelAck(payload);                break;
        case SVC_CONNECTIONSTATE_RESPONSE:  handleConnectionStateResponse(payload);  break;
        default: break;
        }
    }
}

// ─── Slots: timer callbacks ───────────────────────────────────────────────────

void KnxIpTunnelingClient::onAckTimeout()
{
    if (m_pendingPacket.isEmpty())
        return;
    ++m_retryCount;
    if (m_retryCount > kMaxRetries) {
        m_pendingPacket.clear();
        emit errorOccurred(tr("Tunneling ACK nach %1 Versuchen ausgeblieben – Verbindung getrennt")
                           .arg(kMaxRetries));
        disconnectFromInterface();
        return;
    }
    doRetransmit();
}

void KnxIpTunnelingClient::onConnectTimeout()
{
    if (m_connected)
        return;
    m_socket->close();
    emit errorOccurred(tr("KNXnet/IP Verbindungsaufbau Timeout (%1 s) – kein CONNECT_RESPONSE")
                       .arg(kConnectTimeoutMs / 1000));
}

void KnxIpTunnelingClient::onHeartbeatTimeout()
{
    QByteArray body;
    body.append(static_cast<char>(m_channelId));
    body.append(char(0));
    body.append(localHpai(m_socket->localPort()));
    m_socket->writeDatagram(knxipHeader(SVC_CONNECTIONSTATE_REQUEST, body.size()) + body,
                            m_remoteHost, m_remotePort);
    m_heartbeatRespTimer->start();
}

void KnxIpTunnelingClient::onHeartbeatResponseTimeout()
{
    emit errorOccurred(tr("KNXnet/IP Heartbeat ohne Antwort – Verbindung getrennt"));
    disconnectFromInterface();
}
