#include <QtTest>
#include <QTemporaryFile>
#include <QStandardPaths>
#include "Project.h"
#include "TopologyNode.h"
#include "DeviceInstance.h"
#include "GroupAddress.h"
#include "ComObjectLink.h"
#include "KnxprojSerializer.h"

class TestKnxprojSerializer : public QObject
{
    Q_OBJECT

private slots:
    void saveAndLoad()
    {
        // Build a project
        Project orig;
        orig.setName(QStringLiteral("TestProjekt"));
        orig.setCreated(QDate(2026, 4, 20));

        auto area = std::make_unique<TopologyNode>(
            TopologyNode::Type::Area, 1, QStringLiteral("Erdgeschoss"));
        auto line = std::make_unique<TopologyNode>(
            TopologyNode::Type::Line, 1, QStringLiteral("Linie 1"));

        auto dev = std::make_unique<DeviceInstance>(
            QStringLiteral("d1"),
            QStringLiteral("switch-actuator-1ch"),
            QStringLiteral("1.0.0"));
        dev->setPhysicalAddress(QStringLiteral("1.1.1"));
        dev->parameters()[QStringLiteral("p_startup_delay")] = 500;

        ComObjectLink link;
        link.comObjectId = QStringLiteral("co_switch_ch1");
        link.ga = GroupAddress::fromString(QStringLiteral("0/0/1"));
        dev->addLink(link);

        line->addDevice(std::move(dev));
        area->addChild(std::move(line));
        orig.addArea(std::move(area));

        orig.addGroupAddress(GroupAddress(0, 0, 1, QStringLiteral("Wohnzimmer Licht"), QStringLiteral("1.001")));

        // Save to .knxproj
        QTemporaryFile tmp;
        tmp.setFileTemplate(QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                            + QStringLiteral("/project-XXXXXX.knxproj"));
        QVERIFY(tmp.open());
        const QString path = tmp.fileName();
        tmp.close();

        QVERIFY(KnxprojSerializer::save(orig, path));
        QVERIFY(!orig.knxprojId().isEmpty()); // ID was generated

        // Load
        auto loaded = KnxprojSerializer::load(path);
        QVERIFY(loaded != nullptr);

        QCOMPARE(loaded->name(), QStringLiteral("TestProjekt"));
        QCOMPARE(loaded->knxprojId(), orig.knxprojId()); // ID preserved
        QCOMPARE(loaded->areaCount(), 1);

        TopologyNode *area2 = loaded->areaAt(0);
        QCOMPARE(area2->name(), QStringLiteral("Erdgeschoss"));
        QCOMPARE(area2->childCount(), 1);

        TopologyNode *line2 = area2->childAt(0);
        QCOMPARE(line2->name(), QStringLiteral("Linie 1"));
        QCOMPARE(line2->deviceCount(), 1);

        DeviceInstance *dev2 = line2->deviceAt(0);
        QCOMPARE(dev2->physicalAddress(), QStringLiteral("1.1.1"));
        QCOMPARE(dev2->productRefId(),    QStringLiteral("switch-actuator-1ch"));
        QCOMPARE(dev2->appProgramRefId(), QStringLiteral("1.0.0"));

        const auto paramIt = dev2->parameters().find(QStringLiteral("p_startup_delay"));
        QVERIFY(paramIt != dev2->parameters().end());
        QCOMPARE(paramIt->second.toString(), QStringLiteral("500"));

        QCOMPARE(dev2->links().size(), 1);
        QCOMPARE(dev2->links()[0].comObjectId, QStringLiteral("co_switch_ch1"));
        QCOMPARE(dev2->links()[0].ga.toString(), QStringLiteral("0/0/1"));

        QCOMPARE(loaded->groupAddresses().size(), 1);
        QCOMPARE(loaded->groupAddresses()[0].name(), QStringLiteral("Wohnzimmer Licht"));
        QCOMPARE(loaded->groupAddresses()[0].dpt(),  QStringLiteral("1.001"));
    }

    void loadNonexistentFile()
    {
        auto result = KnxprojSerializer::load(QStringLiteral("/does/not/exist.knxproj"));
        QVERIFY(result == nullptr);
    }

    void emptyProject()
    {
        Project empty;
        empty.setName(QStringLiteral("Leer"));

        QTemporaryFile tmp;
        tmp.setFileTemplate(QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                            + QStringLiteral("/project-XXXXXX.knxproj"));
        QVERIFY(tmp.open());
        const QString path = tmp.fileName();
        tmp.close();

        QVERIFY(KnxprojSerializer::save(empty, path));
        auto loaded = KnxprojSerializer::load(path);
        QVERIFY(loaded != nullptr);
        QCOMPARE(loaded->name(), QStringLiteral("Leer"));
        QCOMPARE(loaded->areaCount(), 0);
        QVERIFY(loaded->groupAddresses().isEmpty());
    }

    void idStability()
    {
        // Saving twice should preserve the same project ID
        Project proj;
        proj.setName(QStringLiteral("IdTest"));

        QTemporaryFile tmp;
        tmp.setFileTemplate(QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                            + QStringLiteral("/project-XXXXXX.knxproj"));
        QVERIFY(tmp.open());
        const QString path = tmp.fileName();
        tmp.close();

        QVERIFY(KnxprojSerializer::save(proj, path));
        const QString id1 = proj.knxprojId();
        QVERIFY(!id1.isEmpty());

        QVERIFY(KnxprojSerializer::save(proj, path));
        QCOMPARE(proj.knxprojId(), id1); // same ID on second save
    }

    void deviceDescriptionRoundtrip()
    {
        Project orig;
        orig.setName(QStringLiteral("DescTest"));

        auto area = std::make_unique<TopologyNode>(TopologyNode::Type::Area, 1, QStringLiteral("Bereich 1"));
        auto line = std::make_unique<TopologyNode>(TopologyNode::Type::Line, 1, QStringLiteral("Linie 1"));

        auto dev = std::make_unique<DeviceInstance>(
            QStringLiteral("d1"), QStringLiteral("prod-ref"), QStringLiteral("app-ref"));
        dev->setPhysicalAddress(QStringLiteral("1.1.1"));
        dev->setDescription(QStringLiteral("Schaltaktor Wohnzimmer"));

        line->addDevice(std::move(dev));
        area->addChild(std::move(line));
        orig.addArea(std::move(area));

        QTemporaryFile tmp;
        tmp.setFileTemplate(QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                            + QStringLiteral("/desc-XXXXXX.knxproj"));
        QVERIFY(tmp.open());
        const QString path = tmp.fileName();
        tmp.close();

        QVERIFY(KnxprojSerializer::save(orig, path));

        auto loaded = KnxprojSerializer::load(path);
        QVERIFY(loaded != nullptr);
        QCOMPARE(loaded->areaCount(), 1);

        DeviceInstance *dev2 = loaded->areaAt(0)->childAt(0)->deviceAt(0);
        QVERIFY(dev2 != nullptr);
        QCOMPARE(dev2->description(), QStringLiteral("Schaltaktor Wohnzimmer"));
    }

    void comObjectDirectionRoundtrip()
    {
        Project orig;
        orig.setName(QStringLiteral("DirTest"));

        auto area = std::make_unique<TopologyNode>(TopologyNode::Type::Area, 1, QStringLiteral("A"));
        auto line = std::make_unique<TopologyNode>(TopologyNode::Type::Line, 1, QStringLiteral("L"));

        auto dev = std::make_unique<DeviceInstance>(
            QStringLiteral("d1"), QStringLiteral("prod-ref"), QStringLiteral("app-ref"));
        dev->setPhysicalAddress(QStringLiteral("1.1.1"));

        orig.addGroupAddress(GroupAddress(0, 0, 1, QStringLiteral("GA1"), QStringLiteral("1.001")));
        orig.addGroupAddress(GroupAddress(0, 0, 2, QStringLiteral("GA2"), QStringLiteral("1.001")));

        ComObjectLink sendLink;
        sendLink.comObjectId = QStringLiteral("co_out");
        sendLink.ga          = GroupAddress::fromString(QStringLiteral("0/0/1"));
        sendLink.direction   = ComObjectLink::Direction::Send;
        dev->addLink(sendLink);

        ComObjectLink recvLink;
        recvLink.comObjectId = QStringLiteral("co_status");
        recvLink.ga          = GroupAddress::fromString(QStringLiteral("0/0/2"));
        recvLink.direction   = ComObjectLink::Direction::Receive;
        dev->addLink(recvLink);

        line->addDevice(std::move(dev));
        area->addChild(std::move(line));
        orig.addArea(std::move(area));

        QTemporaryFile tmp;
        tmp.setFileTemplate(QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                            + QStringLiteral("/dir-XXXXXX.knxproj"));
        QVERIFY(tmp.open());
        const QString path = tmp.fileName();
        tmp.close();

        QVERIFY(KnxprojSerializer::save(orig, path));

        auto loaded = KnxprojSerializer::load(path);
        QVERIFY(loaded != nullptr);

        DeviceInstance *dev2 = loaded->areaAt(0)->childAt(0)->deviceAt(0);
        QVERIFY(dev2 != nullptr);
        QCOMPARE(dev2->links().size(), 2);

        // Find each link by comObjectId and verify direction is preserved
        ComObjectLink::Direction sendDir = ComObjectLink::Direction::Receive;
        ComObjectLink::Direction recvDir = ComObjectLink::Direction::Send;
        for (const ComObjectLink &lnk : dev2->links()) {
            if (lnk.comObjectId == QStringLiteral("co_out"))
                sendDir = lnk.direction;
            else if (lnk.comObjectId == QStringLiteral("co_status"))
                recvDir = lnk.direction;
        }
        QCOMPARE(sendDir, ComObjectLink::Direction::Send);
        QCOMPARE(recvDir, ComObjectLink::Direction::Receive);
    }
};

QTEST_MAIN(TestKnxprojSerializer)
#include "test_xml_serializer.moc"
