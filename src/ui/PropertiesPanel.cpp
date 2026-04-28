#include "PropertiesPanel.h"

#include "DeviceInstance.h"
#include "GroupAddress.h"

#include <QStackedWidget>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QFrame>
#include <QWidget>

PropertiesPanel::PropertiesPanel(QWidget *parent)
    : QDockWidget(tr("Eigenschaften"), parent)
{
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    setMinimumWidth(200);
    setObjectName(QStringLiteral("propertiesPanel"));

    m_stack = new QStackedWidget(this);

    // Page 0: empty / no selection
    auto *emptyPage = new QWidget(this);
    auto *emptyLabel = new QLabel(tr("Kein Element ausgewählt"), emptyPage);
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet(QStringLiteral("color: #999;"));
    auto *el = new QVBoxLayout(emptyPage);
    el->addWidget(emptyLabel);
    m_stack->addWidget(emptyPage);  // index 0

    // Page 1: Device properties
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

    m_devPhysEdit = new QLineEdit(m_devicePage);
    m_devPhysEdit->setPlaceholderText(tr("z.B. 1.1.1"));
    devForm->addRow(tr("Phys. Adresse:"), m_devPhysEdit);

    devLayout->addLayout(devForm);
    devLayout->addStretch();
    m_stack->addWidget(m_devicePage);  // index 1

    // Page 2: GroupAddress properties
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

    connect(m_devPhysEdit, &QLineEdit::editingFinished,
            this, &PropertiesPanel::onPhysAddrEdited);
    connect(m_gaNameEdit, &QLineEdit::editingFinished,
            this, &PropertiesPanel::onGaNameEdited);
    connect(m_gaDptEdit, &QLineEdit::editingFinished,
            this, &PropertiesPanel::onGaDptEdited);
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
    m_devPhysEdit->setText(device->physicalAddress());
    m_updating = false;
    m_stack->setCurrentIndex(1);
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
    m_updating = false;
    m_stack->setCurrentIndex(2);
}

void PropertiesPanel::clearSelection()
{
    m_currentDevice = nullptr;
    m_currentGa = nullptr;
    m_stack->setCurrentIndex(0);
}

void PropertiesPanel::onPhysAddrEdited()
{
    if (m_updating || !m_currentDevice)
        return;
    m_currentDevice->setPhysicalAddress(m_devPhysEdit->text().trimmed());
    emit deviceModified();
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
    m_currentGa->setDpt(m_gaDptEdit->text().trimmed());
    emit groupAddressModified();
}
