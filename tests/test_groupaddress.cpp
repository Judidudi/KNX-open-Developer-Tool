#include <QtTest>
#include "GroupAddress.h"

class TestGroupAddress : public QObject
{
    Q_OBJECT

private slots:
    void constructAndToString()
    {
        GroupAddress ga(1, 2, 3, QStringLiteral("Test"), QStringLiteral("1.001"));
        QCOMPARE(ga.toString(), QStringLiteral("1/2/3"));
        QCOMPARE(ga.name(),     QStringLiteral("Test"));
        QCOMPARE(ga.dpt(),      QStringLiteral("1.001"));
        QVERIFY(ga.isValid());
    }

    void fromString_valid()
    {
        GroupAddress ga = GroupAddress::fromString(QStringLiteral("0/0/1"));
        QVERIFY(ga.isValid());
        QCOMPARE(ga.main(),   0);
        QCOMPARE(ga.middle(), 0);
        QCOMPARE(ga.sub(),    1);
    }

    void fromString_invalid()
    {
        GroupAddress ga = GroupAddress::fromString(QStringLiteral("not/a/ga"));
        QVERIFY(!ga.isValid());

        GroupAddress ga2 = GroupAddress::fromString(QStringLiteral("1/2"));
        QVERIFY(!ga2.isValid());
    }

    void rawRoundtrip()
    {
        GroupAddress orig(5, 3, 200);
        uint16_t raw = orig.toRaw();
        GroupAddress rt = GroupAddress::fromRaw(raw);
        QCOMPARE(rt.main(),   orig.main());
        QCOMPARE(rt.middle(), orig.middle());
        QCOMPARE(rt.sub(),    orig.sub());
    }

    void maxValues()
    {
        GroupAddress ga(31, 7, 255);
        QVERIFY(ga.isValid());
        QCOMPARE(ga.toString(), QStringLiteral("31/7/255"));
    }

    void equality()
    {
        GroupAddress a(1, 0, 5);
        GroupAddress b(1, 0, 5);
        GroupAddress c(1, 0, 6);
        QVERIFY(a == b);
        QVERIFY(!(a == c));
    }
};

QTEST_MAIN(TestGroupAddress)
#include "test_groupaddress.moc"
