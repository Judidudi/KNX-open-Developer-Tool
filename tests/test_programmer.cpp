#include <QtTest>
#include "DeviceProgrammer.h"
#include "IKnxInterface.h"
#include "CemiFrame.h"
#include "DeviceInstance.h"
#include "KnxApplicationProgram.h"
#include "ComObjectLink.h"
#include "GroupAddress.h"

// ─── Mock KNX interface ───────────────────────────────────────────────────────
//
// Auto-ACKs every T_Data_Connected frame we send (simulating a KNX device that
// is present and responding).  Supports injecting specific frames from the
// "device" side for prog-mode detection and memory responses.

class MockInterface : public IKnxInterface
{
    Q_OBJECT
public:
    explicit MockInterface(QObject *parent = nullptr) : IKnxInterface(parent) {}

    bool connectToInterface() override    { return true; }
    void disconnectFromInterface() override {}
    bool isConnected() const override     { return m_connected; }

    void sendCemiFrame(const QByteArray &cemi) override
    {
        m_sent.append(cemi);
        if (!m_autoAck) return;

        const CemiFrame f = CemiFrame::fromBytes(cemi);
        if (!f.groupAddress && f.isTDataConnected()) {
            // Queue a fake T_ACK from the device (source = our destination)
            const uint8_t seq = f.tSeqNumber();
            const uint16_t devPa = f.destAddress;
            QMetaObject::invokeMethod(this, [this, seq, devPa]() {
                CemiFrame ack;
                ack.messageCode   = CemiFrame::MessageCode::LDataInd;
                ack.sourceAddress = devPa;    // ACK comes FROM the device
                ack.destAddress   = 0x0000;
                ack.groupAddress  = false;
                ack.apdu.append(static_cast<char>(0xC2 | ((seq & 0x0F) << 2)));
                emit cemiFrameReceived(ack.toBytes());
            }, Qt::QueuedConnection);
        }
    }

    // Simulate a device in programming mode responding to IndividualAddress_Read.
    void injectProgModeResponse(const QString &physAddr)
    {
        CemiFrame resp;
        resp.messageCode   = CemiFrame::MessageCode::LDataInd;
        resp.sourceAddress = CemiFrame::physAddrFromString(physAddr);
        resp.destAddress   = 0x0000;
        resp.groupAddress  = true;            // broadcast response
        resp.apdu.append(char(0x01));         // TPCI=0, APCI[9:8]=01
        resp.apdu.append(char(0x40));         // APCI[7:0]=0x40  →  apci() = 0x140
        emit cemiFrameReceived(resp.toBytes());
    }

    // Simulate A_Memory_Response arriving as T_Data_Connected from device.
    void injectMemoryResponse(const QString &physAddr, uint16_t memAddr,
                               const QByteArray &payload)
    {
        CemiFrame resp;
        resp.messageCode   = CemiFrame::MessageCode::LDataInd;
        resp.sourceAddress = CemiFrame::physAddrFromString(physAddr);
        resp.destAddress   = 0x0000;
        resp.groupAddress  = false;
        const uint8_t cnt = static_cast<uint8_t>(qMin<int>(payload.size(), 12));
        resp.apdu.append(char(0x42));                        // T_Data_Connected[seq=0], APCI[9:8]=10
        resp.apdu.append(static_cast<char>(0x40 | cnt));     // Memory_Response, count
        resp.apdu.append(static_cast<char>(memAddr >> 8));
        resp.apdu.append(static_cast<char>(memAddr & 0xFF));
        resp.apdu.append(payload.left(cnt));
        emit cemiFrameReceived(resp.toBytes());
    }

    bool               m_autoAck   = true;
    bool               m_connected = true;
    QList<QByteArray>  m_sent;
};

// ─── Test fixtures ────────────────────────────────────────────────────────────

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

    void sendsIndividualAddressReadOnStart()
    {
        MockInterface iface;
        iface.m_autoAck = false;  // keep it simple, just check first frame
        KnxApplicationProgram app = makeApp();
        DeviceInstance dev        = makeDevice(app);

        DeviceProgrammer prog(&iface, &dev, &app);
        prog.setProgModeTimeout(5000);  // long timeout so we can inspect

        prog.start();

        // runStep() is queued — spin the event loop once
        QTRY_VERIFY_WITH_TIMEOUT(!iface.m_sent.isEmpty(), 200);

        const CemiFrame f = CemiFrame::fromBytes(iface.m_sent.first());
        QCOMPARE(f.apci(), uint16_t(0x100));   // A_IndividualAddress_Read
        QVERIFY(f.groupAddress);
        QCOMPARE(f.destAddress, uint16_t(0x0000));  // broadcast

        prog.cancel();
    }

    void failsWhenNoProgModeDeviceFound()
    {
        MockInterface iface;
        KnxApplicationProgram app = makeApp();
        DeviceInstance dev        = makeDevice(app);

        DeviceProgrammer prog(&iface, &dev, &app);
        prog.setProgModeTimeout(10);   // short timeout

        QSignalSpy doneSpy(&prog, &DeviceProgrammer::finished);
        prog.start();
        QTRY_VERIFY_WITH_TIMEOUT(doneSpy.count() > 0, 500);

        QCOMPARE(doneSpy.at(0).at(0).toBool(), false);
        QVERIFY(doneSpy.at(0).at(1).toString().contains(
            QStringLiteral("Programmiermodus")));
    }

    void writesPhysAddressAfterProgModeDetection()
    {
        MockInterface iface;
        iface.m_autoAck = false;  // suppress transport so we can inspect broadcast frames
        KnxApplicationProgram app = makeApp();
        DeviceInstance dev        = makeDevice(app);

        DeviceProgrammer prog(&iface, &dev, &app);
        prog.setProgModeTimeout(5000);

        prog.start();
        QTRY_VERIFY_WITH_TIMEOUT(!iface.m_sent.isEmpty(), 200);

        // Inject prog-mode response → programmer advances to WritePhysAddress
        iface.injectProgModeResponse(QStringLiteral("1.1.1"));

        QTRY_VERIFY_WITH_TIMEOUT(iface.m_sent.size() >= 2, 500);

        // Second frame = A_IndividualAddress_Write (APCI 0x0C0), broadcast
        const CemiFrame f = CemiFrame::fromBytes(iface.m_sent[1]);
        QCOMPARE(f.apci(), uint16_t(0x0C0));
        QCOMPARE(f.destAddress, uint16_t(0x0000));
        QVERIFY(f.groupAddress);

        prog.cancel();
    }

    void fullProgrammingSequenceSucceeds()
    {
        MockInterface iface;
        KnxApplicationProgram app = makeApp();
        DeviceInstance dev        = makeDevice(app);

        DeviceProgrammer prog(&iface, &dev, &app);
        prog.setProgModeTimeout(10);

        // Inject a correct memory response when verify begins
        connect(&prog, &DeviceProgrammer::stepStarted,
                [&](int step, const QString &) {
            if (step == DeviceProgrammer::StepVerifyParameters) {
                // Correct param data: p1=42 (0x2A), padded to 2 bytes
                QTimer::singleShot(20, [&]() {
                    iface.injectMemoryResponse(QStringLiteral("1.1.1"), 0x4000,
                                               QByteArray::fromHex("2A00"));
                });
            }
        });

        QSignalSpy doneSpy(&prog, &DeviceProgrammer::finished);

        // Simulate one prog-mode device as the first cemi response
        QTimer::singleShot(5, [&]() {
            iface.injectProgModeResponse(QStringLiteral("1.1.1"));
        });

        prog.start();
        QTRY_VERIFY_WITH_TIMEOUT(doneSpy.count() > 0, 8000);

        QCOMPARE(doneSpy.at(0).at(0).toBool(), true);

        // Must have sent IndividualAddressRead + IndividualAddressWrite + at least
        // one Memory_Write (address/association/parameter tables)
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

    void verifyFailsOnMismatch()
    {
        MockInterface iface;
        KnxApplicationProgram app = makeApp();
        DeviceInstance dev        = makeDevice(app);

        DeviceProgrammer prog(&iface, &dev, &app);
        prog.setProgModeTimeout(10);

        connect(&prog, &DeviceProgrammer::stepStarted,
                [&](int step, const QString &) {
            if (step == DeviceProgrammer::StepVerifyParameters) {
                QTimer::singleShot(20, [&]() {
                    // Wrong data: 0xFF instead of 0x2A
                    iface.injectMemoryResponse(QStringLiteral("1.1.1"), 0x4000,
                                               QByteArray::fromHex("FF00"));
                });
            }
        });

        QTimer::singleShot(5, [&]() {
            iface.injectProgModeResponse(QStringLiteral("1.1.1"));
        });

        QSignalSpy doneSpy(&prog, &DeviceProgrammer::finished);
        prog.start();
        QTRY_VERIFY_WITH_TIMEOUT(doneSpy.count() > 0, 8000);

        QCOMPARE(doneSpy.at(0).at(0).toBool(), false);
        QVERIFY(doneSpy.at(0).at(1).toString().contains(QStringLiteral("Verifikation")));
    }

    void verifyTimeoutIsNonFatal()
    {
        // No memory response injected → verify times out → programming still succeeds
        MockInterface iface;
        KnxApplicationProgram app = makeApp();
        DeviceInstance dev        = makeDevice(app);

        DeviceProgrammer prog(&iface, &dev, &app);
        prog.setProgModeTimeout(10);

        QTimer::singleShot(5, [&]() {
            iface.injectProgModeResponse(QStringLiteral("1.1.1"));
        });

        QSignalSpy doneSpy(&prog, &DeviceProgrammer::finished);
        prog.start();
        // ~10ms prog + 400ms PA settle + ~0ms transport steps + 2000ms verify timeout ≈ 2.5s
        QTRY_VERIFY_WITH_TIMEOUT(doneSpy.count() > 0, 8000);

        QCOMPARE(doneSpy.at(0).at(0).toBool(), true);
    }

    void cancelStopsProgramming()
    {
        MockInterface iface;
        KnxApplicationProgram app = makeApp();
        DeviceInstance dev        = makeDevice(app);

        DeviceProgrammer prog(&iface, &dev, &app);
        prog.setProgModeTimeout(5000);  // long — so it stays in WaitProgMode

        QSignalSpy doneSpy(&prog, &DeviceProgrammer::finished);
        prog.start();
        prog.cancel();

        QCOMPARE(doneSpy.count(), 1);
        QCOMPARE(doneSpy.at(0).at(0).toBool(), false);
        QVERIFY(doneSpy.at(0).at(1).toString().contains(tr("abgebrochen")));
    }

    void chunksLargeParameterBlock()
    {
        // 30-byte parameter block → ceil(30/12) = 3 Memory_Write chunks
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

        QTimer::singleShot(5, [&]() {
            iface.injectProgModeResponse(QStringLiteral("1.1.1"));
        });

        QSignalSpy doneSpy(&prog, &DeviceProgrammer::finished);
        prog.start();
        // Verify times out (no injection) → non-fatal → finishes in ~2.5s
        QTRY_VERIFY_WITH_TIMEOUT(doneSpy.count() > 0, 10000);

        QCOMPARE(doneSpy.at(0).at(0).toBool(), true);

        // Count A_Memory_Write frames (APCI 0x280..0x2BF)
        int memWriteCount = 0;
        for (const QByteArray &raw : iface.m_sent) {
            const CemiFrame f = CemiFrame::fromBytes(raw);
            if (!f.groupAddress && (f.apci() & 0x3C0) == 0x280)
                ++memWriteCount;
        }
        // 3 param chunks + at least 1 addr table chunk + 1 assoc table chunk = ≥ 5
        QVERIFY2(memWriteCount >= 5,
                 qPrintable(QStringLiteral("Expected ≥ 5 Memory_Writes, got %1")
                            .arg(memWriteCount)));
    }

    void noLoadStateMachineMode()
    {
        // When m_useLoadState=false, no PropertyValue_Write frames should appear
        MockInterface iface;
        KnxApplicationProgram app = makeApp();
        DeviceInstance dev        = makeDevice(app);

        DeviceProgrammer prog(&iface, &dev, &app);
        prog.setProgModeTimeout(10);
        prog.setLoadStateMachineEnabled(false);

        QTimer::singleShot(5, [&]() {
            iface.injectProgModeResponse(QStringLiteral("1.1.1"));
        });

        QSignalSpy doneSpy(&prog, &DeviceProgrammer::finished);
        prog.start();
        QTRY_VERIFY_WITH_TIMEOUT(doneSpy.count() > 0, 8000);
        QCOMPARE(doneSpy.at(0).at(0).toBool(), true);

        // No PropertyValue_Write frames (APCI 0x3D7)
        for (const QByteArray &raw : iface.m_sent) {
            const CemiFrame f = CemiFrame::fromBytes(raw);
            QVERIFY2(f.apci() != 0x3D7,
                     "PropertyValue_Write should not be sent in no-load-state mode");
        }
    }
};

QTEST_MAIN(TestProgrammer)
#include "test_programmer.moc"
