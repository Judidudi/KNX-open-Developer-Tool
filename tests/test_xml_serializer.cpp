#include <QtTest>
#include <QTemporaryFile>
#include "Project.h"
#include "TopologyNode.h"
#include "DeviceInstance.h"
#include "ProjectXmlSerializer.h"

class TestXmlSerializer : public QObject
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

        // Save
        QTemporaryFile tmp;
        tmp.setFileTemplate(QStringLiteral("project-XXXXXX.kodtproj"));
        QVERIFY(tmp.open());
        const QString path = tmp.fileName();
        tmp.close(); // allow serializer to reopen

        QVERIFY(ProjectXmlSerializer::save(orig, path));

        // Load
        auto loaded = ProjectXmlSerializer::load(path);
        QVERIFY(loaded != nullptr);

        QCOMPARE(loaded->name(), QStringLiteral("TestProjekt"));
        QCOMPARE(loaded->created(), QDate(2026, 4, 20));
        QCOMPARE(loaded->areaCount(), 1);

        TopologyNode *area2 = loaded->areaAt(0);
        QCOMPARE(area2->name(), QStringLiteral("Erdgeschoss"));
        QCOMPARE(area2->childCount(), 1);

        TopologyNode *line2 = area2->childAt(0);
        QCOMPARE(line2->name(), QStringLiteral("Linie 1"));
        QCOMPARE(line2->deviceCount(), 1);

        DeviceInstance *dev2 = line2->deviceAt(0);
        QCOMPARE(dev2->physicalAddress(), QStringLiteral("1.1.1"));
        QCOMPARE(dev2->catalogRef(),      QStringLiteral("switch-actuator-1ch"));
        QCOMPARE(dev2->parameters().value(QStringLiteral("p_startup_delay")).toString(), QStringLiteral("500"));
        QCOMPARE(dev2->links().size(), 1);
        QCOMPARE(dev2->links()[0].comObjectId, QStringLiteral("co_switch_ch1"));
        QCOMPARE(dev2->links()[0].ga.toString(), QStringLiteral("0/0/1"));

        QCOMPARE(loaded->groupAddresses().size(), 1);
        QCOMPARE(loaded->groupAddresses()[0].name(), QStringLiteral("Wohnzimmer Licht"));
        QCOMPARE(loaded->groupAddresses()[0].dpt(),  QStringLiteral("1.001"));
    }

    void loadNonexistentFile()
    {
        auto result = ProjectXmlSerializer::load(QStringLiteral("/does/not/exist.kodtproj"));
        QVERIFY(result == nullptr);
    }

    void emptyProject()
    {
        Project empty;
        empty.setName(QStringLiteral("Leer"));

        QTemporaryFile tmp;
        tmp.setFileTemplate(QStringLiteral("project-XXXXXX.kodtproj"));
        QVERIFY(tmp.open());
        const QString path = tmp.fileName();
        tmp.close();

        QVERIFY(ProjectXmlSerializer::save(empty, path));
        auto loaded = ProjectXmlSerializer::load(path);
        QVERIFY(loaded != nullptr);
        QCOMPARE(loaded->name(), QStringLiteral("Leer"));
        QCOMPARE(loaded->areaCount(), 0);
        QVERIFY(loaded->groupAddresses().isEmpty());
    }
};

QTEST_MAIN(TestXmlSerializer)
#include "test_xml_serializer.moc"
