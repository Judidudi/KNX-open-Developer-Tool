#include <QtTest>
#include "DeviceInstance.h"
#include "ComObjectLink.h"
#include "GroupAddress.h"
#include "KnxApplicationProgram.h"

class TestDeviceInstance : public QObject
{
    Q_OBJECT

private slots:
    void constructorSetsIds()
    {
        DeviceInstance d(QStringLiteral("dev-1"),
                         QStringLiteral("M-00FA_H-0001"),
                         QStringLiteral("M-00FA_A-0001-00-0001"));
        QCOMPARE(d.id(),             QStringLiteral("dev-1"));
        QCOMPARE(d.productRefId(),   QStringLiteral("M-00FA_H-0001"));
        QCOMPARE(d.appProgramRefId(),QStringLiteral("M-00FA_A-0001-00-0001"));
        QVERIFY(d.physicalAddress().isEmpty());
        QVERIFY(d.description().isEmpty());
        QVERIFY(d.parameters().empty());
        QVERIFY(d.links().isEmpty());
    }

    void physicalAddress()
    {
        DeviceInstance d(QStringLiteral("d"), QStringLiteral("p"), QStringLiteral("a"));
        d.setPhysicalAddress(QStringLiteral("1.2.3"));
        QCOMPARE(d.physicalAddress(), QStringLiteral("1.2.3"));
    }

    void description()
    {
        DeviceInstance d(QStringLiteral("d"), QStringLiteral("p"), QStringLiteral("a"));
        d.setDescription(QStringLiteral("Schaltaktor Wohnzimmer"));
        QCOMPARE(d.description(), QStringLiteral("Schaltaktor Wohnzimmer"));
    }

    void parameterValues()
    {
        DeviceInstance d(QStringLiteral("d"), QStringLiteral("p"), QStringLiteral("a"));
        d.parameters()[QStringLiteral("p_delay")]   = 500;
        d.parameters()[QStringLiteral("p_channel")] = QStringLiteral("1");

        QCOMPARE(d.parameters().at(QStringLiteral("p_delay")).toInt(), 500);
        QCOMPARE(d.parameters().at(QStringLiteral("p_channel")).toString(),
                 QStringLiteral("1"));
        QCOMPARE(static_cast<int>(d.parameters().size()), 2);
    }

    void parameterOverwrite()
    {
        DeviceInstance d(QStringLiteral("d"), QStringLiteral("p"), QStringLiteral("a"));
        d.parameters()[QStringLiteral("p_x")] = 1;
        d.parameters()[QStringLiteral("p_x")] = 99;
        QCOMPARE(d.parameters().at(QStringLiteral("p_x")).toInt(), 99);
    }

    void addLink()
    {
        DeviceInstance d(QStringLiteral("d"), QStringLiteral("p"), QStringLiteral("a"));

        ComObjectLink lnk;
        lnk.comObjectId = QStringLiteral("co_switch");
        lnk.ga          = GroupAddress::fromString(QStringLiteral("0/0/1"));
        lnk.direction   = ComObjectLink::Direction::Send;
        d.addLink(lnk);

        QCOMPARE(d.links().size(), 1);
        QCOMPARE(d.links().first().comObjectId, QStringLiteral("co_switch"));
        QCOMPARE(d.links().first().ga.toString(), QStringLiteral("0/0/1"));
        QCOMPARE(d.links().first().direction, ComObjectLink::Direction::Send);
    }

    void multipleLinks()
    {
        DeviceInstance d(QStringLiteral("d"), QStringLiteral("p"), QStringLiteral("a"));

        ComObjectLink lnk1;
        lnk1.comObjectId = QStringLiteral("co_1");
        lnk1.ga          = GroupAddress::fromString(QStringLiteral("0/0/1"));
        lnk1.direction   = ComObjectLink::Direction::Send;

        ComObjectLink lnk2;
        lnk2.comObjectId = QStringLiteral("co_2");
        lnk2.ga          = GroupAddress::fromString(QStringLiteral("0/0/2"));
        lnk2.direction   = ComObjectLink::Direction::Receive;

        d.addLink(lnk1);
        d.addLink(lnk2);

        QCOMPARE(d.links().size(), 2);
        QCOMPARE(d.links().at(0).comObjectId, QStringLiteral("co_1"));
        QCOMPARE(d.links().at(1).direction, ComObjectLink::Direction::Receive);
    }

    void appProgramPointer()
    {
        DeviceInstance d(QStringLiteral("d"), QStringLiteral("p"), QStringLiteral("a"));
        QVERIFY(d.appProgram() == nullptr);

        auto prog = std::make_shared<KnxApplicationProgram>();
        prog->id = QStringLiteral("M-00FA_A-0001-00-0001");
        d.setAppProgram(prog);

        QVERIFY(d.appProgram() != nullptr);
        QCOMPARE(d.appProgram()->id, QStringLiteral("M-00FA_A-0001-00-0001"));
        QCOMPARE(d.appProgramShared(), prog);
    }

    void constAccessors()
    {
        DeviceInstance d(QStringLiteral("d"), QStringLiteral("p"), QStringLiteral("a"));
        d.parameters()[QStringLiteral("k")] = 42;

        const DeviceInstance &cd = d;
        QCOMPARE(cd.parameters().at(QStringLiteral("k")).toInt(), 42);
        QVERIFY(cd.links().isEmpty());
        QVERIFY(cd.appProgram() == nullptr);
    }
};

QTEST_MAIN(TestDeviceInstance)
#include "test_device_instance.moc"
