#include <QtTest>
#include <QTemporaryDir>
#include "Manifest.h"
#include "YamlToKnxprod.h"
#include "KnxprodCatalog.h"
#include "KnxApplicationProgram.h"

static const char *YAML_DEVICE = R"yaml(
id: "test-knxprod-device"
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
  - id: p_delay
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

class TestKnxprodParser : public QObject
{
    Q_OBJECT

private slots:
    void yamlToKnxprodGeneratesValidIds()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QStringLiteral("manifest-XXXXXX.yaml"));
        QVERIFY(tmp.open());
        tmp.write(YAML_DEVICE);
        tmp.flush();

        auto m = loadManifest(tmp.fileName());
        QVERIFY(m.has_value());

        const QString productRef = YamlToKnxprod::productRefId(*m);
        const QString appRef     = YamlToKnxprod::appProgramRefId(*m);

        QVERIFY(productRef.startsWith(QStringLiteral("M-00FA_H-")));
        QVERIFY(productRef.contains(QStringLiteral("_HP-")));
        QVERIFY(appRef.startsWith(QStringLiteral("M-00FA_A-")));

        // IDs must be deterministic
        QCOMPARE(YamlToKnxprod::productRefId(*m), productRef);
        QCOMPARE(YamlToKnxprod::appProgramRefId(*m), appRef);
    }

    void yamlToKnxprodWritesFile()
    {
        QTemporaryFile yaml;
        yaml.setFileTemplate(QStringLiteral("manifest-XXXXXX.yaml"));
        QVERIFY(yaml.open());
        yaml.write(YAML_DEVICE);
        yaml.flush();

        auto m = loadManifest(yaml.fileName());
        QVERIFY(m.has_value());

        QTemporaryDir outDir;
        QVERIFY(outDir.isValid());
        const QString outPath = outDir.filePath(QStringLiteral("test.knxprod"));

        QVERIFY(YamlToKnxprod::writeFile(*m, outPath));
        QVERIFY(QFile::exists(outPath));
        QVERIFY(QFileInfo(outPath).size() > 0);
    }

    void catalogLoadsKnxprod()
    {
        QTemporaryFile yaml;
        yaml.setFileTemplate(QStringLiteral("manifest-XXXXXX.yaml"));
        QVERIFY(yaml.open());
        yaml.write(YAML_DEVICE);
        yaml.flush();

        auto m = loadManifest(yaml.fileName());
        QVERIFY(m.has_value());

        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString knxprodPath = dir.filePath(QStringLiteral("test.knxprod"));
        QVERIFY(YamlToKnxprod::writeFile(*m, knxprodPath));

        KnxprodCatalog catalog;
        catalog.addSearchPath(dir.path());
        catalog.reload();

        QCOMPARE(catalog.count(), 1);
        const KnxHardwareProduct *prod = catalog.at(0);
        QVERIFY(prod != nullptr);
        QCOMPARE(prod->productId, YamlToKnxprod::productRefId(*m));
        QVERIFY(!prod->productName.isEmpty());
        QVERIFY(prod->appProgram != nullptr);
        QVERIFY(prod->appProgram->isValid());
    }

    void catalogAppProgramHasComObjects()
    {
        QTemporaryFile yaml;
        yaml.setFileTemplate(QStringLiteral("manifest-XXXXXX.yaml"));
        QVERIFY(yaml.open());
        yaml.write(YAML_DEVICE);
        yaml.flush();

        auto m = loadManifest(yaml.fileName());
        QVERIFY(m.has_value());

        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        QVERIFY(YamlToKnxprod::writeFile(*m, dir.filePath(QStringLiteral("test.knxprod"))));

        KnxprodCatalog catalog;
        catalog.addSearchPath(dir.path());
        catalog.reload();

        QVERIFY(catalog.count() > 0);
        const KnxApplicationProgram *app = catalog.at(0)->appProgram.get();
        QVERIFY(app != nullptr);
        QVERIFY(!app->comObjects.isEmpty());
        QCOMPARE(app->comObjects[0].id, QStringLiteral("co1"));
        QCOMPARE(app->comObjects[0].number, 0);
    }

    void catalogAppProgramHasParameters()
    {
        QTemporaryFile yaml;
        yaml.setFileTemplate(QStringLiteral("manifest-XXXXXX.yaml"));
        QVERIFY(yaml.open());
        yaml.write(YAML_DEVICE);
        yaml.flush();

        auto m = loadManifest(yaml.fileName());
        QVERIFY(m.has_value());

        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        QVERIFY(YamlToKnxprod::writeFile(*m, dir.filePath(QStringLiteral("test.knxprod"))));

        KnxprodCatalog catalog;
        catalog.addSearchPath(dir.path());
        catalog.reload();

        QVERIFY(catalog.count() > 0);
        const KnxApplicationProgram *app = catalog.at(0)->appProgram.get();
        QVERIFY(app != nullptr);
        QVERIFY(!app->parameters.isEmpty());

        const KnxParameter *p = app->findParameter(QStringLiteral("p_delay"));
        QVERIFY(p != nullptr);
        QCOMPARE(p->defaultValue, QVariant(100));
        QCOMPARE(p->offset, uint32_t(0));

        const KnxParameterType *pt = app->findType(p->typeId);
        QVERIFY(pt != nullptr);
        QCOMPARE(pt->kind, KnxParameterType::Kind::UInt);
        QCOMPARE(pt->size, uint8_t(2));
    }

    void catalogLookupByProductRef()
    {
        QTemporaryFile yaml;
        yaml.setFileTemplate(QStringLiteral("manifest-XXXXXX.yaml"));
        QVERIFY(yaml.open());
        yaml.write(YAML_DEVICE);
        yaml.flush();

        auto m = loadManifest(yaml.fileName());
        QVERIFY(m.has_value());

        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString productRef = YamlToKnxprod::productRefId(*m);
        QVERIFY(YamlToKnxprod::writeFile(*m, dir.filePath(QStringLiteral("test.knxprod"))));

        KnxprodCatalog catalog;
        catalog.addSearchPath(dir.path());
        catalog.reload();

        auto appProgram = catalog.sharedByProductRef(productRef);
        QVERIFY(appProgram != nullptr);
        QVERIFY(appProgram->isValid());
    }

    void referenceManifestRoundtrip()
    {
        const QString yamlPath = QStringLiteral(CATALOG_PATH "/switch-actuator-1ch.yaml");
        if (!QFile::exists(yamlPath))
            QSKIP("Catalog path not available in test environment");

        auto m = loadManifest(yamlPath);
        QVERIFY(m.has_value());

        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString outPath = dir.filePath(QStringLiteral("switch-actuator-1ch.knxprod"));
        QVERIFY(YamlToKnxprod::writeFile(*m, outPath));

        KnxprodCatalog catalog;
        catalog.addSearchPath(dir.path());
        catalog.reload();

        QCOMPARE(catalog.count(), 1);
        QVERIFY(catalog.at(0)->appProgram != nullptr);
        QVERIFY(catalog.at(0)->appProgram->isValid());
    }
};

QTEST_MAIN(TestKnxprodParser)
#include "test_knxprod_parser.moc"
