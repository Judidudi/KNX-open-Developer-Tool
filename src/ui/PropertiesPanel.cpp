#include "PropertiesPanel.h"

#include "DeviceInstance.h"
#include "GroupAddress.h"
#include "Project.h"
#include "TopologyNode.h"
#include "InterfaceManager.h"
#include "IKnxInterface.h"
#include "CemiFrame.h"
#include "DptRegistry.h"

#include <QStackedWidget>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QFrame>
#include <QWidget>
#include <QPushButton>
#include <QTimer>

PropertiesPanel::PropertiesPanel(QWidget *parent)
    : QDockWidget(tr("Eigenschaften"), parent)
    , m_pingTimer(new QTimer(this))
{
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    setMinimumWidth(200);
    setObjectName(QStringLiteral("propertiesPanel"));

    m_stack = new QStackedWidget(this);

    // ── Page 0: empty / no selection ─────────────────────────────────────────
    auto *emptyPage = new QWidget(this);
    auto *emptyLabel = new QLabel(tr("Kein Element ausgewählt"), emptyPage);
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet(QStringLiteral("color: #999;"));
    auto *el = new QVBoxLayout(emptyPage);
    el->addWidget(emptyLabel);
    m_stack->addWidget(emptyPage);  // index 0

    // ── Page 1: Device properties ─────────────────────────────────────────────
    m_devicePage = new QWidget(this);
    auto *devLayout = new QVBoxLayout(m_devicePage);
    devLayout->setContentsMargins(8, 8, 8, 8);
    devLayout->setSpacing(6);

    auto *devTitle = new QLabel(tr("Gerät"), m_devicePage);
    devTitle->setStyleSheet(QStringLiteral("font-weight:bold; font-size:10pt; color:#E87722;"));
    devLayout->addWidget(devTitle);

    auto *devSep = new QFrame(m_devicePage);
    devSep->setFrameShape(QFrame::HLine);
    devSep->setFrameShadow(QFrame::Sunken);
    devLayout->addWidget(devSep);

    auto *devForm = new QFormLayout;
    devForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    devForm->setSpacing(6);

    m_devTypeLabel = new QLabel(m_devicePage);
    m_devTypeLabel->setWordWrap(true);
    devForm->addRow(tr("Typ:"), m_devTypeLabel);

    m_devNameEdit = new QLineEdit(m_devicePage);
    m_devNameEdit->setPlaceholderText(tr("Bezeichnung"));
    devForm->addRow(tr("Bezeichnung:"), m_devNameEdit);

    m_devPhysEdit = new QLineEdit(m_devicePage);
    m_devPhysEdit->setPlaceholderText(tr("z.B. 1.1.1"));
    devForm->addRow(tr("Phys. Adresse:"), m_devPhysEdit);

    m_devConflictLbl = new QLabel(m_devicePage);
    m_devConflictLbl->setStyleSheet(QStringLiteral("color:#D32F2F; font-size:8pt;"));
    m_devConflictLbl->setVisible(false);
    devForm->addRow(QString(), m_devConflictLbl);

    devLayout->addLayout(devForm);

    // Ping row
    auto *pingRow = new QHBoxLayout;
    m_pingBtn = new QPushButton(tr("Gerät prüfen"), m_devicePage);
    m_pingBtn->setEnabled(false);
    m_pingResultLbl = new QLabel(m_devicePage);
    m_pingResultLbl->setWordWrap(true);
    pingRow->addWidget(m_pingBtn);
    pingRow->addWidget(m_pingResultLbl, 1);
    devLayout->addLayout(pingRow);

    devLayout->addStretch();
    m_stack->addWidget(m_devicePage);  // index 1

    // ── Page 2: GroupAddress properties ──────────────────────────────────────
    m_gaPage = new QWidget(this);
    auto *gaLayout = new QVBoxLayout(m_gaPage);
    gaLayout->setContentsMargins(8, 8, 8, 8);
    gaLayout->setSpacing(6);

    auto *gaTitle = new QLabel(tr("Gruppenadresse"), m_gaPage);
    gaTitle->setStyleSheet(QStringLiteral("font-weight:bold; font-size:10pt; color:#E87722;"));
    gaLayout->addWidget(gaTitle);

    auto *gaSep = new QFrame(m_gaPage);
    gaSep->setFrameShape(QFrame::HLine);
    gaSep->setFrameShadow(QFrame::Sunken);
    gaLayout->addWidget(gaSep);

    auto *gaForm = new QFormLayout;
    gaForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    gaForm->setSpacing(6);

    m_gaAddrLabel = new QLabel(m_gaPage);
    gaForm->addRow(tr("Adresse:"), m_gaAddrLabel);

    m_gaNameEdit = new QLineEdit(m_gaPage);
    gaForm->addRow(tr("Name:"), m_gaNameEdit);

    m_gaDptEdit = new QLineEdit(m_gaPage);
    m_gaDptEdit->setPlaceholderText(tr("z.B. 1.001"));
    gaForm->addRow(tr("DPT:"), m_gaDptEdit);

    gaLayout->addLayout(gaForm);
    gaLayout->addStretch();
    m_stack->addWidget(m_gaPage);  // index 2

    setWidget(m_stack);

    m_pingTimer->setSingleShot(true);
    m_pingTimer->setInterval(2000);

    connect(m_devNameEdit, &QLineEdit::editingFinished,
            this, &PropertiesPanel::onDevDescEdited);
    connect(m_devPhysEdit, &QLineEdit::editingFinished,
            this, &PropertiesPanel::onPhysAddrEdited);
    connect(m_gaNameEdit, &QLineEdit::editingFinished,
            this, &PropertiesPanel::onGaNameEdited);
    connect(m_gaDptEdit, &QLineEdit::editingFinished,
            this, &PropertiesPanel::onGaDptEdited);
    connect(m_pingBtn, &QPushButton::clicked,
            this, &PropertiesPanel::onPingClicked);
    connect(m_pingTimer, &QTimer::timeout,
            this, &PropertiesPanel::onPingTimeout);
}

void PropertiesPanel::setProject(Project *project)
{
    m_project = project;
}

void PropertiesPanel::setInterfaceManager(InterfaceManager *mgr)
{
    if (m_iface)
        disconnect(m_iface, nullptr, this, nullptr);
    m_iface = mgr;
    if (m_iface) {
        connect(m_iface, &InterfaceManager::cemiFrameReceived,
                this, [this](const QByteArray &cemi) {
                    if (!m_pingTimer->isActive() || !m_currentDevice)
                        return;
                    const CemiFrame frame = CemiFrame::fromBytes(cemi);
                    if (!frame.groupAddress && frame.isDeviceDescriptorResponse()) {
                        const uint16_t expectedPa = CemiFrame::physAddrFromString(
                            m_currentDevice->physicalAddress());
                        if (frame.sourceAddress == expectedPa) {
                            m_pingTimer->stop();
                            const uint16_t desc = (static_cast<uint8_t>(frame.apdu.at(1)) << 8)
                                                | static_cast<uint8_t>(frame.apdu.at(2));
                            m_pingResultLbl->setStyleSheet(QStringLiteral("color:#2E7D32;"));
                            m_pingResultLbl->setText(tr("Erreichbar (Typ: 0x%1)")
                                .arg(desc, 4, 16, QLatin1Char('0')));
                        }
                    }
                });
    }
    m_pingBtn->setEnabled(mgr != nullptr);
}

void PropertiesPanel::showDevice(DeviceInstance *device)
{
    m_currentDevice = device;
    m_currentGa = nullptr;
    if (!device) {
        m_stack->setCurrentIndex(0);
        return;
    }
    m_updating = true;
    m_devTypeLabel->setText(device->productRefId());
    m_devNameEdit->setText(device->description());
    m_devPhysEdit->setText(device->physicalAddress());
    m_devConflictLbl->setVisible(false);
    m_pingResultLbl->clear();
    m_updating = false;
    m_stack->setCurrentIndex(1);
    checkPhysAddrConflict(device->physicalAddress());
}

void PropertiesPanel::showGroupAddress(GroupAddress *ga)
{
    m_currentGa = ga;
    m_currentDevice = nullptr;
    if (!ga) {
        m_stack->setCurrentIndex(0);
        return;
    }
    m_updating = true;
    m_gaAddrLabel->setText(ga->toString());
    m_gaNameEdit->setText(ga->name());
    m_gaDptEdit->setText(ga->dpt());
    if (const DptInfo *info = DptRegistry::instance().find(ga->dpt()))
        m_gaDptEdit->setToolTip(info->nameDe + QStringLiteral(" (") + info->name + QLatin1Char(')'));
    else
        m_gaDptEdit->setToolTip({});
    m_updating = false;
    m_stack->setCurrentIndex(2);
}

void PropertiesPanel::clearSelection()
{
    m_currentDevice = nullptr;
    m_currentGa = nullptr;
    m_stack->setCurrentIndex(0);
}

void PropertiesPanel::onDevDescEdited()
{
    if (m_updating || !m_currentDevice)
        return;
    m_currentDevice->setDescription(m_devNameEdit->text().trimmed());
    emit deviceModified();
}

void PropertiesPanel::onPhysAddrEdited()
{
    if (m_updating || !m_currentDevice)
        return;
    const QString pa = m_devPhysEdit->text().trimmed();
    m_currentDevice->setPhysicalAddress(pa);
    checkPhysAddrConflict(pa);
    emit deviceModified();
}

void PropertiesPanel::checkPhysAddrConflict(const QString &pa)
{
    if (!m_project || pa.isEmpty()) {
        m_devConflictLbl->setVisible(false);
        return;
    }
    int count = 0;
    for (int a = 0; a < m_project->areaCount(); ++a) {
        const auto *area = m_project->areaAt(a);
        for (int l = 0; l < area->childCount(); ++l) {
            const auto *line = area->childAt(l);
            for (int d = 0; d < line->deviceCount(); ++d) {
                const auto *dev = line->deviceAt(d);
                if (dev && dev != m_currentDevice && dev->physicalAddress() == pa)
                    ++count;
            }
        }
    }
    if (count > 0) {
        m_devConflictLbl->setText(tr("Adresse bereits %n-mal vergeben!", nullptr, count));
        m_devConflictLbl->setVisible(true);
    } else {
        m_devConflictLbl->setVisible(false);
    }
}

void PropertiesPanel::onGaNameEdited()
{
    if (m_updating || !m_currentGa)
        return;
    m_currentGa->setName(m_gaNameEdit->text().trimmed());
    emit groupAddressModified();
}

void PropertiesPanel::onGaDptEdited()
{
    if (m_updating || !m_currentGa)
        return;
    const QString dpt = m_gaDptEdit->text().trimmed();
    m_currentGa->setDpt(dpt);
    if (const DptInfo *info = DptRegistry::instance().find(dpt))
        m_gaDptEdit->setToolTip(info->nameDe + QStringLiteral(" (") + info->name + QLatin1Char(')'));
    else
        m_gaDptEdit->setToolTip({});
    emit groupAddressModified();
}

void PropertiesPanel::onPingClicked()
{
    if (!m_currentDevice || !m_iface || !m_iface->activeInterface())
        return;
    m_pingResultLbl->setStyleSheet(QStringLiteral("color:#555;"));
    m_pingResultLbl->setText(tr("Warte…"));
    const uint16_t pa = CemiFrame::physAddrFromString(m_currentDevice->physicalAddress());
    m_iface->activeInterface()->sendCemiFrame(CemiFrame::buildDeviceDescriptorRead(pa));
    m_pingTimer->start();
}

void PropertiesPanel::onPingTimeout()
{
    m_pingResultLbl->setStyleSheet(QStringLiteral("color:#D32F2F;"));
    m_pingResultLbl->setText(tr("Nicht erreichbar (Timeout)"));
}
