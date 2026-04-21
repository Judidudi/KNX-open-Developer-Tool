#include <QtTest>
#include "TableBuilder.h"
#include "DeviceInstance.h"
#include "ComObjectLink.h"
#include "GroupAddress.h"
#include "Manifest.h"

class TestTableBuilder : public QObject
{
    Q_OBJECT

private slots:
    void buildsAddressTableWithPaAndGas()
    {
        Manifest m;
        m.id      = QStringLiteral("dev");
        m.version = QStringLiteral("1.0.0");
        ManifestComObject co1; co1.id = QStringLiteral("co1"); co1.number = 0; co1.dpt = QStringLiteral("1.001");
        ManifestComObject co2; co2.id = QStringLiteral("co2"); co2.number = 1; co2.dpt = QStringLiteral("1.001");
        m.comObjects << co1 << co2;
        m.memoryLayout.parameterSize = 0;

        DeviceInstance dev(QStringLiteral("d1"), QStringLiteral("dev"), QStringLiteral("1.0.0"));
        dev.setPhysicalAddress(QStringLiteral("1.1.5"));

        ComObjectLink l1;
        l1.comObjectId = QStringLiteral("co1");
        l1.ga = GroupAddress(0, 0, 1, QStringLiteral("GA1"), QStringLiteral("1.001"));
        dev.addLink(l1);

        ComObjectLink l2;
        l2.comObjectId = QStringLiteral("co2");
        l2.ga = GroupAddress(0, 0, 2, QStringLiteral("GA2"), QStringLiteral("1.001"));
        dev.addLink(l2);

        DeviceMemoryImage img = TableBuilder::build(dev, m);

        // addressTable: [count_hi][count_lo] [PA] [GA1] [GA2]
        QCOMPARE(img.addressTable.size(), 2 + 2 * 3);   // 2 header + 3 entries * 2 bytes
        QCOMPARE(static_cast<uint8_t>(img.addressTable[1]), uint8_t(3));  // 1 PA + 2 GAs
        // PA 1.1.5 = 0x1105
        QCOMPARE(static_cast<uint8_t>(img.addressTable[2]), uint8_t(0x11));
        QCOMPARE(static_cast<uint8_t>(img.addressTable[3]), uint8_t(0x05));
    }

    void buildsAssociationTableEntries()
    {
        Manifest m;
        m.id = QStringLiteral("dev");
        m.version = QStringLiteral("1.0.0");
        ManifestComObject co; co.id = QStringLiteral("co1"); co.number = 7;
        m.comObjects << co;
        m.memoryLayout.parameterSize = 0;

        DeviceInstance dev(QStringLiteral("d1"), QStringLiteral("dev"), QStringLiteral("1.0.0"));
        dev.setPhysicalAddress(QStringLiteral("1.1.1"));

        ComObjectLink l;
        l.comObjectId = QStringLiteral("co1");
        l.ga = GroupAddress(0, 0, 1);
        dev.addLink(l);

        DeviceMemoryImage img = TableBuilder::build(dev, m);

        // associationTable: [count_hi][count_lo] [ga_idx][co_number]
        QCOMPARE(img.associationTable.size(), 4);
        QCOMPARE(static_cast<uint8_t>(img.associationTable[1]), uint8_t(1));
        QCOMPARE(static_cast<uint8_t>(img.associationTable[2]), uint8_t(0));  // first GA
        QCOMPARE(static_cast<uint8_t>(img.associationTable[3]), uint8_t(7));  // co.number
    }

    void packsParameterValuesAtDeclaredOffset()
    {
        Manifest m;
        m.id      = QStringLiteral("dev");
        m.version = QStringLiteral("1.0.0");
        ManifestParameter p1;
        p1.id = QStringLiteral("p1");
        p1.type = QStringLiteral("uint16");
        p1.memoryOffset = 0;
        p1.size = 2;
        ManifestParameter p2;
        p2.id = QStringLiteral("p2");
        p2.type = QStringLiteral("uint8");
        p2.memoryOffset = 2;
        p2.size = 1;
        m.parameters.push_back(p1);
        m.parameters.push_back(p2);
        m.memoryLayout.parameterSize = 4;

        DeviceInstance dev(QStringLiteral("d1"), QStringLiteral("dev"), QStringLiteral("1.0.0"));
        dev.parameters()[QStringLiteral("p1")] = 0x1234;
        dev.parameters()[QStringLiteral("p2")] = 0xAB;

        DeviceMemoryImage img = TableBuilder::build(dev, m);
        QCOMPARE(img.parameterBlock.size(), 4);
        // Little-endian uint16 at offset 0: 0x34 0x12
        QCOMPARE(static_cast<uint8_t>(img.parameterBlock[0]), uint8_t(0x34));
        QCOMPARE(static_cast<uint8_t>(img.parameterBlock[1]), uint8_t(0x12));
        QCOMPARE(static_cast<uint8_t>(img.parameterBlock[2]), uint8_t(0xAB));
        QCOMPARE(static_cast<uint8_t>(img.parameterBlock[3]), uint8_t(0x00));
    }
};

QTEST_MAIN(TestTableBuilder)
#include "test_table_builder.moc"
