#include "TransportConnection.h"

#include "IKnxInterface.h"
#include "CemiFrame.h"

#include <QTimer>

static constexpr int kDefaultAckTimeoutMs = 3000;   // KNX spec: T_Data_Connected ACK ≤ 3s

TransportConnection::TransportConnection(IKnxInterface *iface, QObject *parent)
    : QObject(parent)
    , m_iface(iface)
    , m_ackTimer(new QTimer(this))
{
    m_ackTimer->setSingleShot(true);
    m_ackTimer->setInterval(kDefaultAckTimeoutMs);
    connect(m_ackTimer, &QTimer::timeout, this, &TransportConnection::onAckTimeout);
}

void TransportConnection::setAckTimeoutMs(int ms)
{
    m_ackTimer->setInterval(ms);
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

void TransportConnection::open(uint16_t destPa)
{
    if (m_open && m_destPa == destPa)
        return;
    if (m_open)
        close();

    m_destPa     = destPa;
    m_txSeq      = 0;
    m_rxSeq      = 0;
    m_inFlight   = false;
    m_retryCount = 0;
    m_queue.clear();
    m_open       = true;

    if (m_iface)
        m_iface->sendCemiFrame(CemiFrame::buildTConnect(destPa));
    emit opened();
}

void TransportConnection::close()
{
    if (!m_open) return;
    m_ackTimer->stop();
    if (m_iface && m_iface->isConnected())
        m_iface->sendCemiFrame(CemiFrame::buildTDisconnect(m_destPa));
    m_queue.clear();
    m_inFlight = false;
    m_open     = false;
    emit closed();
}

// ─── Public send API ─────────────────────────────────────────────────────────

void TransportConnection::sendMemoryWrite(uint16_t memAddr, const QByteArray &data)
{
    Pending p;
    p.kind = Kind::MemoryWrite;
    p.memAddr = memAddr;
    p.data = data;
    enqueue(p);
}

void TransportConnection::sendMemoryRead(uint16_t memAddr, uint8_t count)
{
    Pending p;
    p.kind = Kind::MemoryRead;
    p.memAddr = memAddr;
    p.count = count;
    enqueue(p);
}

void TransportConnection::sendPropertyWrite(uint8_t objIdx, uint8_t propId, uint8_t count,
                                              uint16_t startIdx, const QByteArray &data)
{
    Pending p;
    p.kind = Kind::PropertyWrite;
    p.objIdx = objIdx;
    p.propId = propId;
    p.count = count;
    p.startIdx = startIdx;
    p.data = data;
    enqueue(p);
}

void TransportConnection::sendPropertyRead(uint8_t objIdx, uint8_t propId, uint8_t count,
                                             uint16_t startIdx)
{
    Pending p;
    p.kind = Kind::PropertyRead;
    p.objIdx = objIdx;
    p.propId = propId;
    p.count = count;
    p.startIdx = startIdx;
    enqueue(p);
}

void TransportConnection::sendRestart()
{
    Pending p;
    p.kind = Kind::Restart;
    enqueue(p);
}

// ─── Internal: queue & send ──────────────────────────────────────────────────

void TransportConnection::enqueue(Pending p)
{
    if (!m_open) return;
    m_queue.enqueue(p);
    if (!m_inFlight)
        trySendNext();
}

void TransportConnection::trySendNext()
{
    if (m_queue.isEmpty()) {
        m_inFlight = false;
        emit idle();
        return;
    }
    m_lastSent       = m_queue.dequeue();
    m_lastSent.seq   = m_txSeq;
    m_inFlight       = true;
    m_retryCount     = 0;
    buildAndSend(m_lastSent);
}

void TransportConnection::buildAndSend(const Pending &p)
{
    if (!m_iface) return;

    QByteArray frame;
    switch (p.kind) {
    case Kind::MemoryWrite:
        frame = CemiFrame::buildMemoryWrite(m_destPa, p.memAddr, p.data, p.seq);
        break;
    case Kind::MemoryRead:
        frame = CemiFrame::buildMemoryRead(m_destPa, p.memAddr, p.count, p.seq);
        break;
    case Kind::PropertyWrite:
        frame = CemiFrame::buildPropertyValueWrite(m_destPa, p.objIdx, p.propId,
                                                    p.count, p.startIdx, p.data, p.seq);
        break;
    case Kind::PropertyRead:
        frame = CemiFrame::buildPropertyValueRead(m_destPa, p.objIdx, p.propId,
                                                   p.count, p.startIdx, p.seq);
        break;
    case Kind::Restart:
        frame = CemiFrame::buildRestart(m_destPa, p.seq);
        break;
    }
    m_iface->sendCemiFrame(frame);
    m_ackTimer->start();
}

void TransportConnection::sendTAckFor(uint8_t seq)
{
    if (m_iface && m_iface->isConnected())
        m_iface->sendCemiFrame(CemiFrame::buildTAck(m_destPa, seq));
}

void TransportConnection::sendTNakFor(uint8_t seq)
{
    if (m_iface && m_iface->isConnected())
        m_iface->sendCemiFrame(CemiFrame::buildTNak(m_destPa, seq));
}

// ─── Incoming frame handling ─────────────────────────────────────────────────

bool TransportConnection::handleIncoming(const QByteArray &cemi)
{
    if (!m_open) return false;

    const CemiFrame f = CemiFrame::fromBytes(cemi);

    // The frame must originate from our peer (or unspecified 0x0000 – tunneling
    // routers sometimes leave src empty in the loopback path)
    const bool peerMatch = (f.sourceAddress == m_destPa) || (f.sourceAddress == 0);
    if (!peerMatch || f.groupAddress)
        return false;

    if (f.isTAck()) {
        const uint8_t seq = f.tSeqNumber();
        if (m_inFlight && seq == m_lastSent.seq) {
            m_ackTimer->stop();
            // advance sequence (0..15 wrap)
            m_txSeq    = static_cast<uint8_t>((m_txSeq + 1) & 0x0F);
            m_inFlight = false;
            trySendNext();
        }
        return true;
    }

    if (f.isTNak()) {
        // Negative ACK → retransmit (counted)
        if (m_inFlight) {
            ++m_retryCount;
            if (m_retryCount > m_maxRetries) {
                m_ackTimer->stop();
                emit error(tr("T_NAK nach %1 Versuchen — Verbindung wird geschlossen")
                           .arg(m_maxRetries));
                close();
                return true;
            }
            buildAndSend(m_lastSent);
        }
        return true;
    }

    if (f.isTDisconnect()) {
        // Peer-initiated disconnect
        m_ackTimer->stop();
        m_open = false;
        m_queue.clear();
        m_inFlight = false;
        emit closed();
        return true;
    }

    if (f.isTConnect()) {
        // Spurious — we initiated, peer should not send T_Connect. Ignore.
        return true;
    }

    if (f.isTDataConnected()) {
        // Inbound data: must ACK and forward APDU (everything after TPCI byte).
        const uint8_t seq = f.tSeqNumber();
        sendTAckFor(seq);
        // Track expected sequence loosely (for monitoring); KNX peers may resend
        // on lost ACK so a duplicate seq is normal — handle idempotently.
        if (seq == m_rxSeq)
            m_rxSeq = static_cast<uint8_t>((m_rxSeq + 1) & 0x0F);
        emit apduReceived(f.apdu.mid(1));
        return true;
    }

    return false;
}

// ─── ACK timeout ─────────────────────────────────────────────────────────────

void TransportConnection::onAckTimeout()
{
    if (!m_inFlight) return;

    ++m_retryCount;
    if (m_retryCount > m_maxRetries) {
        m_inFlight = false;
        emit error(tr("Keine T_ACK-Antwort nach %1 Versuchen — Gerät antwortet nicht")
                   .arg(m_maxRetries));
        close();
        return;
    }
    buildAndSend(m_lastSent);
}
