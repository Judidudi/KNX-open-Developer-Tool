#include <QtTest>
#include "TransportConnection.h"
#include "IKnxInterface.h"
#include "CemiFrame.h"

// ─── Mock interface ───────────────────────────────────────────────────────────

class MockIface : public IKnxInterface
{
    Q_OBJECT
public:
    explicit MockIface(QObject *parent = nullptr) : IKnxInterface(parent) {}
    bool connectToInterface() override    { return true; }
    void disconnectFromInterface() override {}
    bool isConnected() const override     { return true; }
    void sendCemiFrame(const QByteArray &f) override { sent.append(f); }

    // Push a fake incoming frame
    void push(const QByteArray &cemi) { emit cemiFrameReceived(cemi); }

    // Build a T_ACK from 'srcPa' for the given sequence number
    QByteArray makeTAck(uint16_t srcPa, uint8_t seq) const {
        CemiFrame f;
        f.messageCode   = CemiFrame::MessageCode::LDataInd;
        f.sourceAddress = srcPa;
        f.destAddress   = 0x0000;
        f.groupAddress  = false;
        f.apdu.append(static_cast<char>(0xC2 | ((seq & 0x0F) << 2)));
        return f.toBytes();
    }

    // Build a T_Disconnect from 'srcPa'
    QByteArray makeTDisconnect(uint16_t srcPa) const {
        CemiFrame f;
        f.messageCode   = CemiFrame::MessageCode::LDataInd;
        f.sourceAddress = srcPa;
        f.destAddress   = 0x0000;
        f.groupAddress  = false;
        f.apdu.append(char(0x81));
        return f.toBytes();
    }

    // Build a T_Data_Connected[seq] memory response from 'srcPa'
    QByteArray makeMemoryResponse(uint16_t srcPa, uint16_t addr,
                                   const QByteArray &data, uint8_t seq = 0) const {
        CemiFrame f;
        f.messageCode   = CemiFrame::MessageCode::LDataInd;
        f.sourceAddress = srcPa;
        f.destAddress   = 0x0000;
        f.groupAddress  = false;
        const uint8_t cnt = static_cast<uint8_t>(qMin<int>(data.size(), 12));
        f.apdu.append(static_cast<char>(0x42 | ((seq & 0x0F) << 2)));  // T_Data_Connected
        f.apdu.append(static_cast<char>(0x40 | cnt));                   // Memory_Response
        f.apdu.append(static_cast<char>(addr >> 8));
        f.apdu.append(static_cast<char>(addr & 0xFF));
        f.apdu.append(data.left(cnt));
        return f.toBytes();
    }

    QList<QByteArray> sent;
};

// ─── Tests ────────────────────────────────────────────────────────────────────

class TestTransportConnection : public QObject
{
    Q_OBJECT

private slots:

    void openSendsTConnect()
    {
        MockIface iface;
        TransportConnection tc(&iface);
        tc.open(0x1101);

        QVERIFY(tc.isOpen());
        QVERIFY(!iface.sent.isEmpty());
        const CemiFrame f = CemiFrame::fromBytes(iface.sent.first());
        QVERIFY(f.isTConnect());
        QCOMPARE(f.destAddress, uint16_t(0x1101));
    }

    void openEmitsOpenedSignal()
    {
        MockIface iface;
        TransportConnection tc(&iface);
        QSignalSpy spy(&tc, &TransportConnection::opened);
        tc.open(0x1101);
        QCOMPARE(spy.count(), 1);
    }

    void closeSendsTDisconnect()
    {
        MockIface iface;
        TransportConnection tc(&iface);
        tc.open(0x1101);
        iface.sent.clear();
        tc.close();

        QVERIFY(!tc.isOpen());
        QVERIFY(!iface.sent.isEmpty());
        const CemiFrame f = CemiFrame::fromBytes(iface.sent.first());
        QVERIFY(f.isTDisconnect());
        QCOMPARE(f.destAddress, uint16_t(0x1101));
    }

    void closeEmitsClosedSignal()
    {
        MockIface iface;
        TransportConnection tc(&iface);
        tc.open(0x1101);
        QSignalSpy spy(&tc, &TransportConnection::closed);
        tc.close();
        QCOMPARE(spy.count(), 1);
    }

    void sendMemoryWriteQueuedAndSent()
    {
        MockIface iface;
        TransportConnection tc(&iface);
        tc.open(0x1101);
        iface.sent.clear();

        tc.sendMemoryWrite(0x4000, QByteArray::fromHex("DEADBEEF"));

        // Frame should be in sent
        QVERIFY(!iface.sent.isEmpty());
        const CemiFrame f = CemiFrame::fromBytes(iface.sent.first());
        QVERIFY(f.isTDataConnected());
        QCOMPARE(f.destAddress, uint16_t(0x1101));
        // seq 0
        QCOMPARE(f.tSeqNumber(), uint8_t(0));
    }

    void ackAdvancesSequenceAndDrainsQueue()
    {
        MockIface iface;
        TransportConnection tc(&iface);
        tc.open(0x1101);
        iface.sent.clear();

        tc.sendMemoryWrite(0x4000, QByteArray(4, 'A'));
        tc.sendMemoryWrite(0x4004, QByteArray(4, 'B'));

        // After first send: only first frame in flight
        QCOMPARE(iface.sent.size(), 1);
        QCOMPARE(CemiFrame::fromBytes(iface.sent[0]).tSeqNumber(), uint8_t(0));

        // Inject T_ACK[0] from device
        tc.handleIncoming(iface.makeTAck(0x1101, 0));

        // Second frame should now be sent with seq 1
        QCOMPARE(iface.sent.size(), 2);
        QCOMPARE(CemiFrame::fromBytes(iface.sent[1]).tSeqNumber(), uint8_t(1));

        // Inject T_ACK[1]
        QSignalSpy idleSpy(&tc, &TransportConnection::idle);
        tc.handleIncoming(iface.makeTAck(0x1101, 1));
        QCOMPARE(idleSpy.count(), 1);   // queue drained → idle
    }

    void incomingTDataConnectedEmitsApduReceived()
    {
        MockIface iface;
        TransportConnection tc(&iface);
        tc.open(0x1101);
        iface.sent.clear();

        QSignalSpy spy(&tc, &TransportConnection::apduReceived);
        const QByteArray memResp = iface.makeMemoryResponse(0x1101, 0x4000,
                                                             QByteArray::fromHex("0102"), 0);
        tc.handleIncoming(memResp);

        QCOMPARE(spy.count(), 1);

        // Transport must also have sent T_ACK back
        bool sentAck = false;
        for (const auto &raw : iface.sent) {
            if (CemiFrame::fromBytes(raw).isTAck()) { sentAck = true; break; }
        }
        QVERIFY(sentAck);

        // apduReceived payload: TPCI stripped → [0x42, ...].mid(1) = [0x40|cnt, addr_hi, ...]
        const QByteArray apdu = spy.at(0).at(0).toByteArray();
        QVERIFY(apdu.size() >= 3);
        QCOMPARE(static_cast<uint8_t>(apdu[0]) & 0xC0, uint8_t(0x40));  // memory response
    }

    void peerInitiatedDisconnectEmitsClosed()
    {
        MockIface iface;
        TransportConnection tc(&iface);
        tc.open(0x1101);

        QSignalSpy spy(&tc, &TransportConnection::closed);
        tc.handleIncoming(iface.makeTDisconnect(0x1101));

        QVERIFY(!tc.isOpen());
        QCOMPARE(spy.count(), 1);
    }

    void framesFromOtherSourceIgnored()
    {
        MockIface iface;
        TransportConnection tc(&iface);
        tc.open(0x1101);

        // T_ACK from a different PA should not be processed
        const bool handled = tc.handleIncoming(iface.makeTAck(0x1102, 0));
        QVERIFY(!handled);
    }

    void groupFramesIgnored()
    {
        MockIface iface;
        TransportConnection tc(&iface);
        tc.open(0x1101);

        // Group-addressed frame (GA telegram) must be ignored
        CemiFrame ga;
        ga.messageCode   = CemiFrame::MessageCode::LDataInd;
        ga.sourceAddress = 0x1101;
        ga.destAddress   = 0x0001;
        ga.groupAddress  = true;
        ga.apdu.append(char(0x00));
        ga.apdu.append(char(0x80));  // GroupValue_Write
        const bool handled = tc.handleIncoming(ga.toBytes());
        QVERIFY(!handled);
    }

    void ackTimeoutTriggersRetransmit()
    {
        MockIface iface;
        TransportConnection tc(&iface);
        tc.setAckTimeoutMs(50);     // short for testing
        tc.setMaxRetries(1);
        tc.open(0x1101);
        iface.sent.clear();

        QSignalSpy errorSpy(&tc, &TransportConnection::error);
        tc.sendMemoryWrite(0x4000, QByteArray(1, 'X'));

        // Let the timer fire twice (retransmit then give up)
        QTRY_VERIFY_WITH_TIMEOUT(errorSpy.count() > 0, 500);
        QVERIFY(!tc.isOpen());

        // Should have sent: original + 1 retransmit (maxRetries=1)
        // +1 T_Disconnect from error handling
        QVERIFY(iface.sent.size() >= 2);
    }

    void sequenceWrapsAt15()
    {
        MockIface iface;
        TransportConnection tc(&iface);
        tc.open(0x1101);
        iface.sent.clear();

        // Send 17 frames with ACK for each → seq wraps 0..15, 0, 1
        for (int i = 0; i < 17; ++i) {
            tc.sendMemoryWrite(static_cast<uint16_t>(0x4000 + i), QByteArray(1, 'A'));
            const uint8_t expectedSeq = static_cast<uint8_t>(i & 0x0F);
            const CemiFrame sent = CemiFrame::fromBytes(iface.sent.last());
            QCOMPARE(sent.tSeqNumber(), expectedSeq);
            tc.handleIncoming(iface.makeTAck(0x1101, expectedSeq));
        }
    }
};

QTEST_MAIN(TestTransportConnection)
#include "test_transport_connection.moc"
