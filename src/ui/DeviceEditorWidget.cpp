#include "DeviceEditorWidget.h"

#include "Project.h"
#include "DeviceInstance.h"
#include "KnxApplicationProgram.h"
#include "GroupAddress.h"
#include "ComObjectLink.h"
#include "InterfaceManager.h"
#include "IKnxInterface.h"
#include "CemiFrame.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QTabWidget>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QScrollArea>
#include <QScrollBar>

DeviceEditorWidget::DeviceEditorWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);

    m_title = new QLabel(tr("Kein Gerät ausgewählt"), this);
    m_title->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 11pt;"));
    layout->addWidget(m_title);

    m_tabs = new QTabWidget(this);
    layout->addWidget(m_tabs);

    // Parameter tab: wrap in a QScrollArea so visibility rebuilds don't change window size
    m_paramTab = new QWidget(this);
    m_paramLayout = new QFormLayout(m_paramTab);
    auto *paramScroll = new QScrollArea(m_tabs);
    paramScroll->setWidgetResizable(true);
    paramScroll->setWidget(m_paramTab);

    auto *paramTabContainer = new QWidget(m_tabs);
    auto *paramTabLayout    = new QVBoxLayout(paramTabContainer);
    paramTabLayout->setContentsMargins(0, 0, 0, 0);
    paramTabLayout->addWidget(paramScroll);

    auto *readRow = new QHBoxLayout;
    m_readBtn = new QPushButton(tr("Vom Gerät lesen"), paramTabContainer);
    m_readBtn->setEnabled(false);
    readRow->addStretch();
    readRow->addWidget(m_readBtn);
    paramTabLayout->addLayout(readRow);
    connect(m_readBtn, &QPushButton::clicked, this, &DeviceEditorWidget::onReadParametersClicked);

    m_tabs->addTab(paramTabContainer, tr("Parameter"));

    m_comObjTab = new QWidget(m_tabs);
    m_tabs->addTab(m_comObjTab, tr("Kommunikationsobjekte"));

    auto *comLayout = new QVBoxLayout(m_comObjTab);
    m_comObjTable = new QTableWidget(m_comObjTab);
    m_comObjTable->setColumnCount(5);
    m_comObjTable->setHorizontalHeaderLabels({tr("Name"), tr("DPT"), tr("Flags"), tr("Richtung"), tr("Gruppenadresse")});
    m_comObjTable->horizontalHeader()->setStretchLastSection(true);
    m_comObjTable->verticalHeader()->setVisible(false);
    m_comObjTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_comObjTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    comLayout->addWidget(m_comObjTable);

    m_tabs->setEnabled(false);
}

void DeviceEditorWidget::setInterfaceManager(InterfaceManager *mgr)
{
    if (m_iface == mgr)
        return;
    if (m_iface)
        disconnect(m_iface, &InterfaceManager::cemiFrameReceived,
                   this, &DeviceEditorWidget::onCemiReceived);
    m_iface = mgr;
    if (m_iface)
        connect(m_iface, &InterfaceManager::cemiFrameReceived,
                this, &DeviceEditorWidget::onCemiReceived);
    m_readBtn->setEnabled(m_iface && m_iface->isConnected() && m_device);
}

void DeviceEditorWidget::clearDevice()
{
    m_device  = nullptr;
    m_project = nullptr;
    m_title->setText(tr("Kein Gerät ausgewählt"));
    clearTabs();
    m_tabs->setEnabled(false);
    m_readBtn->setEnabled(false);
    m_readBuffer.clear();
    m_readExpected = 0;
}

void DeviceEditorWidget::setDevice(DeviceInstance *device, Project *project)
{
    m_device  = device;
    m_project = project;
    clearTabs();

    if (!m_device || !m_device->appProgram()) {
        m_title->setText(tr("Anwendungsprogramm nicht gefunden: %1").arg(m_device ? m_device->productRefId() : QString()));
        return;
    }

    const KnxApplicationProgram *app = m_device->appProgram();
    m_title->setText(tr("%1 – %2 (%3)")
                         .arg(m_device->physicalAddress(), app->name, m_device->productRefId()));

    buildParameterTab();
    buildComObjectTab();
    m_tabs->setEnabled(true);
    m_readBtn->setEnabled(m_iface && m_iface->isConnected());
}

void DeviceEditorWidget::clearTabs()
{
    m_paramWidgets.clear();

    // Drain parameter form layout
    while (m_paramLayout->count() > 0) {
        QLayoutItem *item = m_paramLayout->takeAt(0);
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }

    m_comObjTable->setRowCount(0);
}

bool DeviceEditorWidget::isParameterVisible(const KnxParameter &p) const
{
    if (p.access == KnxParameter::Access::Hidden)
        return false;

    if (!p.conditionParamId.isEmpty()) {
        const auto it = m_device->parameters().find(p.conditionParamId);
        const QVariant cur = (it != m_device->parameters().end()) ? it->second : QVariant(0);
        const bool eq = (cur.toString() == p.conditionValue.toString());
        if (p.conditionOp == KnxParameter::ConditionOp::Equal && !eq)
            return false;
        if (p.conditionOp == KnxParameter::ConditionOp::NotEqual && eq)
            return false;
    }
    return true;
}

void DeviceEditorWidget::buildParameterTab()
{
    m_updating = true;
    const KnxApplicationProgram *app = m_device->appProgram();

    int visibleCount = 0;
    for (const KnxParameter &p : app->parameters) {
        if (!isParameterVisible(p))
            continue;

        ++visibleCount;
        auto paramIt = m_device->parameters().find(p.id);
        const QVariant currentValue = (paramIt != m_device->parameters().end()) ? paramIt->second : p.defaultValue;
        const KnxParameterType *pt = app->findType(p.typeId);
        QWidget *editor = nullptr;

        if (pt && pt->kind == KnxParameterType::Kind::Bool) {
            auto *cb = new QCheckBox(m_paramTab);
            cb->setChecked(currentValue.toInt() != 0);
            connect(cb, &QCheckBox::toggled, this, &DeviceEditorWidget::onParameterChanged);
            editor = cb;
        } else if (pt && pt->kind == KnxParameterType::Kind::Enum) {
            auto *combo = new QComboBox(m_paramTab);
            int selectedIdx = 0;
            for (int i = 0; i < pt->enumValues.size(); ++i) {
                const auto &ev = pt->enumValues[i];
                combo->addItem(ev.text, ev.value);
                if (ev.value == currentValue.toInt())
                    selectedIdx = i;
            }
            combo->setCurrentIndex(selectedIdx);
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &DeviceEditorWidget::onParameterChanged);
            editor = combo;
        } else {
            auto *spin = new QSpinBox(m_paramTab);
            const int minV = pt ? pt->minValue : 0;
            const int maxV = pt ? pt->maxValue : 65535;
            spin->setRange(minV, maxV);
            spin->setValue(currentValue.toInt());
            connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
                    this, &DeviceEditorWidget::onParameterChanged);
            editor = spin;
        }

        editor->setProperty("paramId", p.id);
        m_paramWidgets.insert(p.id, editor);
        m_paramLayout->addRow(p.name + QStringLiteral(":"), editor);
    }

    if (visibleCount == 0)
        m_paramLayout->addRow(new QLabel(tr("Dieses Gerät hat keine sichtbaren Parameter."), m_paramTab));

    m_updating = false;
}

void DeviceEditorWidget::rebuildParameterTab()
{
    // Preserve scroll position across rebuild
    QScrollArea *scroll = nullptr;
    for (int i = 0; i < m_tabs->count(); ++i) {
        if ((scroll = qobject_cast<QScrollArea *>(m_tabs->widget(i))))
            break;
    }
    const int scrollValue = scroll ? scroll->verticalScrollBar()->value() : 0;

    m_paramWidgets.clear();
    while (m_paramLayout->count() > 0) {
        QLayoutItem *item = m_paramLayout->takeAt(0);
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }

    buildParameterTab();

    if (scroll)
        scroll->verticalScrollBar()->setValue(scrollValue);
}

void DeviceEditorWidget::buildComObjectTab()
{
    m_updating = true;
    const KnxApplicationProgram *app = m_device->appProgram();

    m_comObjTable->setRowCount(app->comObjects.size());

    for (int row = 0; row < app->comObjects.size(); ++row) {
        const KnxComObject &co = app->comObjects[row];

        m_comObjTable->setItem(row, 0, new QTableWidgetItem(co.name));
        m_comObjTable->setItem(row, 1, new QTableWidgetItem(co.dpt));
        m_comObjTable->setItem(row, 2, new QTableWidgetItem(co.flags.join(QString())));

        // Find currently linked state for this ComObject
        ComObjectLink::Direction currentDir = ComObjectLink::Direction::Send;
        QString linkedGa;
        for (const ComObjectLink &link : m_device->links()) {
            if (link.comObjectId == co.id && link.ga.isValid()) {
                linkedGa   = link.ga.toString();
                currentDir = link.direction;
                break;
            }
        }

        // Direction dropdown
        auto *dirCombo = new QComboBox(m_comObjTable);
        dirCombo->addItem(tr("Senden"),   static_cast<int>(ComObjectLink::Direction::Send));
        dirCombo->addItem(tr("Empfangen"), static_cast<int>(ComObjectLink::Direction::Receive));
        dirCombo->setCurrentIndex(currentDir == ComObjectLink::Direction::Receive ? 1 : 0);
        connect(dirCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, row](int){ onComObjectLinkChanged(row); });
        m_comObjTable->setCellWidget(row, 3, dirCombo);

        // GA dropdown: lists all project group addresses + "(keine)"
        auto *gaCombo = new QComboBox(m_comObjTable);
        gaCombo->addItem(tr("(keine)"), QString());
        if (m_project) {
            for (const GroupAddress &ga : m_project->groupAddresses()) {
                if (!ga.dpt().isEmpty() && ga.dpt() != co.dpt)
                    continue;  // filter by matching DPT
                gaCombo->addItem(QStringLiteral("%1 – %2").arg(ga.toString(), ga.name()),
                                 ga.toString());
            }
        }

        const int idx = gaCombo->findData(linkedGa);
        gaCombo->setCurrentIndex(idx >= 0 ? idx : 0);

        connect(gaCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, row](int){ onComObjectLinkChanged(row); });

        m_comObjTable->setCellWidget(row, 4, gaCombo);
    }

    m_comObjTable->resizeColumnsToContents();
    m_comObjTable->horizontalHeader()->setStretchLastSection(true);
    m_updating = false;
}

void DeviceEditorWidget::onParameterChanged()
{
    if (m_updating || !m_device)
        return;

    QWidget *editor = qobject_cast<QWidget *>(sender());
    if (!editor)
        return;

    const QString id = editor->property("paramId").toString();
    if (id.isEmpty())
        return;

    QVariant newValue;
    if (auto *cb = qobject_cast<QCheckBox *>(editor))
        newValue = cb->isChecked() ? 1 : 0;
    else if (auto *spin = qobject_cast<QSpinBox *>(editor))
        newValue = spin->value();
    else if (auto *combo = qobject_cast<QComboBox *>(editor))
        newValue = combo->currentData();

    m_device->parameters()[id] = newValue;

    // Check if any parameter uses this one as a condition — if so, rebuild tab.
    const KnxApplicationProgram *app = m_device->appProgram();
    if (app) {
        bool needsRebuild = false;
        for (const KnxParameter &p : app->parameters) {
            if (p.conditionParamId == id) {
                needsRebuild = true;
                break;
            }
        }
        if (needsRebuild)
            rebuildParameterTab();
    }

    emit deviceModified();
}

void DeviceEditorWidget::onComObjectLinkChanged(int row)
{
    if (m_updating || !m_device || !m_device->appProgram())
        return;
    if (row < 0 || row >= m_device->appProgram()->comObjects.size())
        return;

    const QString comObjectId = m_device->appProgram()->comObjects[row].id;
    auto *dirCombo = qobject_cast<QComboBox *>(m_comObjTable->cellWidget(row, 3));
    auto *gaCombo  = qobject_cast<QComboBox *>(m_comObjTable->cellWidget(row, 4));
    if (!gaCombo)
        return;

    const QString gaString = gaCombo->currentData().toString();
    const auto direction = (dirCombo && dirCombo->currentData().toInt() ==
                            static_cast<int>(ComObjectLink::Direction::Receive))
                           ? ComObjectLink::Direction::Receive
                           : ComObjectLink::Direction::Send;

    // Update or create link entry
    bool found = false;
    for (ComObjectLink &link : m_device->links()) {
        if (link.comObjectId == comObjectId) {
            link.ga        = gaString.isEmpty() ? GroupAddress() : GroupAddress::fromString(gaString);
            link.direction = direction;
            found = true;
            break;
        }
    }
    if (!found) {
        ComObjectLink link;
        link.comObjectId = comObjectId;
        link.ga          = gaString.isEmpty() ? GroupAddress() : GroupAddress::fromString(gaString);
        link.direction   = direction;
        m_device->addLink(link);
    }

    emit deviceModified();
}

void DeviceEditorWidget::onReadParametersClicked()
{
    if (!m_device || !m_iface || !m_iface->activeInterface())
        return;
    const KnxApplicationProgram *app = m_device->appProgram();
    if (!app || app->memoryLayout.parameterSize == 0)
        return;

    const uint16_t pa   = CemiFrame::physAddrFromString(m_device->physicalAddress());
    const uint16_t base = static_cast<uint16_t>(app->memoryLayout.parameterBase);
    uint32_t       size = app->memoryLayout.parameterSize;

    m_readBaseAddr = base;
    m_readBuffer.clear();
    m_readExpected = static_cast<int>(size);

    // Send read requests in 63-byte chunks
    uint16_t offset = 0;
    while (size > 0) {
        const uint8_t chunk = static_cast<uint8_t>(size > 63 ? 63 : size);
        m_iface->activeInterface()->sendCemiFrame(
            CemiFrame::buildMemoryRead(pa, static_cast<uint16_t>(base + offset), chunk));
        offset += chunk;
        size   -= chunk;
    }

    m_readBtn->setEnabled(false);
}

void DeviceEditorWidget::onCemiReceived(const QByteArray &cemi)
{
    if (!m_device)
        return;
    const CemiFrame frame = CemiFrame::fromBytes(cemi);
    if (!frame.isMemoryResponse())
        return;

    const uint16_t pa = CemiFrame::physAddrFromString(m_device->physicalAddress());
    if (frame.sourceAddress != pa)
        return;

    uint16_t   addr;
    QByteArray data;
    if (!frame.memoryResponseData(addr, data))
        return;

    const int bufOffset = addr - m_readBaseAddr;
    if (bufOffset < 0)
        return;
    if (m_readBuffer.size() < bufOffset + data.size())
        m_readBuffer.resize(bufOffset + data.size());
    for (int i = 0; i < data.size(); ++i)
        m_readBuffer[bufOffset + i] = data[i];

    if (m_readBuffer.size() >= m_readExpected) {
        applyReadbackValues(m_readBuffer, m_readBaseAddr);
        m_readBuffer.clear();
        m_readExpected = 0;
        m_readBtn->setEnabled(true);
    }
}

void DeviceEditorWidget::applyReadbackValues(const QByteArray &memory, uint16_t baseAddr)
{
    if (!m_device)
        return;
    const KnxApplicationProgram *app = m_device->appProgram();
    if (!app)
        return;

    for (const KnxParameter &p : app->parameters) {
        QWidget *w = m_paramWidgets.value(p.id);
        if (!w)
            continue;
        const int memOffset = static_cast<int>(p.offset) - static_cast<int>(baseAddr);
        if (memOffset < 0 || memOffset >= memory.size())
            continue;

        const uint8_t raw = static_cast<uint8_t>(memory[memOffset]);
        auto it = m_device->parameters().find(p.id);
        const int projectVal = (it != m_device->parameters().end()) ? it->second.toInt() : p.defaultValue.toInt();
        const bool match = (static_cast<int>(raw) == projectVal);

        w->setStyleSheet(match
            ? QStringLiteral("background-color: #c8e6c9;")
            : QStringLiteral("background-color: #ffcdd2;"));
        w->setToolTip(match
            ? tr("Gerätewert stimmt überein: %1").arg(raw)
            : tr("Abweichung! Gerät: %1  Projekt: %2").arg(raw).arg(projectVal));
    }
}
