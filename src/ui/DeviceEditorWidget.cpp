#include "DeviceEditorWidget.h"

#include "Project.h"
#include "DeviceInstance.h"
#include "KnxApplicationProgram.h"
#include "GroupAddress.h"
#include "ComObjectLink.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QTabWidget>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
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
    m_tabs->addTab(paramScroll, tr("Parameter"));

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

void DeviceEditorWidget::clearDevice()
{
    m_device  = nullptr;
    m_project = nullptr;
    m_title->setText(tr("Kein Gerät ausgewählt"));
    clearTabs();
    m_tabs->setEnabled(false);
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
