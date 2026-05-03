#include <QtTest>
#include "TopologyNode.h"
#include "DeviceInstance.h"

class TestTopologyNode : public QObject
{
    Q_OBJECT

private slots:
    void constructorSetsFields()
    {
        TopologyNode n(TopologyNode::Type::Area, 3, QStringLiteral("Erdgeschoss"));
        QCOMPARE(n.type(), TopologyNode::Type::Area);
        QCOMPARE(n.id(), 3);
        QCOMPARE(n.name(), QStringLiteral("Erdgeschoss"));
        QVERIFY(n.parent() == nullptr);
        QCOMPARE(n.childCount(), 0);
        QCOMPARE(n.deviceCount(), 0);
    }

    void setNameWorks()
    {
        TopologyNode n(TopologyNode::Type::Line, 1, QStringLiteral("Alt"));
        n.setName(QStringLiteral("Neu"));
        QCOMPARE(n.name(), QStringLiteral("Neu"));
    }

    void addChildSetsParent()
    {
        auto area = std::make_unique<TopologyNode>(
            TopologyNode::Type::Area, 1, QStringLiteral("Bereich 1"));
        auto line = std::make_unique<TopologyNode>(
            TopologyNode::Type::Line, 1, QStringLiteral("Linie 1"));

        TopologyNode *linePtr = line.get();
        area->addChild(std::move(line));

        QCOMPARE(area->childCount(), 1);
        QCOMPARE(area->childAt(0), linePtr);
        QCOMPARE(linePtr->parent(), area.get());
    }

    void removeChildAt()
    {
        auto area = std::make_unique<TopologyNode>(
            TopologyNode::Type::Area, 1, QStringLiteral("A"));
        area->addChild(std::make_unique<TopologyNode>(
            TopologyNode::Type::Line, 1, QStringLiteral("L1")));
        area->addChild(std::make_unique<TopologyNode>(
            TopologyNode::Type::Line, 2, QStringLiteral("L2")));
        area->addChild(std::make_unique<TopologyNode>(
            TopologyNode::Type::Line, 3, QStringLiteral("L3")));

        area->removeChildAt(1); // remove L2
        QCOMPARE(area->childCount(), 2);
        QCOMPARE(area->childAt(0)->name(), QStringLiteral("L1"));
        QCOMPARE(area->childAt(1)->name(), QStringLiteral("L3"));
    }

    void takeChildAtTransfersOwnership()
    {
        auto area = std::make_unique<TopologyNode>(
            TopologyNode::Type::Area, 1, QStringLiteral("A"));
        area->addChild(std::make_unique<TopologyNode>(
            TopologyNode::Type::Line, 1, QStringLiteral("L1")));

        auto taken = area->takeChildAt(0);
        QVERIFY(taken != nullptr);
        QCOMPARE(taken->name(), QStringLiteral("L1"));
        QCOMPARE(area->childCount(), 0);
        QVERIFY(taken->parent() == nullptr);
    }

    void insertChildAtPosition()
    {
        auto area = std::make_unique<TopologyNode>(
            TopologyNode::Type::Area, 1, QStringLiteral("A"));
        area->addChild(std::make_unique<TopologyNode>(
            TopologyNode::Type::Line, 1, QStringLiteral("L1")));
        area->addChild(std::make_unique<TopologyNode>(
            TopologyNode::Type::Line, 3, QStringLiteral("L3")));
        area->insertChildAt(1, std::make_unique<TopologyNode>(
            TopologyNode::Type::Line, 2, QStringLiteral("L2")));

        QCOMPARE(area->childCount(), 3);
        QCOMPARE(area->childAt(0)->name(), QStringLiteral("L1"));
        QCOMPARE(area->childAt(1)->name(), QStringLiteral("L2"));
        QCOMPARE(area->childAt(2)->name(), QStringLiteral("L3"));
    }

    void indexOfChild()
    {
        auto area = std::make_unique<TopologyNode>(
            TopologyNode::Type::Area, 1, QStringLiteral("A"));
        area->addChild(std::make_unique<TopologyNode>(
            TopologyNode::Type::Line, 1, QStringLiteral("L1")));
        area->addChild(std::make_unique<TopologyNode>(
            TopologyNode::Type::Line, 2, QStringLiteral("L2")));

        const TopologyNode *l2 = area->childAt(1);
        QCOMPARE(area->indexOfChild(l2), 1);

        TopologyNode foreign(TopologyNode::Type::Line, 9, QStringLiteral("X"));
        QCOMPARE(area->indexOfChild(&foreign), -1);
    }

    void indexInParent()
    {
        auto area = std::make_unique<TopologyNode>(
            TopologyNode::Type::Area, 1, QStringLiteral("A"));
        area->addChild(std::make_unique<TopologyNode>(
            TopologyNode::Type::Line, 1, QStringLiteral("L1")));
        area->addChild(std::make_unique<TopologyNode>(
            TopologyNode::Type::Line, 2, QStringLiteral("L2")));

        QCOMPARE(area->childAt(0)->indexInParent(), 0);
        QCOMPARE(area->childAt(1)->indexInParent(), 1);
        QCOMPARE(area->indexInParent(), -1); // no parent
    }

    void addAndRemoveDevice()
    {
        auto line = std::make_unique<TopologyNode>(
            TopologyNode::Type::Line, 1, QStringLiteral("L1"));

        auto dev = std::make_unique<DeviceInstance>(
            QStringLiteral("d1"), QStringLiteral("prod-1"), QStringLiteral("app-1"));
        dev->setPhysicalAddress(QStringLiteral("1.1.1"));

        DeviceInstance *devPtr = dev.get();
        line->addDevice(std::move(dev));

        QCOMPARE(line->deviceCount(), 1);
        QCOMPARE(line->deviceAt(0), devPtr);
        QCOMPARE(line->indexOfDevice(devPtr), 0);

        line->removeDeviceAt(0);
        QCOMPARE(line->deviceCount(), 0);
    }

    void takeDeviceTransfersOwnership()
    {
        auto line = std::make_unique<TopologyNode>(
            TopologyNode::Type::Line, 1, QStringLiteral("L1"));
        line->addDevice(std::make_unique<DeviceInstance>(
            QStringLiteral("d1"), QStringLiteral("p"), QStringLiteral("a")));

        auto taken = line->takeDeviceAt(0);
        QVERIFY(taken != nullptr);
        QCOMPARE(line->deviceCount(), 0);
    }

    void insertDeviceAtPosition()
    {
        auto line = std::make_unique<TopologyNode>(
            TopologyNode::Type::Line, 1, QStringLiteral("L"));
        line->addDevice(std::make_unique<DeviceInstance>(
            QStringLiteral("d1"), QStringLiteral("p"), QStringLiteral("a")));
        line->addDevice(std::make_unique<DeviceInstance>(
            QStringLiteral("d3"), QStringLiteral("p"), QStringLiteral("a")));
        line->insertDeviceAt(1, std::make_unique<DeviceInstance>(
            QStringLiteral("d2"), QStringLiteral("p"), QStringLiteral("a")));

        QCOMPARE(line->deviceCount(), 3);
        QCOMPARE(line->deviceAt(0)->id(), QStringLiteral("d1"));
        QCOMPARE(line->deviceAt(1)->id(), QStringLiteral("d2"));
        QCOMPARE(line->deviceAt(2)->id(), QStringLiteral("d3"));
    }
};

QTEST_MAIN(TestTopologyNode)
#include "test_topology_node.moc"
