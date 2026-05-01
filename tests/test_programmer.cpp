#include <QtTest>
#include "DeviceProgrammer.h"
#include "IKnxInterface.h"
#include "CemiFrame.h"
#include "DeviceInstance.h"
#include "KnxApplicationProgram.h"
#include "ComObjectLink.h"
#include "GroupAddress.h"

// ─── Mock KNX interface ───────────────────────────────────────────────────────

class MockInterface : public IKnxInterface
{
    Q_OBJECT
public:
    explicit MockInterface(QObject *parent = nullptr) : IKnxInterface(parent)
    { m_connected = true; }

    bool connectToInterface() override    { return true; }
    void disconnectFromInterface() override {}
    bool isConnected() const override     { return m_connected; }

    void sendCemiFrame(const QByteArray &cemi) override
    { m_sent.append(cemi); }

    void injectFrame(const QByteArray &cemi)
    { emit cemiFrameReceived(cemi); }

    QList<QByteArray> m_sent;
    bool              m_connected = true;
};

// ─── Helper: build a minimal A_Memory_Response frame ─────────────────────────

// Produces a fake A_Memory_Response CEMI from physAddr back to us,
// reporting `payload` bytes starting at `memAddr`.
static QByteArray fakeMemoryResponse(const QString &physAddr, uint16_t memAddr,
                                      const QByteArray &payload)
{
    CemiFrame resp;
    resp.messageCode  = CemiFrame::MessageCode::LDataInd;
    resp.sourceAddress = CemiFrame::physAddrFromString(physAddr);
    resp.destAddress  = 0x0000;
    resp.groupAddress = false;
    const uint8_t cnt = static_cast<uint8_t>(qMin<int>(payload.size(), 12));
    resp.apdu.append(char(0x42));
    resp.apdu.append(static_cast<char>(0x40 | cnt));
    resp.apdu.append(static_cast<char>(memAddr >> 8));
    resp.apdu.append(static_cast<char>(memAddr & 0xFF));
    resp.apdu.append(payload.left(cnt));
    return resp.toBytes();
}

// ─── Helpers: build test fixtures ────────────────────────────────────────────

static KnxApplicationProgram makeApp()
{
    KnxApplicationProgram app;
    app.id   = QStringLiteral("test-app");
    app.name = QStringLiteral("Test App");

    KnxParameterType t; t.id = QStringLiteral("uint8");
    t.kind = KnxParameterType::Kind::UInt; t.size = 1;
    app.paramTypes.insert(t.id, t);

    KnxParameter p; p.id = QStringLiteral("p1");
    p.typeId = QStringLiteral("uint8"); p.offset = 0; p.defaultValue = QVariant(7);
    app.parameters << p;

    KnxComObject co; co.id = QStringLiteral("co1"); co.number = 0;
    app.comObjects << co;

    app.memoryLayout.addressTable     = 0x0116;
    app.memoryLayout.associationTable = 0x011A;
    app.memoryLayout.parameterBase    = 0x4000;
    app.memoryLayout.parameterSize    = 2;
    return app;
}

static DeviceInstance makeDevice(const KnxApplicationProgram &app)
{
    DeviceInstance dev(QStringLiteral("d1"), app.id, app.id);
    dev.setPhysicalAddress(QStringLiteral("1.1.1"));
    dev.parameters()[QStringLiteral("p1")] = QVariant(42);

    ComObjectLink l; l.comObjectId = QStringLiteral("co1");
    l.ga = GroupAddress(0, 0, 1, QStringLiteral("Licht"), QStringLiteral("1.001"));
    dev.addLink(l);
    return dev;
}

// ─── Tests ───────────────────────────────────────────────────────────────────

class TestProgrammer : public QObject
{
    Q_OBJECT

private slots:

    void failsWhenNotConnected()
    {
        MockInterface iface;
        iface.m_connected = false;
        KnxApplicationProgram app = makeApp();
        DeviceInstance dev        = makeDevice(app);

        DeviceProgrammer prog(&iface, &dev, &app);

        QSignalSpy spy(&prog, &DeviceProgrammer::finished);
        prog.start();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), false);
        QVERIFY(iface.m_sent.isEmpty());
    }

    void sendsPaWriteAfterProgModeWait()
    {
        MockInterface iface;
        KnxApplicationProgram app = makeApp();
        DeviceInstance dev        = makeDevice(app);

        DeviceProgrammer prog(&iface, &dev, &app);
        prog.setProgModeTimeout(10);

        prog.start();
        QTRY_VERIFY_WITH_TIMEOUT(!iface.m_sent.isEmpty(), 500);

        // First frame must be an A_IndividualAddress_Write (APCI 0x0C0)
        const CemiFrame f = CemiFrame::fromBytes(iface.m_sent.first());
        QCOMPARE(f.apci(), uint16_t(0x0C0));
        QCOMPARE(f.destAddress, uint16_t(0x0000)); // broadcast
    }

    void sendsMemoryWritesAndSucceedsWithCorrectResponse()
    {
        MockInterface iface;
        KnxApplicationProgram app = makeApp();
        DeviceInstance dev        = makeDevice(app);

        DeviceProgrammer prog(&iface, &dev, &app);
        prog.setProgModeTimeout(10);

        // Inject a correct memory response when the verify step begins
        connect(&prog, &DeviceProgrammer::stepStarted,
                [&](int step, const QString &) {
            if (step == DeviceProgrammer::StepVerifyParameters) {
                QTimer::singleShot(50, [&]() {
                    // Expected: paramBlock = [42, 0]
                    iface.injectFrame(fakeMemoryResponse(
                        QStringLiteral("1.1.1"), 0x4000,
                        QByteArray::fromHex("2A00")));
                });
            }
        });

        QSignalSpy doneSpy(&prog, &DeviceProgrammer::finished);
        prog.start();
        QTRY_VERIFY_WITH_TIMEOUT(doneSpy.count() > 0, 8000);

        QCOMPARE(doneSpy.at(0).at(0).toBool(), true);

        // Must have sent: PA_Write + at least address/assoc/param memory writes
        QVERIFY(iface.m_sent.size() >= 4);
        bool hasMemWrite = false;
        for (const QByteArray &raw : iface.m_sent) {
            const CemiFrame f = CemiFrame::fromBytes(raw);
            if (!f.groupAddress && (f.apci() & 0x3C0) == 0x280) {
                hasMemWrite = true;
                break;
            }
        }
        QVERIFY(hasMemWrite);
    }

    void verifyStepFailsOnMismatch()
    {
        MockInterface iface;
        KnxApplicationProgram app = makeApp();
        DeviceInstance dev        = makeDevice(app);

        DeviceProgrammer prog(&iface, &dev, &app);
        prog.setProgModeTimeout(10);

        // Inject a WRONG memory response during the verify step
        connect(&prog, &DeviceProgrammer::stepStarted,
                [&](int step, const QString &) {
            if (step == DeviceProgrammer::StepVerifyParameters) {
                QTimer::singleShot(50, [&]() {
                    // Wrong data: device returns [0xFF, 0x00] but we wrote [42, 0]
                    iface.injectFrame(fakeMemoryResponse(
                        QStringLiteral("1.1.1"), 0x4000,
                        QByteArray::fromHex("FF00")));
                });
            }
        });

        QSignalSpy doneSpy(&prog, &DeviceProgrammer::finished);
        prog.start();
        QTRY_VERIFY_WITH_TIMEOUT(doneSpy.count() > 0, 8000);

        QCOMPARE(doneSpy.at(0).at(0).toBool(), false);
        QVERIFY(doneSpy.at(0).at(1).toString().contains(QStringLiteral("Verifikation")));
    }

    void verifyTimeoutIsNonFatal()
    {
        // No memory response is ever injected → verify step times out → programming
        // should still complete successfully (non-fatal warning)
        MockInterface iface;
        KnxApplicationProgram app = makeApp();
        DeviceInstance dev        = makeDevice(app);

        DeviceProgrammer prog(&iface, &dev, &app);
        prog.setProgModeTimeout(10);

        QSignalSpy doneSpy(&prog, &DeviceProgrammer::finished);
        prog.start();
        // Must finish (with success, just no verify) within reasonable time
        // Sequence: ~10 + 400×4steps + 2000 verify timeout + 100 + 1000 ≈ 5s
        QTRY_VERIFY_WITH_TIMEOUT(doneSpy.count() > 0, 8000);

        QCOMPARE(doneSpy.at(0).at(0).toBool(), true);
    }

    void cancelStopsProgramming()
    {
        MockInterface iface;
        KnxApplicationProgram app = makeApp();
        DeviceInstance dev        = makeDevice(app);

        DeviceProgrammer prog(&iface, &dev, &app);
        prog.setProgModeTimeout(500); // long enough to cancel before it fires

        QSignalSpy doneSpy(&prog, &DeviceProgrammer::finished);
        prog.start();
        prog.cancel();

        QCOMPARE(doneSpy.count(), 1);
        QCOMPARE(doneSpy.at(0).at(0).toBool(), false);
        QVERIFY(doneSpy.at(0).at(1).toString().contains(tr("abgebrochen")));
    }

    void chunksLargeParameterBlock()
    {
        // 30-byte parameter block → needs ceil(30/12) = 3 write chunks
        KnxApplicationProgram app = makeApp();
        app.parameters.clear();
        KnxParameterType t; t.id = QStringLiteral("uint8");
        t.kind = KnxParameterType::Kind::UInt; t.size = 1;
        app.paramTypes.insert(t.id, t);
        for (int i = 0; i < 30; ++i) {
            KnxParameter p;
            p.id     = QStringLiteral("p%1").arg(i);
            p.typeId = QStringLiteral("uint8");
            p.offset = static_cast<uint32_t>(i);
            app.parameters << p;
        }
        app.memoryLayout.parameterSize = 30;

        DeviceInstance dev = makeDevice(app);

        MockInterface iface;
        DeviceProgrammer prog(&iface, &dev, &app);
        prog.setProgModeTimeout(10);

        QSignalSpy doneSpy(&prog, &DeviceProgrammer::finished);
        prog.start();
        // Let verify time out (non-fatal) — total ≈ 10 + 400×(1+1+3) + 2000 + 100 + 1000 ≈ 5s
        QTRY_VERIFY_WITH_TIMEOUT(doneSpy.count() > 0, 10000);

        QCOMPARE(doneSpy.at(0).at(0).toBool(), true);

        // Count A_Memory_Write frames
        int memWriteCount = 0;
        for (const QByteArray &raw : iface.m_sent) {
            const CemiFrame f = CemiFrame::fromBytes(raw);
            if (!f.groupAddress && (f.apci() & 0x3C0) == 0x280)
                ++memWriteCount;
        }
        // 3 parameter chunks + at least 1 address table + 1 association table chunk
        QVERIFY2(memWriteCount >= 5,
                 qPrintable(QStringLiteral("Expected ≥ 5 Memory_Writes, got %1").arg(memWriteCount)));
    }
};

QTEST_MAIN(TestProgrammer)
#include "test_programmer.moc"
