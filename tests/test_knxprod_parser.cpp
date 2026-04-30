#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "KnxprodCatalog.h"
#include "KnxApplicationProgram.h"
#include "ZipUtils.h"

// ─── XML fixtures ─────────────────────────────────────────────────────────────

// Hardware XML using the ETS6 standard <Hardware2ProgrammeVersion> element.
static const QByteArray kHwXmlEts6 =
    "<KNX><ManufacturerData><Manufacturer RefId=\"M-00FA\">"
    "<Hardware><Hardware Id=\"M-00FA_H-1234\" Name=\"TestDevice\">"
    "<Products><Product Id=\"M-00FA_H-1234_HP-1234\" Text=\"TestDevice\"/></Products>"
    "<Hardware2Programs><Hardware2Program>"
    "<Hardware2ProgrammeVersion ApplicationProgramRefId=\"M-00FA_A-1234-0001\"/>"
    "</Hardware2Program></Hardware2Programs>"
    "</Hardware></Hardware>"
    "</Manufacturer></ManufacturerData></KNX>";

// Legacy Hardware XML using <ApplicationProgramRef RefId="…"> (our own generated format).
static const QByteArray kHwXmlLegacy =
    "<KNX><ManufacturerData><Manufacturer RefId=\"M-00FA\">"
    "<Hardware><Hardware Id=\"M-00FA_H-1234\" Name=\"TestDevice\">"
    "<Products><Product Id=\"M-00FA_H-1234_HP-1234\" Text=\"TestDevice\"/></Products>"
    "<Hardware2Programs><Hardware2Program>"
    "<ApplicationProgramRef RefId=\"M-00FA_A-1234-0001\"/>"
    "</Hardware2Program></Hardware2Programs>"
    "</Hardware></Hardware>"
    "</Manufacturer></ManufacturerData></KNX>";

static QByteArray makeAppXml(const QString &extraBody)
{
    return (
        "<KNX><ManufacturerData><Manufacturer RefId=\"M-00FA\">"
        "<ApplicationPrograms>"
        "<ApplicationProgram Id=\"M-00FA_A-1234-0001\" Name=\"TestApp\">"
        "<Static>"
        + extraBody +
        "</Static>"
        "</ApplicationProgram>"
        "</ApplicationPrograms>"
        "</Manufacturer></ManufacturerData></KNX>"
    ).toUtf8();
}

static QByteArray buildKnxprod(const QByteArray &hwXml, const QByteArray &appXml)
{
    return ZipUtils::buildZip({
        { QStringLiteral("M-00FA/M-00FA_H-1234_HP-1234.xml"), hwXml  },
        { QStringLiteral("M-00FA/M-00FA_A-1234-0001.xml"),    appXml },
    });
}

// Write a .knxprod to a temp dir and return the catalog loaded from that dir.
static KnxprodCatalog catalogFromZip(const QByteArray &zip, QTemporaryDir &dir)
{
    const QString path = dir.filePath(QStringLiteral("test.knxprod"));
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(zip);
    f.close();

    KnxprodCatalog cat;
    cat.addSearchPath(dir.path());
    cat.reload();
    return cat;
}

// ─── Test class ───────────────────────────────────────────────────────────────

class TestKnxprodParser : public QObject
{
    Q_OBJECT

private slots:

    // ── Basic loading ─────────────────────────────────────────────────────────

    void catalogLoadsProductEts6Format()
    {
        // ETS6 standard format: <Hardware2ProgrammeVersion ApplicationProgramRefId="…">
        const QByteArray appXml = makeAppXml(
            "<ComObjectTable/>"
            "<ComObjects>"
            "<ComObject Id=\"co1\" Number=\"0\" Name=\"Switch\" DatapointType=\"1.001\""
            "  CommunicationFlag=\"C,W\"/>"
            "</ComObjects>");

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KnxprodCatalog cat = catalogFromZip(buildKnxprod(kHwXmlEts6, appXml), dir);

        QCOMPARE(cat.count(), 1);
        const KnxHardwareProduct *prod = cat.at(0);
        QVERIFY(prod != nullptr);
        QCOMPARE(prod->productId, QStringLiteral("M-00FA_H-1234_HP-1234"));
        QVERIFY(!prod->productName.isEmpty());
        QVERIFY(prod->appProgram != nullptr);
        QVERIFY(prod->appProgram->isValid());
    }

    void catalogLoadsProductLegacyFormat()
    {
        // Legacy format: <ApplicationProgramRef RefId="…">
        const QByteArray appXml = makeAppXml(
            "<ComObjects>"
            "<ComObject Id=\"co1\" Number=\"0\" Name=\"Switch\" DatapointType=\"1.001\""
            "  CommunicationFlag=\"C,W\"/>"
            "</ComObjects>");

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KnxprodCatalog cat = catalogFromZip(buildKnxprod(kHwXmlLegacy, appXml), dir);

        QCOMPARE(cat.count(), 1);
        QVERIFY(cat.at(0)->appProgram != nullptr);
    }

    void catalogLoadsParameters()
    {
        const QByteArray appXml = makeAppXml(
            "<ParameterTypes>"
            "<ParameterType Id=\"PT_Delay\">"
            "<TypeNumber Type=\"unsignedInt\" SizeByte=\"2\" minInclusive=\"0\" maxInclusive=\"5000\"/>"
            "</ParameterType>"
            "</ParameterTypes>"
            "<Parameters>"
            "<Parameter Id=\"p_delay\" Name=\"Delay\" ParameterType=\"PT_Delay\" Offset=\"0\">"
            "<Value>100</Value>"
            "</Parameter>"
            "</Parameters>");

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KnxprodCatalog cat = catalogFromZip(buildKnxprod(kHwXmlEts6, appXml), dir);

        QCOMPARE(cat.count(), 1);
        const KnxApplicationProgram *app = cat.at(0)->appProgram.get();
        QVERIFY(app != nullptr);
        QVERIFY(!app->parameters.isEmpty());

        const KnxParameter *p = app->findParameter(QStringLiteral("p_delay"));
        QVERIFY(p != nullptr);
        QCOMPARE(p->defaultValue, QVariant(QStringLiteral("100")));
        QCOMPARE(p->offset, uint32_t(0));

        const KnxParameterType *pt = app->findType(QStringLiteral("PT_Delay"));
        QVERIFY(pt != nullptr);
        QCOMPARE(pt->kind, KnxParameterType::Kind::UInt);
        QCOMPARE(pt->size, uint8_t(2));
    }

    void catalogLoadsComObjects()
    {
        const QByteArray appXml = makeAppXml(
            "<ComObjects>"
            "<ComObject Id=\"co_sw\" Number=\"0\" Name=\"Switch\" DatapointType=\"1.001\""
            "  CommunicationFlag=\"C,W,T\"/>"
            "</ComObjects>");

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KnxprodCatalog cat = catalogFromZip(buildKnxprod(kHwXmlEts6, appXml), dir);

        QCOMPARE(cat.count(), 1);
        const KnxApplicationProgram *app = cat.at(0)->appProgram.get();
        QVERIFY(app != nullptr);
        QVERIFY(!app->comObjects.isEmpty());
        QCOMPARE(app->comObjects[0].id,     QStringLiteral("co_sw"));
        QCOMPARE(app->comObjects[0].number, 0);
        QCOMPARE(app->comObjects[0].dpt,    QStringLiteral("1.001"));
        QVERIFY(app->comObjects[0].flags.contains(QStringLiteral("W")));
    }

    void catalogLookupByProductRef()
    {
        const QByteArray appXml = makeAppXml(
            "<ComObjects>"
            "<ComObject Id=\"co1\" Number=\"0\" Name=\"Out\" DatapointType=\"1.001\""
            "  CommunicationFlag=\"C,W\"/>"
            "</ComObjects>");

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KnxprodCatalog cat = catalogFromZip(buildKnxprod(kHwXmlEts6, appXml), dir);

        QCOMPARE(cat.count(), 1);
        auto appProg = cat.sharedByProductRef(QStringLiteral("M-00FA_H-1234_HP-1234"));
        QVERIFY(appProg != nullptr);
        QVERIFY(appProg->isValid());
    }

    // ── H1: Hardware2ProgrammeVersion ─────────────────────────────────────────

    void hardware2ProgrammeVersionParsed()
    {
        // Confirm that ETS6-style Hardware XML is loaded correctly and that the
        // appProgramRefId is resolved from ApplicationProgramRefId attribute.
        const QByteArray appXml = makeAppXml(
            "<ComObjects>"
            "<ComObject Id=\"co1\" Number=\"0\" Name=\"Out\" DatapointType=\"1.001\""
            "  CommunicationFlag=\"C,W\"/>"
            "</ComObjects>");

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KnxprodCatalog cat = catalogFromZip(buildKnxprod(kHwXmlEts6, appXml), dir);

        QCOMPARE(cat.count(), 1);
        QCOMPARE(cat.at(0)->appProgramRefId, QStringLiteral("M-00FA_A-1234-0001"));
    }

    // ── H3: ComObjectRef merges overrides ─────────────────────────────────────

    void comObjectRefMergesNameAndDpt()
    {
        // Base ComObject has minimal info; ComObjectRef overrides Name and DPT.
        const QByteArray appXml = makeAppXml(
            "<ComObjectTable/>"
            "<ComObjects>"
            "<ComObject Id=\"co_base\" Number=\"0\" Name=\"BaseOut\" DatapointType=\"DPT-1\""
            "  CommunicationObjectEnable=\"Enabled\" WriteFlag=\"Enabled\"/>"
            "</ComObjects>"
            "<ComObjectRefs>"
            "<ComObjectRef Id=\"cor_0\" RefId=\"co_base\" Name=\"DisplayOut\" DatapointType=\"DPST-1-1\"/>"
            "</ComObjectRefs>");

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KnxprodCatalog cat = catalogFromZip(buildKnxprod(kHwXmlEts6, appXml), dir);

        QCOMPARE(cat.count(), 1);
        const KnxApplicationProgram *app = cat.at(0)->appProgram.get();
        QVERIFY(app != nullptr);
        QCOMPARE(app->comObjects.size(), 1);

        // ComObjectRef overrides Name and DPT; id stays as the base ComObject id.
        QCOMPARE(app->comObjects[0].id,   QStringLiteral("co_base"));
        QCOMPARE(app->comObjects[0].name, QStringLiteral("DisplayOut"));
        QCOMPARE(app->comObjects[0].dpt,  QStringLiteral("1.001"));   // DPST-1-1 normalised
    }

    void comObjectRefKeepsBaseFlagsWhenNoOverride()
    {
        // ComObjectRef without flag attributes → base flags are preserved.
        const QByteArray appXml = makeAppXml(
            "<ComObjects>"
            "<ComObject Id=\"co_base\" Number=\"0\" Name=\"Base\" DatapointType=\"1.001\""
            "  CommunicationFlag=\"C,W,T\"/>"
            "</ComObjects>"
            "<ComObjectRefs>"
            "<ComObjectRef Id=\"cor_0\" RefId=\"co_base\" Name=\"Display\"/>"
            "</ComObjectRefs>");

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KnxprodCatalog cat = catalogFromZip(buildKnxprod(kHwXmlEts6, appXml), dir);

        QCOMPARE(cat.count(), 1);
        const KnxComObject &co = cat.at(0)->appProgram->comObjects[0];
        QVERIFY(co.flags.contains(QStringLiteral("W")));
        QVERIFY(co.flags.contains(QStringLiteral("T")));
    }

    // ── E1 existing tests (kept from before) ──────────────────────────────────

    void loadsComObjectWithIndividualFlags()
    {
        const QByteArray appXml = makeAppXml(
            "<ComObjectTable/>"
            "<ComObjects>"
            "<ComObject Id=\"co_out\" Number=\"0\" Name=\"Output\" DatapointType=\"1.001\""
            "  CommunicationObjectEnable=\"Enabled\" WriteFlag=\"Enabled\""
            "  TransmitFlag=\"Enabled\" ReadFlag=\"Disabled\" UpdateFlag=\"Disabled\"/>"
            "</ComObjects>");

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KnxprodCatalog cat = catalogFromZip(buildKnxprod(kHwXmlLegacy, appXml), dir);

        QCOMPARE(cat.count(), 1);
        const KnxApplicationProgram *app = cat.at(0)->appProgram.get();
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
        const QByteArray appXml = makeAppXml(
            "<ComObjects>"
            "<ComObject Id=\"co1\" Number=\"0\" Name=\"Switch\" DatapointType=\"DPST-1-1\""
            "  CommunicationFlag=\"C,W\"/>"
            "<ComObject Id=\"co2\" Number=\"1\" Name=\"Dimmer\" DatapointType=\"DPT-5\""
            "  CommunicationFlag=\"C,W\"/>"
            "</ComObjects>");

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KnxprodCatalog cat = catalogFromZip(buildKnxprod(kHwXmlLegacy, appXml), dir);

        QCOMPARE(cat.count(), 1);
        const KnxApplicationProgram *app = cat.at(0)->appProgram.get();
        QVERIFY(app != nullptr);
        QCOMPARE(app->comObjects.size(), 2);
        QCOMPARE(app->comObjects[0].dpt, QStringLiteral("1.001"));
        QCOMPARE(app->comObjects[1].dpt, QStringLiteral("DPT-5"));
    }

    void toleratesUnknownParameterType()
    {
        const QByteArray appXml = makeAppXml(
            "<ParameterTypes>"
            "<ParameterType Id=\"PT_Float\">"
            "<TypeFloat Encoding=\"F16\" minInclusive=\"-100\" maxInclusive=\"100\"/>"
            "</ParameterType>"
            "</ParameterTypes>"
            "<Parameters>"
            "<Parameter Id=\"p1\" Name=\"TempOffset\" ParameterType=\"PT_Float\" Offset=\"0\">"
            "<Value>0</Value>"
            "</Parameter>"
            "</Parameters>");

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KnxprodCatalog cat = catalogFromZip(buildKnxprod(kHwXmlLegacy, appXml), dir);

        QCOMPARE(cat.count(), 1);
        const KnxApplicationProgram *app = cat.at(0)->appProgram.get();
        QVERIFY(app != nullptr);
        QVERIFY(!app->paramTypes.isEmpty());
        const KnxParameterType *pt = app->findType(QStringLiteral("PT_Float"));
        QVERIFY(pt != nullptr);
        QCOMPARE(pt->kind, KnxParameterType::Kind::UInt);
        QCOMPARE(pt->size, uint8_t(4));
    }

    // ── Multi-product loading ─────────────────────────────────────────────────

    void loadsMultipleProductsFromOneFile()
    {
        // Two products sharing one application program.
        const QByteArray hwXml =
            "<KNX><ManufacturerData><Manufacturer RefId=\"M-00FA\">"
            "<Hardware>"
            "<Hardware Id=\"M-00FA_H-0001\" Name=\"Dev1\">"
            "<Products><Product Id=\"M-00FA_H-0001_HP-0001\" Text=\"Device 1\"/></Products>"
            "<Hardware2Programs><Hardware2Program>"
            "<Hardware2ProgrammeVersion ApplicationProgramRefId=\"M-00FA_A-0001-0001\"/>"
            "</Hardware2Program></Hardware2Programs>"
            "</Hardware>"
            "<Hardware Id=\"M-00FA_H-0002\" Name=\"Dev2\">"
            "<Products><Product Id=\"M-00FA_H-0002_HP-0002\" Text=\"Device 2\"/></Products>"
            "<Hardware2Programs><Hardware2Program>"
            "<Hardware2ProgrammeVersion ApplicationProgramRefId=\"M-00FA_A-0001-0001\"/>"
            "</Hardware2Program></Hardware2Programs>"
            "</Hardware>"
            "</Hardware>"
            "</Manufacturer></ManufacturerData></KNX>";

        const QByteArray appXml =
            "<KNX><ManufacturerData><Manufacturer RefId=\"M-00FA\">"
            "<ApplicationPrograms>"
            "<ApplicationProgram Id=\"M-00FA_A-0001-0001\" Name=\"SharedApp\">"
            "<Static>"
            "<ComObjects>"
            "<ComObject Id=\"co1\" Number=\"0\" Name=\"Out\" DatapointType=\"1.001\""
            "  CommunicationFlag=\"C,W\"/>"
            "</ComObjects>"
            "</Static>"
            "</ApplicationProgram>"
            "</ApplicationPrograms>"
            "</Manufacturer></ManufacturerData></KNX>";

        const QByteArray zip = ZipUtils::buildZip({
            { QStringLiteral("M-00FA/M-00FA_H-0001_HP-0001.xml"), hwXml },
            { QStringLiteral("M-00FA/M-00FA_A-0001-0001.xml"),    appXml },
        });

        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        // Write two separate _HP_ files to simulate a multi-product package.
        // For simplicity, reuse the same hwXml content in one zip — both products
        // are in the single hwXml file and the parser must handle both.
        KnxprodCatalog cat = catalogFromZip(zip, dir);

        // Both hardware entries in hwXml reference the same appProgram — 2 products expected.
        QCOMPARE(cat.count(), 2);
        QCOMPARE(cat.at(0)->productId, QStringLiteral("M-00FA_H-0001_HP-0001"));
        QCOMPARE(cat.at(1)->productId, QStringLiteral("M-00FA_H-0002_HP-0002"));
    }
};

QTEST_MAIN(TestKnxprodParser)
#include "test_knxprod_parser.moc"
