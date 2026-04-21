#include <QtTest>
#include <QTemporaryFile>
#include "Manifest.h"

static const char *VALID_MANIFEST = R"yaml(
id: "test-device"
version: "1.0.0"
manufacturer: "TestCo"
name:
  de: "Testgerät"
  en: "Test device"
hardware:
  target: "STM32G0B1"
  transceiver: "NCN5120"
channels:
  - id: ch1
    name:
      de: "Kanal 1"
      en: "Channel 1"
comObjects:
  - id: co1
    number: 0
    channel: ch1
    name:
      de: "Schalten"
      en: "Switch"
    dpt: "1.001"
    flags: [C, W, T]
parameters:
  - id: p1
    name:
      de: "Verzögerung"
      en: "Delay"
    type: uint16
    unit: ms
    default: 100
    range: [0, 5000]
    memoryOffset: 0
    size: 2
memoryLayout:
  addressTable:     "0x4000"
  associationTable: "0x4100"
  parameterBase:    "0x4400"
  parameterSize:    4
)yaml";

static const char *INVALID_MANIFEST = R"yaml(
version: "1.0.0"
# missing 'id' field
)yaml";

class TestManifestParser : public QObject
{
    Q_OBJECT

private slots:
    void parseValidManifest()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QStringLiteral("manifest-XXXXXX.yaml"));
        QVERIFY(tmp.open());
        tmp.write(VALID_MANIFEST);
        tmp.flush();

        auto result = loadManifest(tmp.fileName());
        QVERIFY(result.has_value());

        const Manifest &m = *result;
        QCOMPARE(m.id,           QStringLiteral("test-device"));
        QCOMPARE(m.version,      QStringLiteral("1.0.0"));
        QCOMPARE(m.manufacturer, QStringLiteral("TestCo"));
        QCOMPARE(m.name.de,      QStringLiteral("Testgerät"));
        QCOMPARE(m.name.en,      QStringLiteral("Test device"));
        QCOMPARE(m.hardware.target, QStringLiteral("STM32G0B1"));
        QVERIFY(m.isValid());
    }

    void parseComObjects()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QStringLiteral("manifest-XXXXXX.yaml"));
        QVERIFY(tmp.open());
        tmp.write(VALID_MANIFEST);
        tmp.flush();

        auto result = loadManifest(tmp.fileName());
        QVERIFY(result.has_value());

        QCOMPARE(result->comObjects.size(), 1);
        QCOMPARE(result->comObjects[0].id,      QStringLiteral("co1"));
        QCOMPARE(result->comObjects[0].dpt,     QStringLiteral("1.001"));
        QCOMPARE(result->comObjects[0].flags,   QStringList({QStringLiteral("C"), QStringLiteral("W"), QStringLiteral("T")}));
    }

    void parseParameters()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QStringLiteral("manifest-XXXXXX.yaml"));
        QVERIFY(tmp.open());
        tmp.write(VALID_MANIFEST);
        tmp.flush();

        auto result = loadManifest(tmp.fileName());
        QVERIFY(result.has_value());

        QCOMPARE(result->parameters.size(), 1);
        const ManifestParameter &p = result->parameters[0];
        QCOMPARE(p.id,           QStringLiteral("p1"));
        QCOMPARE(p.type,         QStringLiteral("uint16"));
        QCOMPARE(p.unit,         QStringLiteral("ms"));
        QCOMPARE(p.defaultValue, QVariant(100));
        QCOMPARE(p.rangeMin,     QVariant(0));
        QCOMPARE(p.rangeMax,     QVariant(5000));
        QCOMPARE(p.memoryOffset, 0u);
        QCOMPARE(p.size,         2u);
        QCOMPARE(p.effectiveSize(), 2u);
    }

    void parseMemoryLayout()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QStringLiteral("manifest-XXXXXX.yaml"));
        QVERIFY(tmp.open());
        tmp.write(VALID_MANIFEST);
        tmp.flush();

        auto result = loadManifest(tmp.fileName());
        QVERIFY(result.has_value());
        QCOMPARE(result->memoryLayout.addressTable,     0x4000u);
        QCOMPARE(result->memoryLayout.associationTable, 0x4100u);
        QCOMPARE(result->memoryLayout.parameterBase,    0x4400u);
        QCOMPARE(result->memoryLayout.parameterSize,    4u);
        QCOMPARE(result->comObjects[0].number, 0);
    }

    void invalidManifestReturnsNullopt()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QStringLiteral("manifest-XXXXXX.yaml"));
        QVERIFY(tmp.open());
        tmp.write(INVALID_MANIFEST);
        tmp.flush();

        auto result = loadManifest(tmp.fileName());
        QVERIFY(!result.has_value());
    }

    void nonexistentFileReturnsNullopt()
    {
        auto result = loadManifest(QStringLiteral("/does/not/exist.yaml"));
        QVERIFY(!result.has_value());
    }

    void referenceManifestLoads()
    {
        // Loads the actual bundled manifest from the repo
        const QString path = QStringLiteral(CATALOG_PATH "/switch-actuator-1ch.yaml");
        auto result = loadManifest(path);
        if (!QFile::exists(path)) QSKIP("Catalog path not available in test environment");
        QVERIFY(result.has_value());
        QCOMPARE(result->id, QStringLiteral("switch-actuator-1ch"));
    }
};

QTEST_MAIN(TestManifestParser)
#include "test_manifest_parser.moc"
