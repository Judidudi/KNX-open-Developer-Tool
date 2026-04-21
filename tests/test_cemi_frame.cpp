#include <QtTest>
#include "CemiFrame.h"

class TestCemiFrame : public QObject
{
    Q_OBJECT

private slots:
    void physAddrToString()
    {
        // 1.1.1 = (1<<12)|(1<<8)|1 = 0x1101
        uint16_t addr = (1 << 12) | (1 << 8) | 1;
        QCOMPARE(CemiFrame::physAddrToString(addr), QStringLiteral("1.1.1"));
    }

    void groupAddrToString()
    {
        // 0/0/1 = (0<<11)|(0<<8)|1 = 0x0001
        uint16_t addr = 0x0001;
        QCOMPARE(CemiFrame::groupAddrToString(addr), QStringLiteral("0/0/1"));

        // 1/2/3 = (1<<11)|(2<<8)|3 = 0x0803
        addr = (1 << 11) | (2 << 8) | 3;
        QCOMPARE(CemiFrame::groupAddrToString(addr), QStringLiteral("1/2/3"));
    }

    void toAndFromBytes()
    {
        CemiFrame f;
        f.messageCode = CemiFrame::MessageCode::LDataReq;
        f.sourceAddress = (1 << 12) | (1 << 8) | 0;  // 1.1.0
        f.destAddress   = (0 << 11) | (0 << 8) | 1;  // 0/0/1
        f.groupAddress  = true;
        f.hopCount      = 6;
        f.apdu          = QByteArray::fromHex("0081");  // GroupValue_Write(1)

        QByteArray bytes = f.toBytes();
        QVERIFY(!bytes.isEmpty());

        CemiFrame parsed = CemiFrame::fromBytes(bytes);
        QCOMPARE(parsed.sourceAddress, f.sourceAddress);
        QCOMPARE(parsed.destAddress,   f.destAddress);
        QCOMPARE(parsed.groupAddress,  f.groupAddress);
        QCOMPARE(parsed.apdu,          f.apdu);
    }

    void tooShortBytesReturnDefault()
    {
        CemiFrame f = CemiFrame::fromBytes(QByteArray::fromHex("29"));
        QCOMPARE(f.sourceAddress, 0);
    }

    void physAddrRoundTrip()
    {
        QCOMPARE(CemiFrame::physAddrFromString(QStringLiteral("1.2.3")), uint16_t(0x1203));
        QCOMPARE(CemiFrame::physAddrFromString(QStringLiteral("15.15.255")), uint16_t(0xFFFF));
    }

    void groupAddrRoundTrip()
    {
        QCOMPARE(CemiFrame::groupAddrFromString(QStringLiteral("0/0/1")), uint16_t(0x0001));
        QCOMPARE(CemiFrame::groupAddrFromString(QStringLiteral("1/2/3")), uint16_t((1 << 11) | (2 << 8) | 3));
    }

    void buildIndividualAddressWriteFrame()
    {
        QByteArray bytes = CemiFrame::buildIndividualAddressWrite(0x1101);
        CemiFrame f = CemiFrame::fromBytes(bytes);
        QCOMPARE(f.destAddress, uint16_t(0x0000));
        QVERIFY(f.apdu.size() >= 4);
        // APCI high byte 0xC0 marks A_IndividualAddress_Write
        QCOMPARE(static_cast<uint8_t>(f.apdu[1]), uint8_t(0xC0));
        QCOMPARE(static_cast<uint8_t>(f.apdu[2]), uint8_t(0x11));
        QCOMPARE(static_cast<uint8_t>(f.apdu[3]), uint8_t(0x01));
    }

    void buildMemoryWriteFrameContainsData()
    {
        QByteArray payload = QByteArray::fromHex("DEADBEEF");
        QByteArray bytes   = CemiFrame::buildMemoryWrite(0x1101, 0x4000, payload);
        CemiFrame f = CemiFrame::fromBytes(bytes);
        QCOMPARE(f.destAddress,   uint16_t(0x1101));
        QCOMPARE(f.groupAddress,  false);
        // APDU: [TPCI][APCI|count][addr_hi][addr_lo][data...]
        QCOMPARE(static_cast<uint8_t>(f.apdu[1]) & 0x3F, 4);   // count
        QCOMPARE(static_cast<uint8_t>(f.apdu[2]), uint8_t(0x40));
        QCOMPARE(static_cast<uint8_t>(f.apdu[3]), uint8_t(0x00));
        QCOMPARE(f.apdu.mid(4), payload);
    }

    void groupValuePayloadOneBit()
    {
        QByteArray bytes = CemiFrame::buildGroupValueWrite(0x0001, QByteArray(1, char(0x01)));
        CemiFrame f = CemiFrame::fromBytes(bytes);
        QVERIFY(f.isGroupValueWrite());
        QCOMPARE(f.groupValuePayload(), QByteArray(1, char(0x01)));
    }
};

QTEST_MAIN(TestCemiFrame)
#include "test_cemi_frame.moc"
