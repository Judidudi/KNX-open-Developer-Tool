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
};

QTEST_MAIN(TestCemiFrame)
#include "test_cemi_frame.moc"
