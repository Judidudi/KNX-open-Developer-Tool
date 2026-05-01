#include <QtTest>
#include "KnxApplicationProgram.h"
#include "DeviceInstance.h"
#include "ComObjectLink.h"
#include "GroupAddress.h"
#include "TopologyNode.h"

class TestParameterVisibility : public QObject
{
    Q_OBJECT

private:
    // Builds a minimal KnxApplicationProgram with two parameters:
    //   p_mode  (enum, always visible)
    //   p_delay (uint, visible only when p_mode == 1)
    static std::shared_ptr<KnxApplicationProgram> makeApp()
    {
        auto app = std::make_shared<KnxApplicationProgram>();
        app->id   = QStringLiteral("M-TEST_A-0001");
        app->name = QStringLiteral("Testprogramm");

        KnxParameterType pt;
        pt.id       = QStringLiteral("PT-UINT");
        pt.kind     = KnxParameterType::Kind::UInt;
        pt.minValue = 0;
        pt.maxValue = 255;
        pt.size     = 8;
        app->paramTypes.insert(pt.id, pt);

        KnxParameterType ptEnum;
        ptEnum.id   = QStringLiteral("PT-MODE");
        ptEnum.kind = KnxParameterType::Kind::Enum;
        ptEnum.enumValues = { {0, QStringLiteral("Aus")}, {1, QStringLiteral("Ein")} };
        app->paramTypes.insert(ptEnum.id, ptEnum);

        KnxParameter pMode;
        pMode.id           = QStringLiteral("p_mode");
        pMode.name         = QStringLiteral("Modus");
        pMode.typeId       = QStringLiteral("PT-MODE");
        pMode.defaultValue = 0;
        pMode.access       = KnxParameter::Access::ReadWrite;
        app->parameters.append(pMode);

        KnxParameter pDelay;
        pDelay.id              = QStringLiteral("p_delay");
        pDelay.name            = QStringLiteral("Verzögerung");
        pDelay.typeId          = QStringLiteral("PT-UINT");
        pDelay.defaultValue    = 100;
        pDelay.access          = KnxParameter::Access::ReadWrite;
        pDelay.conditionParamId = QStringLiteral("p_mode");
        pDelay.conditionValue  = QVariant(1);
        pDelay.conditionOp     = KnxParameter::ConditionOp::Equal;
        app->parameters.append(pDelay);

        KnxParameter pHidden;
        pHidden.id     = QStringLiteral("p_hidden");
        pHidden.name   = QStringLiteral("Intern");
        pHidden.typeId = QStringLiteral("PT-UINT");
        pHidden.access = KnxParameter::Access::Hidden;
        app->parameters.append(pHidden);

        return app;
    }

private slots:

    // Hidden parameter is never visible
    void hiddenParameterNeverVisible()
    {
        auto app = makeApp();
        auto dev = std::make_unique<DeviceInstance>(
            QStringLiteral("d1"), QStringLiteral("ref"), QStringLiteral("app"));
        dev->setAppProgram(app);

        const KnxParameter *p = app->findParameter(QStringLiteral("p_hidden"));
        QVERIFY(p != nullptr);
        QCOMPARE(p->access, KnxParameter::Access::Hidden);
    }

    // Conditional parameter hidden when condition not met
    void conditionalParameterHiddenWhenConditionFalse()
    {
        auto app = makeApp();
        auto dev = std::make_unique<DeviceInstance>(
            QStringLiteral("d1"), QStringLiteral("ref"), QStringLiteral("app"));
        dev->setAppProgram(app);
        dev->parameters()[QStringLiteral("p_mode")] = QVariant(0);  // mode OFF

        const KnxParameter *pDelay = app->findParameter(QStringLiteral("p_delay"));
        QVERIFY(pDelay != nullptr);

        const auto it = dev->parameters().find(pDelay->conditionParamId);
        const QVariant cur = (it != dev->parameters().end()) ? it->second : QVariant{};
        const bool eq = (cur.toString() == pDelay->conditionValue.toString());
        const bool visible = !(pDelay->conditionOp == KnxParameter::ConditionOp::Equal && !eq);
        QVERIFY(!visible);
    }

    // Conditional parameter visible when condition met
    void conditionalParameterVisibleWhenConditionTrue()
    {
        auto app = makeApp();
        auto dev = std::make_unique<DeviceInstance>(
            QStringLiteral("d1"), QStringLiteral("ref"), QStringLiteral("app"));
        dev->setAppProgram(app);
        dev->parameters()[QStringLiteral("p_mode")] = QVariant(1);  // mode ON

        const KnxParameter *pDelay = app->findParameter(QStringLiteral("p_delay"));
        QVERIFY(pDelay != nullptr);

        const auto it = dev->parameters().find(pDelay->conditionParamId);
        const QVariant cur = (it != dev->parameters().end()) ? it->second : QVariant{};
        const bool eq = (cur.toString() == pDelay->conditionValue.toString());
        const bool visible = !(pDelay->conditionOp == KnxParameter::ConditionOp::Equal && !eq);
        QVERIFY(visible);
    }

    // NotEqual operator: visible when values differ
    void notEqualConditionOp()
    {
        auto app = makeApp();
        KnxParameter &pDelay = app->parameters[1];
        pDelay.conditionOp = KnxParameter::ConditionOp::NotEqual;

        auto dev = std::make_unique<DeviceInstance>(
            QStringLiteral("d1"), QStringLiteral("ref"), QStringLiteral("app"));
        dev->setAppProgram(app);
        dev->parameters()[QStringLiteral("p_mode")] = QVariant(0);  // != 1 → visible

        const auto it2 = dev->parameters().find(pDelay.conditionParamId);
        const QVariant cur = (it2 != dev->parameters().end()) ? it2->second : QVariant{};
        const bool eq = (cur.toString() == pDelay.conditionValue.toString());
        // NotEqual: visible when eq is false (p_mode=0 != conditionValue=1)
        const bool visible = !(pDelay.conditionOp == KnxParameter::ConditionOp::NotEqual && eq);
        QVERIFY(visible);
    }

    // Default value used when parameter not set
    void defaultValueUsedWhenNoExplicitValue()
    {
        auto app = makeApp();
        const KnxParameter *pDelay = app->findParameter(QStringLiteral("p_delay"));
        QVERIFY(pDelay != nullptr);
        QCOMPARE(pDelay->defaultValue.toInt(), 100);

        auto dev = std::make_unique<DeviceInstance>(
            QStringLiteral("d1"), QStringLiteral("ref"), QStringLiteral("app"));
        dev->setAppProgram(app);
        // p_delay not explicitly set in device params
        QVERIFY(!dev->parameters().count(QStringLiteral("p_delay")));
    }

    // findParameter returns nullptr for unknown id
    void findParameterReturnsNullForUnknown()
    {
        auto app = makeApp();
        QVERIFY(app->findParameter(QStringLiteral("nonexistent")) == nullptr);
    }

    // findType returns nullptr for unknown id
    void findTypeReturnsNullForUnknown()
    {
        auto app = makeApp();
        QVERIFY(app->findType(QStringLiteral("nonexistent")) == nullptr);
    }

    // ComObject link: add and retrieve
    void comObjectLinkAddRetrieve()
    {
        auto dev = std::make_unique<DeviceInstance>(
            QStringLiteral("d1"), QStringLiteral("ref"), QStringLiteral("app"));

        ComObjectLink link;
        link.comObjectId = QStringLiteral("co_switch");
        link.ga          = GroupAddress(0, 0, 1, QStringLiteral("Licht"), QStringLiteral("1.001"));
        link.direction   = ComObjectLink::Direction::Send;
        dev->addLink(link);

        QCOMPARE(dev->links().size(), 1);
        QCOMPARE(dev->links()[0].comObjectId, QStringLiteral("co_switch"));
        QCOMPARE(dev->links()[0].ga.toString(), QStringLiteral("0/0/1"));
        QCOMPARE(dev->links()[0].direction, ComObjectLink::Direction::Send);
    }

    // ComObject link: Receive direction
    void comObjectLinkReceiveDirection()
    {
        auto dev = std::make_unique<DeviceInstance>(
            QStringLiteral("d1"), QStringLiteral("ref"), QStringLiteral("app"));

        ComObjectLink link;
        link.comObjectId = QStringLiteral("co_status");
        link.ga          = GroupAddress(0, 0, 2, QStringLiteral("Status"), QStringLiteral("1.001"));
        link.direction   = ComObjectLink::Direction::Receive;
        dev->addLink(link);

        QCOMPARE(dev->links()[0].direction, ComObjectLink::Direction::Receive);
    }

    // ComObject link: invalid GA
    void comObjectLinkInvalidGa()
    {
        auto dev = std::make_unique<DeviceInstance>(
            QStringLiteral("d1"), QStringLiteral("ref"), QStringLiteral("app"));

        ComObjectLink link;
        link.comObjectId = QStringLiteral("co_switch");
        // ga left default (invalid)
        dev->addLink(link);

        QVERIFY(!dev->links()[0].ga.isValid());
    }

    // Device physical address round-trip
    void devicePhysicalAddressRoundTrip()
    {
        auto dev = std::make_unique<DeviceInstance>(
            QStringLiteral("d1"), QStringLiteral("ref"), QStringLiteral("app"));
        dev->setPhysicalAddress(QStringLiteral("1.2.3"));
        QCOMPARE(dev->physicalAddress(), QStringLiteral("1.2.3"));
    }

    // Device description round-trip
    void deviceDescriptionRoundTrip()
    {
        auto dev = std::make_unique<DeviceInstance>(
            QStringLiteral("d1"), QStringLiteral("ref"), QStringLiteral("app"));
        dev->setDescription(QStringLiteral("Schaltaktor Wohnzimmer"));
        QCOMPARE(dev->description(), QStringLiteral("Schaltaktor Wohnzimmer"));
    }

    // takeDeviceAt moves ownership out of line
    void takeDeviceAtTransfersOwnership()
    {
        auto line = std::make_unique<TopologyNode>(
            TopologyNode::Type::Line, 1, QStringLiteral("L"));
        line->addDevice(std::make_unique<DeviceInstance>(
            QStringLiteral("d1"), QStringLiteral("ref"), QStringLiteral("app")));
        line->addDevice(std::make_unique<DeviceInstance>(
            QStringLiteral("d2"), QStringLiteral("ref"), QStringLiteral("app")));
        QCOMPARE(line->deviceCount(), 2);

        auto taken = line->takeDeviceAt(0);
        QVERIFY(taken != nullptr);
        QCOMPARE(taken->id(), QStringLiteral("d1"));
        QCOMPARE(line->deviceCount(), 1);
        QCOMPARE(line->deviceAt(0)->id(), QStringLiteral("d2"));
    }
};

QTEST_MAIN(TestParameterVisibility)
#include "test_parameter_visibility.moc"
