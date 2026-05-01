#include <QtTest>
#include "TableBuilder.h"
#include "DeviceInstance.h"
#include "ComObjectLink.h"
#include "GroupAddress.h"
#include "KnxApplicationProgram.h"

static KnxApplicationProgram makeApp(const QString &id)
{
    KnxApplicationProgram app;
    app.id   = id;
    app.name = id;
    return app;
}

class TestTableBuilder : public QObject
{
    Q_OBJECT

private slots:

    // ── Existing baseline tests ───────────────────────────────────────────────

    void buildsAddressTableWithPaAndGas()
    {
        KnxApplicationProgram app = makeApp(QStringLiteral("dev"));
        KnxComObject co1; co1.id = QStringLiteral("co1"); co1.number = 0;
        KnxComObject co2; co2.id = QStringLiteral("co2"); co2.number = 1;
        app.comObjects << co1 << co2;
        app.memoryLayout.parameterSize = 0;

        DeviceInstance dev(QStringLiteral("d1"), QStringLiteral("dev"), QStringLiteral("dev"));
        dev.setPhysicalAddress(QStringLiteral("1.1.5"));

        ComObjectLink l1; l1.comObjectId = QStringLiteral("co1");
        l1.ga = GroupAddress(0, 0, 1, QStringLiteral("GA1"), QStringLiteral("1.001"));
        dev.addLink(l1);
        ComObjectLink l2; l2.comObjectId = QStringLiteral("co2");
        l2.ga = GroupAddress(0, 0, 2, QStringLiteral("GA2"), QStringLiteral("1.001"));
        dev.addLink(l2);

        DeviceMemoryImage img = TableBuilder::build(dev, app);

        QCOMPARE(img.addressTable.size(), 2 + 2 * 3);  // 2 header + 3 entries * 2 bytes
        QCOMPARE(static_cast<uint8_t>(img.addressTable[1]), uint8_t(3)); // 1 PA + 2 GAs
        QCOMPARE(static_cast<uint8_t>(img.addressTable[2]), uint8_t(0x11));
        QCOMPARE(static_cast<uint8_t>(img.addressTable[3]), uint8_t(0x05));
    }

    void buildsAssociationTableEntries()
    {
        KnxApplicationProgram app = makeApp(QStringLiteral("dev"));
        KnxComObject co; co.id = QStringLiteral("co1"); co.number = 7;
        app.comObjects << co;
        app.memoryLayout.parameterSize = 0;

        DeviceInstance dev(QStringLiteral("d1"), QStringLiteral("dev"), QStringLiteral("dev"));
        dev.setPhysicalAddress(QStringLiteral("1.1.1"));
        ComObjectLink l; l.comObjectId = QStringLiteral("co1");
        l.ga = GroupAddress(0, 0, 1);
        dev.addLink(l);

        DeviceMemoryImage img = TableBuilder::build(dev, app);

        QCOMPARE(img.associationTable.size(), 4);
        QCOMPARE(static_cast<uint8_t>(img.associationTable[1]), uint8_t(1));
        QCOMPARE(static_cast<uint8_t>(img.associationTable[2]), uint8_t(0)); // ga_idx
        QCOMPARE(static_cast<uint8_t>(img.associationTable[3]), uint8_t(7)); // co.number
    }

    void packsParameterValuesAtDeclaredOffset()
    {
        KnxApplicationProgram app = makeApp(QStringLiteral("dev"));
        KnxParameterType t1; t1.id = QStringLiteral("uint16");
        t1.kind = KnxParameterType::Kind::UInt; t1.size = 2;
        KnxParameterType t2; t2.id = QStringLiteral("uint8");
        t2.kind = KnxParameterType::Kind::UInt; t2.size = 1;
        app.paramTypes.insert(t1.id, t1);
        app.paramTypes.insert(t2.id, t2);

        KnxParameter p1; p1.id = QStringLiteral("p1"); p1.typeId = QStringLiteral("uint16"); p1.offset = 0;
        KnxParameter p2; p2.id = QStringLiteral("p2"); p2.typeId = QStringLiteral("uint8");  p2.offset = 2;
        app.parameters << p1 << p2;
        app.memoryLayout.parameterSize = 4;

        DeviceInstance dev(QStringLiteral("d1"), QStringLiteral("dev"), QStringLiteral("dev"));
        dev.parameters()[QStringLiteral("p1")] = 0x1234;
        dev.parameters()[QStringLiteral("p2")] = 0xAB;

        DeviceMemoryImage img = TableBuilder::build(dev, app);
        QCOMPARE(img.parameterBlock.size(), 4);
        QCOMPARE(static_cast<uint8_t>(img.parameterBlock[0]), uint8_t(0x34)); // LE
        QCOMPARE(static_cast<uint8_t>(img.parameterBlock[1]), uint8_t(0x12));
        QCOMPARE(static_cast<uint8_t>(img.parameterBlock[2]), uint8_t(0xAB));
        QCOMPARE(static_cast<uint8_t>(img.parameterBlock[3]), uint8_t(0x00));
    }

    // ── New safety / edge-case tests ──────────────────────────────────────────

    void outOfBoundsParameterIsSkipped()
    {
        // Parameter at offset 10 with size 2 in a 4-byte block → must be ignored
        KnxApplicationProgram app = makeApp(QStringLiteral("dev"));
        KnxParameterType t; t.id = QStringLiteral("uint16");
        t.kind = KnxParameterType::Kind::UInt; t.size = 2;
        app.paramTypes.insert(t.id, t);
        KnxParameter p; p.id = QStringLiteral("p1"); p.typeId = QStringLiteral("uint16");
        p.offset = 10; // past end of block
        app.parameters << p;
        app.memoryLayout.parameterSize = 4;

        DeviceInstance dev(QStringLiteral("d1"), QStringLiteral("dev"), QStringLiteral("dev"));
        dev.parameters()[QStringLiteral("p1")] = 0xDEAD;

        DeviceMemoryImage img = TableBuilder::build(dev, app);
        QCOMPARE(img.parameterBlock.size(), 4);
        // Block must remain all-zero (write was skipped)
        QCOMPARE(img.parameterBlock, QByteArray(4, char(0)));
    }

    void uint8OverflowIsTruncated()
    {
        // A uint8 parameter written with value 0x1FF should be stored as 0xFF
        KnxApplicationProgram app = makeApp(QStringLiteral("dev"));
        KnxParameterType t; t.id = QStringLiteral("uint8");
        t.kind = KnxParameterType::Kind::UInt; t.size = 1;
        app.paramTypes.insert(t.id, t);
        KnxParameter p; p.id = QStringLiteral("p"); p.typeId = QStringLiteral("uint8");
        p.offset = 0;
        app.parameters << p;
        app.memoryLayout.parameterSize = 2;

        DeviceInstance dev(QStringLiteral("d1"), QStringLiteral("dev"), QStringLiteral("dev"));
        dev.parameters()[QStringLiteral("p")] = 0x1FF; // overflows uint8

        DeviceMemoryImage img = TableBuilder::build(dev, app);
        QCOMPARE(img.parameterBlock.size(), 2);
        QCOMPARE(static_cast<uint8_t>(img.parameterBlock[0]), uint8_t(0xFF));
        QCOMPARE(static_cast<uint8_t>(img.parameterBlock[1]), uint8_t(0x00));
    }

    void duplicateGaMappedOnce()
    {
        // Two ComObject links to the same GA → only one entry in address table
        KnxApplicationProgram app = makeApp(QStringLiteral("dev"));
        KnxComObject co1; co1.id = QStringLiteral("co1"); co1.number = 0;
        KnxComObject co2; co2.id = QStringLiteral("co2"); co2.number = 1;
        app.comObjects << co1 << co2;
        app.memoryLayout.parameterSize = 0;

        DeviceInstance dev(QStringLiteral("d1"), QStringLiteral("dev"), QStringLiteral("dev"));
        dev.setPhysicalAddress(QStringLiteral("1.1.1"));

        const GroupAddress sharedGa(0, 0, 5, QStringLiteral("Licht"), QStringLiteral("1.001"));
        ComObjectLink l1; l1.comObjectId = QStringLiteral("co1"); l1.ga = sharedGa;
        ComObjectLink l2; l2.comObjectId = QStringLiteral("co2"); l2.ga = sharedGa;
        dev.addLink(l1);
        dev.addLink(l2);

        DeviceMemoryImage img = TableBuilder::build(dev, app);

        // Address table: count=2 (1 PA + 1 unique GA), PA, GA
        QCOMPARE(img.addressTable.size(), 2 + 2 * 2);
        QCOMPARE(static_cast<uint8_t>(img.addressTable[1]), uint8_t(2));

        // Association table: 2 entries (both COs mapped to same GA index 0)
        QCOMPARE(img.associationTable.size(), 2 + 4); // count(2) + 2×(ga_idx+co_num)
        QCOMPARE(static_cast<uint8_t>(img.associationTable[2]), uint8_t(0)); // ga_idx = 0
        QCOMPARE(static_cast<uint8_t>(img.associationTable[4]), uint8_t(0)); // ga_idx = 0 again
    }

    void largeParameterBlockProducesCorrectSize()
    {
        // 100-byte parameter block with a single uint32 at offset 96
        KnxApplicationProgram app = makeApp(QStringLiteral("dev"));
        KnxParameterType t; t.id = QStringLiteral("uint32");
        t.kind = KnxParameterType::Kind::UInt; t.size = 4;
        app.paramTypes.insert(t.id, t);
        KnxParameter p; p.id = QStringLiteral("p"); p.typeId = QStringLiteral("uint32");
        p.offset = 96;
        app.parameters << p;
        app.memoryLayout.parameterSize = 100;

        DeviceInstance dev(QStringLiteral("d1"), QStringLiteral("dev"), QStringLiteral("dev"));
        dev.parameters()[QStringLiteral("p")] = static_cast<int>(0xDEADBEEF);

        DeviceMemoryImage img = TableBuilder::build(dev, app);
        QCOMPARE(img.parameterBlock.size(), 100);
        // Little-endian 0xDEADBEEF at offset 96
        QCOMPARE(static_cast<uint8_t>(img.parameterBlock[96]), uint8_t(0xEF));
        QCOMPARE(static_cast<uint8_t>(img.parameterBlock[97]), uint8_t(0xBE));
        QCOMPARE(static_cast<uint8_t>(img.parameterBlock[98]), uint8_t(0xAD));
        QCOMPARE(static_cast<uint8_t>(img.parameterBlock[99]), uint8_t(0xDE));
    }

    void noLinksProducesMinimalTables()
    {
        KnxApplicationProgram app = makeApp(QStringLiteral("dev"));
        app.memoryLayout.parameterSize = 0;

        DeviceInstance dev(QStringLiteral("d1"), QStringLiteral("dev"), QStringLiteral("dev"));
        dev.setPhysicalAddress(QStringLiteral("1.1.1"));

        DeviceMemoryImage img = TableBuilder::build(dev, app);

        // Address table: count=1 (PA only), PA
        QCOMPARE(img.addressTable.size(), 4);
        QCOMPARE(static_cast<uint8_t>(img.addressTable[1]), uint8_t(1));

        // Association table: count=0
        QCOMPARE(img.associationTable.size(), 2);
        QCOMPARE(static_cast<uint8_t>(img.associationTable[1]), uint8_t(0));

        QCOMPARE(img.parameterBlock.size(), 0);
    }

    void defaultValuesUsedWhenDeviceParamsMissing()
    {
        KnxApplicationProgram app = makeApp(QStringLiteral("dev"));
        KnxParameterType t; t.id = QStringLiteral("uint8");
        t.kind = KnxParameterType::Kind::UInt; t.size = 1;
        app.paramTypes.insert(t.id, t);
        KnxParameter p; p.id = QStringLiteral("p"); p.typeId = QStringLiteral("uint8");
        p.offset = 0; p.defaultValue = QVariant(42);
        app.parameters << p;
        app.memoryLayout.parameterSize = 2;

        DeviceInstance dev(QStringLiteral("d1"), QStringLiteral("dev"), QStringLiteral("dev"));
        // No explicit parameter value → default used

        DeviceMemoryImage img = TableBuilder::build(dev, app);
        QCOMPARE(static_cast<uint8_t>(img.parameterBlock[0]), uint8_t(42));
    }
};

QTEST_MAIN(TestTableBuilder)
#include "test_table_builder.moc"
