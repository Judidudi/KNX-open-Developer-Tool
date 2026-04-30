#include <QtTest>
#include "Project.h"
#include "TopologyNode.h"
#include "DeviceInstance.h"
#include "BuildingPart.h"
#include "GroupAddress.h"

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

    void removeAreaFromProject()
    {
        Project p;
        p.addArea(std::make_unique<TopologyNode>(TopologyNode::Type::Area, 1, QStringLiteral("A1")));
        p.addArea(std::make_unique<TopologyNode>(TopologyNode::Type::Area, 2, QStringLiteral("A2")));
        QCOMPARE(p.areaCount(), 2);

        p.removeAreaAt(0);
        QCOMPARE(p.areaCount(), 1);
        QCOMPARE(p.areaAt(0)->name(), QStringLiteral("A2"));
    }

    void removeLineFromArea()
    {
        auto area = std::make_unique<TopologyNode>(TopologyNode::Type::Area, 1, QStringLiteral("A"));
        area->addChild(std::make_unique<TopologyNode>(TopologyNode::Type::Line, 1, QStringLiteral("L1")));
        area->addChild(std::make_unique<TopologyNode>(TopologyNode::Type::Line, 2, QStringLiteral("L2")));
        QCOMPARE(area->childCount(), 2);

        area->removeChildAt(0);
        QCOMPARE(area->childCount(), 1);
        QCOMPARE(area->childAt(0)->name(), QStringLiteral("L2"));
    }

    void removeDeviceFromLine()
    {
        auto line = std::make_unique<TopologyNode>(TopologyNode::Type::Line, 1, QStringLiteral("L"));
        line->addDevice(std::make_unique<DeviceInstance>(QStringLiteral("d1"), QStringLiteral("ref"), QStringLiteral("app")));
        line->addDevice(std::make_unique<DeviceInstance>(QStringLiteral("d2"), QStringLiteral("ref"), QStringLiteral("app")));
        QCOMPARE(line->deviceCount(), 2);

        DeviceInstance *d2 = line->deviceAt(1);
        const int idx = line->indexOfDevice(d2);
        QCOMPARE(idx, 1);
        line->removeDeviceAt(idx);
        QCOMPARE(line->deviceCount(), 1);
        QCOMPARE(line->deviceAt(0)->id(), QStringLiteral("d1"));
    }

    void removeGroupAddress()
    {
        Project p;
        p.addGroupAddress(GroupAddress(0, 0, 1, QStringLiteral("GA1"), QStringLiteral("1.001")));
        p.addGroupAddress(GroupAddress(0, 0, 2, QStringLiteral("GA2"), QStringLiteral("1.001")));
        QCOMPARE(p.groupAddresses().size(), 2);

        p.removeGroupAddress(QStringLiteral("0/0/1"));
        QCOMPARE(p.groupAddresses().size(), 1);
        QCOMPARE(p.groupAddresses()[0].name(), QStringLiteral("GA2"));
        QVERIFY(p.findGroupAddress(QStringLiteral("0/0/1")) == nullptr);
    }

    void buildingPartHierarchy()
    {
        auto building = std::make_unique<BuildingPart>(BuildingPart::Type::Building, QStringLiteral("Haus"));
        auto floor    = std::make_unique<BuildingPart>(BuildingPart::Type::Floor,    QStringLiteral("EG"));
        auto room     = std::make_unique<BuildingPart>(BuildingPart::Type::Room,     QStringLiteral("Wohnzimmer"));

        BuildingPart *floorPtr = floor.get();
        BuildingPart *roomPtr  = room.get();

        room->addGroupAddressRef(QStringLiteral("ga-1"));
        floor->addChild(std::move(room));
        building->addChild(std::move(floor));

        QCOMPARE(building->childCount(), 1);
        QCOMPARE(building->childAt(0), floorPtr);
        QCOMPARE(floorPtr->childCount(), 1);
        QCOMPARE(floorPtr->childAt(0), roomPtr);
        QCOMPARE(roomPtr->parent(), floorPtr);
        QCOMPARE(floorPtr->parent(), building.get());
        QCOMPARE(roomPtr->groupAddressRefs().size(), 1);
        QCOMPARE(roomPtr->groupAddressRefs()[0], QStringLiteral("ga-1"));

        // Remove floor
        building->removeChildAt(0);
        QCOMPARE(building->childCount(), 0);
    }

    void buildingAddRemoveFromProject()
    {
        Project p;
        p.addBuilding(std::make_unique<BuildingPart>(BuildingPart::Type::Building, QStringLiteral("B1")));
        p.addBuilding(std::make_unique<BuildingPart>(BuildingPart::Type::Building, QStringLiteral("B2")));
        QCOMPARE(p.buildingCount(), 2);

        p.removeBuildingAt(0);
        QCOMPARE(p.buildingCount(), 1);
        QCOMPARE(p.buildingAt(0)->name(), QStringLiteral("B2"));
    }
};

QTEST_MAIN(TestProjectModel)
#include "test_project_model.moc"
