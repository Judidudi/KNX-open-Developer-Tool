#include "KnxIpDiscovery.h"
#include <QUdpSocket>
#include <QTimer>
#include <QNetworkDatagram>
#include <QHostAddress>

static constexpr quint16 KNX_IP_PORT     = 3671;
static const     QString KNX_MULTICAST   = QStringLiteral("224.0.23.12");

// KNXnet/IP SEARCH_REQUEST (KNX spec 03_08_02, §2.6)
static QByteArray buildSearchRequest(quint16 clientPort)
{
    QByteArray pkt;
    // Header
    pkt.append(char(0x06)); pkt.append(char(0x10)); // protocol version
    pkt.append(char(0x02)); pkt.append(char(0x01)); // SEARCH_REQUEST service type
    pkt.append(char(0x00)); pkt.append(char(0x0E)); // total length = 14
    // Client HPAI (UDP)
    pkt.append(char(0x08));                          // structure length
    pkt.append(char(0x01));                          // UDP
    pkt.append(char(0x00)); pkt.append(char(0x00));  // IP 0.0.0.0 (any)
    pkt.append(char(0x00)); pkt.append(char(0x00));
    pkt.append(static_cast<char>(clientPort >> 8));
    pkt.append(static_cast<char>(clientPort & 0xFF));
    return pkt;
}

KnxIpDiscovery::KnxIpDiscovery(QObject *parent)
    : QObject(parent)
    , m_socket(new QUdpSocket(this))
{
    connect(m_socket, &QUdpSocket::readyRead, this, &KnxIpDiscovery::onReadyRead);
}

KnxIpDiscovery::~KnxIpDiscovery() = default;

void KnxIpDiscovery::startSearch()
{
    m_socket->bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress);
    m_socket->writeDatagram(
        buildSearchRequest(m_socket->localPort()),
        QHostAddress(KNX_MULTICAST), KNX_IP_PORT);

    QTimer::singleShot(3000, this, &KnxIpDiscovery::onTimeout);
}

void KnxIpDiscovery::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram dg = m_socket->receiveDatagram();
        QByteArray data = dg.data();
        // Minimal SEARCH_RESPONSE check: service type 0x0202, min length 0x48
        if (data.size() < 8)
            continue;
        uint16_t svc = (static_cast<uint8_t>(data[2]) << 8) | static_cast<uint8_t>(data[3]);
        if (svc != 0x0202)
            continue;

        KnxIpDevice dev;
        dev.ipAddress = dg.senderAddress().toString();
        dev.port      = KNX_IP_PORT;
        // Device name starts at byte 32 in the SEARCH_RESPONSE (after DIB_SUPP_SVC_FAMILIES)
        if (data.size() >= 64)
            dev.name = QString::fromLatin1(data.mid(32, 30)).trimmed();
        if (dev.name.isEmpty())
            dev.name = QStringLiteral("KNXnet/IP Interface (%1)").arg(dev.ipAddress);

        emit deviceFound(dev);
    }
}

void KnxIpDiscovery::onTimeout()
{
    m_socket->close();
    emit searchFinished();
}
