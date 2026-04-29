#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "Manifest.h"
#include "YamlToKnxprod.h"
#include "KnxprodCatalog.h"
#include "KnxApplicationProgram.h"
#include "ZipUtils.h"

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

// ─── Helper: build a minimal .knxprod ZIP from hw + app XML ──────────────────

static QByteArray buildMinimalKnxprod(const QByteArray &hwXml, const QByteArray &appXml)
{
    return ZipUtils::buildZip({
        { QStringLiteral("M-00FA/M-00FA_H-1234_HP-1234.xml"), hwXml  },
        { QStringLiteral("M-00FA/M-00FA_A-1234-0001.xml"),    appXml },
    });
}

static const QByteArray kMinimalHwXml =
    "<KNX><ManufacturerData><Manufacturer RefId=\"M-00FA\">"
    "<Hardware><Hardware Id=\"M-00FA_H-1234\" Name=\"TestDevice\">"
    "<Products><Product Id=\"M-00FA_H-1234_HP-1234\" Text=\"TestDevice\"/></Products>"
    "<Hardware2Programs><Hardware2Program>"
    "<ApplicationProgramRef RefId=\"M-00FA_A-1234-0001\"/>"
    "</Hardware2Program></Hardware2Programs>"
    "</Hardware></Hardware>"
    "</Manufacturer></ManufacturerData></KNX>";

// ─── Test class ───────────────────────────────────────────────────────────────

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

    // ── E1 new tests ──────────────────────────────────────────────────────────

    void loadsComObjectWithIndividualFlags()
    {
        // Manufacturer-style individual flag attributes (no CommunicationFlag)
        const QByteArray appXml =
            "<KNX><ManufacturerData><Manufacturer RefId=\"M-00FA\">"
            "<ApplicationPrograms>"
            "<ApplicationProgram Id=\"M-00FA_A-1234-0001\" Name=\"FlagTest\">"
            "<Static>"
            "<ComObjectTable/>"
            "<ComObjects>"
            "<ComObject Id=\"co_out\" Number=\"0\" Name=\"Output\" DatapointType=\"1.001\""
            "  CommunicationObjectEnable=\"Enabled\" WriteFlag=\"Enabled\""
            "  TransmitFlag=\"Enabled\" ReadFlag=\"Disabled\" UpdateFlag=\"Disabled\"/>"
            "</ComObjects>"
            "</Static>"
            "</ApplicationProgram>"
            "</ApplicationPrograms>"
            "</Manufacturer></ManufacturerData></KNX>";

        const QByteArray zip = buildMinimalKnxprod(kMinimalHwXml, appXml);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile f(dir.filePath(QStringLiteral("flagtest.knxprod")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(zip);
        f.close();

        KnxprodCatalog catalog;
        catalog.addSearchPath(dir.path());
        catalog.reload();
        QCOMPARE(catalog.count(), 1);

        const KnxApplicationProgram *app = catalog.at(0)->appProgram.get();
        QVERIFY(app != nullptr);
        QVERIFY(!app->comObjects.isEmpty());

        const KnxComObject &co = app->comObjects[0];
        QVERIFY(co.flags.contains(QStringLiteral("C")));
        QVERIFY(co.flags.contains(QStringLiteral("W")));
        QVERIFY(co.flags.contains(QStringLiteral("T")));
        QVERIFY(!co.flags.contains(QStringLiteral("R")));
        QVERIFY(!co.flags.contains(QStringLiteral("U")));
    }

    void normalizesDptFormat()
    {
        // DPST-<main>-<sub> → <main>.<sub padded to 3 digits>; DPT-N kept as-is
        const QByteArray appXml =
            "<KNX><ManufacturerData><Manufacturer RefId=\"M-00FA\">"
            "<ApplicationPrograms>"
            "<ApplicationProgram Id=\"M-00FA_A-1234-0001\" Name=\"DptTest\">"
            "<Static>"
            "<ComObjectTable/>"
            "<ComObjects>"
            "<ComObject Id=\"co1\" Number=\"0\" Name=\"Switch\" DatapointType=\"DPST-1-1\""
            "  CommunicationFlag=\"C,W\"/>"
            "<ComObject Id=\"co2\" Number=\"1\" Name=\"Dimmer\" DatapointType=\"DPT-5\""
            "  CommunicationFlag=\"C,W\"/>"
            "</ComObjects>"
            "</Static>"
            "</ApplicationProgram>"
            "</ApplicationPrograms>"
            "</Manufacturer></ManufacturerData></KNX>";

        const QByteArray zip = buildMinimalKnxprod(kMinimalHwXml, appXml);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile f(dir.filePath(QStringLiteral("dpttest.knxprod")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(zip);
        f.close();

        KnxprodCatalog catalog;
        catalog.addSearchPath(dir.path());
        catalog.reload();
        QCOMPARE(catalog.count(), 1);

        const KnxApplicationProgram *app = catalog.at(0)->appProgram.get();
        QVERIFY(app != nullptr);
        QCOMPARE(app->comObjects.size(), 2);
        QCOMPARE(app->comObjects[0].dpt, QStringLiteral("1.001"));  // DPST-1-1 normalized
        QCOMPARE(app->comObjects[1].dpt, QStringLiteral("DPT-5"));  // kept as-is
    }

    void toleratesUnknownParameterType()
    {
        // TypeFloat is an unknown type child – should fall back to UInt/4 bytes without crashing
        const QByteArray appXml =
            "<KNX><ManufacturerData><Manufacturer RefId=\"M-00FA\">"
            "<ApplicationPrograms>"
            "<ApplicationProgram Id=\"M-00FA_A-1234-0001\" Name=\"TypeTest\">"
            "<Static>"
            "<ParameterTypes>"
            "<ParameterType Id=\"PT_Float\">"
            "<TypeFloat Encoding=\"F16\" minInclusive=\"-100\" maxInclusive=\"100\"/>"
            "</ParameterType>"
            "</ParameterTypes>"
            "<Parameters>"
            "<Parameter Id=\"p1\" Name=\"TempOffset\" ParameterType=\"PT_Float\" Offset=\"0\">"
            "<Value>0</Value>"
            "</Parameter>"
            "</Parameters>"
            "</Static>"
            "</ApplicationProgram>"
            "</ApplicationPrograms>"
            "</Manufacturer></ManufacturerData></KNX>";

        const QByteArray zip = buildMinimalKnxprod(kMinimalHwXml, appXml);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile f(dir.filePath(QStringLiteral("typetest.knxprod")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(zip);
        f.close();

        // Must not crash; the parameter type should be present with UInt fallback
        KnxprodCatalog catalog;
        catalog.addSearchPath(dir.path());
        catalog.reload();
        QCOMPARE(catalog.count(), 1);

        const KnxApplicationProgram *app = catalog.at(0)->appProgram.get();
        QVERIFY(app != nullptr);
        QVERIFY(!app->paramTypes.isEmpty());
        const KnxParameterType *pt = app->findType(QStringLiteral("PT_Float"));
        QVERIFY(pt != nullptr);
        QCOMPARE(pt->kind, KnxParameterType::Kind::UInt);
        QCOMPARE(pt->size, uint8_t(4));
    }
};

QTEST_MAIN(TestKnxprodParser)
#include "test_knxprod_parser.moc"
