#include <QtTest>
#include "Project.h"
#include "TopologyNode.h"
#include "DeviceInstance.h"

class TestProjectModel : public QObject
{
    Q_OBJECT

private slots:
    void newProjectDefaults()
    {
        Project p;
        QCOMPARE(p.areaCount(), 0);
        QVERIFY(p.groupAddresses().isEmpty());
        QCOMPARE(p.created(), QDate::currentDate());
    }

    void addAreaAndLine()
    {
        Project p;
        p.setName(QStringLiteral("TestProjekt"));
        QCOMPARE(p.name(), QStringLiteral("TestProjekt"));

        auto area = std::make_unique<TopologyNode>(
            TopologyNode::Type::Area, 1, QStringLiteral("EG"));
        auto line = std::make_unique<TopologyNode>(
            TopologyNode::Type::Line, 1, QStringLiteral("Linie 1"));

        TopologyNode *linePtr = line.get();
        area->addChild(std::move(line));
        p.addArea(std::move(area));

        QCOMPARE(p.areaCount(), 1);
        QCOMPARE(p.areaAt(0)->childCount(), 1);
        QCOMPARE(p.areaAt(0)->childAt(0), linePtr);
    }

    void addDeviceToLine()
    {
        auto area = std::make_unique<TopologyNode>(
            TopologyNode::Type::Area, 1, QStringLiteral("EG"));
        auto line = std::make_unique<TopologyNode>(
            TopologyNode::Type::Line, 1, QStringLiteral("Linie 1"));

        auto dev = std::make_unique<DeviceInstance>(
            QStringLiteral("d1"),
            QStringLiteral("switch-actuator-1ch"),
            QStringLiteral("1.0.0"));
        dev->setPhysicalAddress(QStringLiteral("1.1.1"));

        TopologyNode *linePtr = line.get();
        line->addDevice(std::move(dev));
        area->addChild(std::move(line));

        QCOMPARE(linePtr->deviceCount(), 1);
        QCOMPARE(linePtr->deviceAt(0)->physicalAddress(), QStringLiteral("1.1.1"));
    }

    void groupAddressLookup()
    {
        Project p;
        p.addGroupAddress(GroupAddress(0, 0, 1, QStringLiteral("Wohnzimmer"), QStringLiteral("1.001")));
        p.addGroupAddress(GroupAddress(0, 0, 2, QStringLiteral("Küche"),      QStringLiteral("1.001")));

        QCOMPARE(p.groupAddresses().size(), 2);
        GroupAddress *ga = p.findGroupAddress(QStringLiteral("0/0/1"));
        QVERIFY(ga != nullptr);
        QCOMPARE(ga->name(), QStringLiteral("Wohnzimmer"));

        QVERIFY(p.findGroupAddress(QStringLiteral("9/9/9")) == nullptr);
    }

    void topologyNodeParentLink()
    {
        auto area = std::make_unique<TopologyNode>(
            TopologyNode::Type::Area, 1, QStringLiteral("EG"));
        TopologyNode *areaPtr = area.get();
        auto line = std::make_unique<TopologyNode>(
            TopologyNode::Type::Line, 1, QStringLiteral("L1"));
        TopologyNode *linePtr = line.get();
        area->addChild(std::move(line));

        QCOMPARE(linePtr->parent(), areaPtr);
        QCOMPARE(linePtr->indexInParent(), 0);
    }
};

QTEST_MAIN(TestProjectModel)
#include "test_project_model.moc"
