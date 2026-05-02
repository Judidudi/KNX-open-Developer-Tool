#pragma once

#include <QObject>
#include <QByteArray>
#include <QQueue>
#include <cstdint>

class IKnxInterface;
class QTimer;

// One transport-layer connection to a single KNX device (KNX spec 03_03_04).
//
// Manages T_Connect / T_Data_Connected[seq] / T_ACK / T_NAK / T_Disconnect
// for connection-oriented point-to-point communication.  Without this a
// strict-spec BCU will silently ignore A_Memory_Write / A_PropertyValue_Write.
//
// Lifecycle:
//   open(pa) -> sends T_Connect; transitions to Open.  T_Connect is unconfirmed
//               on the wire — the first T_ACK[0] of a subsequent T_Data_Connected
//               implicitly confirms that the device accepted the connection.
//   sendXxx() queues a frame to be sent as T_Data_Connected[seq] with auto-incremented
//               sequence numbers (0..15, wrapping).  Waits for matching T_ACK[seq]
//               before sending the next.  On T_NAK or ACK timeout: up to 3 retransmits;
//               on giving up, emits error() and closes.
//   close() -> sends T_Disconnect; transitions to Closed.
//
// Incoming T_Data_Connected from the device is acknowledged with T_ACK[seq] and
// re-emitted via apduReceived().
class TransportConnection : public QObject
{
    Q_OBJECT
public:
    explicit TransportConnection(IKnxInterface *iface, QObject *parent = nullptr);

    void open(uint16_t destPa);
    void close();
    bool isOpen() const { return m_open; }

    uint16_t destinationAddress() const { return m_destPa; }

    // High-level connection-oriented operations.  Each call queues exactly one
    // T_Data_Connected frame with auto-assigned sequence number.
    void sendMemoryWrite (uint16_t memAddr, const QByteArray &data);
    void sendMemoryRead  (uint16_t memAddr, uint8_t count);
    void sendPropertyWrite(uint8_t objIdx, uint8_t propId, uint8_t count,
                           uint16_t startIdx, const QByteArray &data);
    void sendPropertyRead (uint8_t objIdx, uint8_t propId, uint8_t count,
                           uint16_t startIdx);
    void sendRestart();

    // Returns true if the frame was for us (matching dest/src address) and
    // was processed.  Caller (DeviceProgrammer) still receives unrelated
    // frames via the interface signal directly.
    bool handleIncoming(const QByteArray &cemi);

    // Tunables (for tests)
    void setAckTimeoutMs(int ms);
    void setMaxRetries(int n)   { m_maxRetries = n; }

signals:
    void opened();
    void closed();
    void idle();                                  // send queue drained
    void apduReceived(const QByteArray &apdu);    // received T_Data_Connected payload
    void error(const QString &message);

private slots:
    void onAckTimeout();

private:
    enum class Kind {
        MemoryWrite, MemoryRead, PropertyWrite, PropertyRead, Restart
    };
    struct Pending {
        Kind        kind;
        uint16_t    memAddr   = 0;
        uint8_t     count     = 0;
        uint8_t     objIdx    = 0;
        uint8_t     propId    = 0;
        uint16_t    startIdx  = 0;
        QByteArray  data;
        uint8_t     seq       = 0;     // seq used for current attempt
    };

    void enqueue(Pending p);
    void trySendNext();
    void buildAndSend(const Pending &p);
    void sendTAckFor(uint8_t seq);
    void sendTNakFor(uint8_t seq);

    IKnxInterface       *m_iface       = nullptr;
    QTimer              *m_ackTimer    = nullptr;
    bool                 m_open        = false;
    uint16_t             m_destPa      = 0;
    uint8_t              m_txSeq       = 0;     // next seq to send (0..15)
    uint8_t              m_rxSeq       = 0;     // next expected received seq
    QQueue<Pending>      m_queue;
    bool                 m_inFlight    = false;
    Pending              m_lastSent;
    int                  m_retryCount  = 0;
    int                  m_maxRetries  = 3;
};
