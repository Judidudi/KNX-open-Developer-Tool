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

    void buildMemoryReadFrame()
    {
        QByteArray bytes = CemiFrame::buildMemoryRead(0x1101, 0x4400, 10);
        CemiFrame f = CemiFrame::fromBytes(bytes);
        QCOMPARE(f.destAddress,  uint16_t(0x1101));
        QCOMPARE(f.groupAddress, false);
        QVERIFY(f.apdu.size() >= 4);
        // APCI bits[9:6] = 0b1000 = A_Memory_Read, count in low 6 bits
        QCOMPARE(static_cast<uint8_t>(f.apdu[1]) & 0x3F, uint8_t(10));
        QCOMPARE(static_cast<uint8_t>(f.apdu[2]), uint8_t(0x44));
        QCOMPARE(static_cast<uint8_t>(f.apdu[3]), uint8_t(0x00));
    }

    void buildMemoryReadClampCount()
    {
        // count > 63 should be clamped to 63
        QByteArray bytes = CemiFrame::buildMemoryRead(0x1101, 0x4000, 100);
        CemiFrame f = CemiFrame::fromBytes(bytes);
        QCOMPARE(static_cast<uint8_t>(f.apdu[1]) & 0x3F, uint8_t(63));
    }

    void isMemoryResponseTrue()
    {
        // Build a synthetic A_Memory_Response frame (APCI 0x240 | count)
        CemiFrame f;
        f.messageCode   = CemiFrame::MessageCode::LDataInd;
        f.sourceAddress = 0x1101;
        f.destAddress   = 0x0000;
        f.groupAddress  = false;
        // APDU: [TPCI|0x42][0x40|count=4][addr_hi][addr_lo][d0][d1][d2][d3]
        f.apdu.append(char(0x42));
        f.apdu.append(char(0x40 | 4));  // A_Memory_Response, 4 bytes
        f.apdu.append(char(0x40));      // address high byte
        f.apdu.append(char(0x00));      // address low byte
        f.apdu.append(char(0x01));
        f.apdu.append(char(0x02));
        f.apdu.append(char(0x03));
        f.apdu.append(char(0x04));

        QVERIFY(f.isMemoryResponse());

        uint16_t addr = 0;
        QByteArray data;
        QVERIFY(f.memoryResponseData(addr, data));
        QCOMPARE(addr, uint16_t(0x4000));
        QCOMPARE(data.size(), 4);
        QCOMPARE(static_cast<uint8_t>(data[0]), uint8_t(0x01));
        QCOMPARE(static_cast<uint8_t>(data[3]), uint8_t(0x04));
    }

    void isMemoryResponseFalseForGroupTelegram()
    {
        CemiFrame f;
        f.groupAddress = true;
        f.apdu.append(char(0x00));
        f.apdu.append(char(0x40 | 4));
        f.apdu.append(char(0x40));
        f.apdu.append(char(0x00));
        QVERIFY(!f.isMemoryResponse());
    }

    void memoryResponseDataReturnsFalseForShortApdu()
    {
        CemiFrame f;
        f.groupAddress = false;
        f.apdu.append(char(0x42));
        f.apdu.append(char(0x40 | 4));  // claims 4 bytes but we don't have them
        // Only 2 bytes in apdu → size < 4
        uint16_t addr = 0;
        QByteArray data;
        QVERIFY(!f.memoryResponseData(addr, data));
    }
};

QTEST_MAIN(TestCemiFrame)
#include "test_cemi_frame.moc"
