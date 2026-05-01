#include "ProjectTreeWidget.h"
#include "ProjectTreeModel.h"
#include "CatalogWidget.h"
#include "BuildingPart.h"
#include "TopologyNode.h"
#include "DeviceInstance.h"
#include "GroupAddress.h"

#include <QTabWidget>
#include <QTreeView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QItemSelectionModel>
#include <QMenu>
#include <QKeyEvent>
#include <QAbstractItemView>

// Helper: installs an event filter on a tree view to intercept Del/F2
class TreeKeyFilter : public QObject
{
public:
    explicit TreeKeyFilter(QTreeView *view, ProjectTreeWidget *target)
        : QObject(view), m_view(view), m_target(target)
    {}

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override
    {
        if (obj == m_view && ev->type() == QEvent::KeyPress) {
            auto *ke = static_cast<QKeyEvent *>(ev);
            const QModelIndexList sel = m_view->selectionModel()->selectedIndexes();
            if (sel.isEmpty()) return false;
            const QModelIndex idx = sel.first();

            if (ke->key() == Qt::Key_F2) {
                m_view->edit(idx);
                return true;
            }
            if (ke->key() == Qt::Key_Delete) {
                if (m_view->objectName() == QLatin1String("topoView"))
                    m_target->onTopoKeyPressed(Qt::Key_Delete, idx);
                else if (m_view->objectName() == QLatin1String("gaView"))
                    m_target->onGaKeyPressed(Qt::Key_Delete, idx);
                else if (m_view->objectName() == QLatin1String("buildingView"))
                    m_target->onBuildingKeyPressed(Qt::Key_Delete, idx);
                return true;
            }
        }
        return false;
    }

private:
    QTreeView        *m_view;
    ProjectTreeWidget *m_target;
};

static void setupTreeView(QTreeView *view)
{
    view->setRootIsDecorated(true);
    view->setHeaderHidden(true);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setAnimated(true);
    view->setIndentation(16);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
}

ProjectTreeWidget::ProjectTreeWidget(QWidget *parent)
    : QWidget(parent)
    , m_tabs(new QTabWidget(this))
    , m_topoView(new QTreeView(this))
    , m_gaView(new QTreeView(this))
    , m_buildingView(new QTreeView(this))
    , m_catWidget(new CatalogWidget(this))
    , m_model(new ProjectTreeModel(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_tabs);

    ::setupTreeView(m_topoView);
    ::setupTreeView(m_gaView);
    ::setupTreeView(m_buildingView);

    m_topoView->setObjectName(QStringLiteral("topoView"));
    m_gaView->setObjectName(QStringLiteral("gaView"));
    m_buildingView->setObjectName(QStringLiteral("buildingView"));

    m_topoView->setModel(m_model);
    m_gaView->setModel(m_model);
    m_buildingView->setModel(m_model);

    // Only show the label column (column 1 = "details" is hidden in nav views)
    m_topoView->setColumnHidden(1, true);
    m_gaView->setColumnHidden(1, true);
    m_buildingView->setColumnHidden(1, true);

    m_tabs->addTab(m_topoView,      tr("Topologie"));
    m_tabs->addTab(m_gaView,        tr("Gruppen"));
    m_tabs->addTab(m_buildingView,  tr("Gebäude"));
    m_tabs->addTab(m_catWidget,     tr("Katalog"));

    connect(m_topoView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ProjectTreeWidget::onTopoSelectionChanged);
    connect(m_gaView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ProjectTreeWidget::onGaSelectionChanged);

    // Context menus
    connect(m_topoView, &QTreeView::customContextMenuRequested,
            this, &ProjectTreeWidget::onTopoContextMenu);
    connect(m_gaView, &QTreeView::customContextMenuRequested,
            this, &ProjectTreeWidget::onGaContextMenu);
    connect(m_buildingView, &QTreeView::customContextMenuRequested,
            this, &ProjectTreeWidget::onBuildingContextMenu);

    // Key filters for Del/F2
    m_topoView->installEventFilter(new TreeKeyFilter(m_topoView, this));
    m_gaView->installEventFilter(new TreeKeyFilter(m_gaView, this));
    m_buildingView->installEventFilter(new TreeKeyFilter(m_buildingView, this));

    // Forward catalog signals up to MainWindow
    connect(m_catWidget, &CatalogWidget::addDeviceRequested,
            this,        &ProjectTreeWidget::addDeviceRequested);
    connect(m_catWidget, &CatalogWidget::importRequested,
            this,        &ProjectTreeWidget::catalogImportRequested);

    // Emit projectModified when inline rename succeeds (setData emits dataChanged with DisplayRole)
    connect(m_model, &ProjectTreeModel::dataChanged,
            this, [this](const QModelIndex &, const QModelIndex &, const QList<int> &roles) {
                if (roles.contains(Qt::DisplayRole))
                    emit projectModified();
            });
}

void ProjectTreeWidget::setProject(Project *project)
{
    m_model->setProject(project);
    updateRootIndices();
}

void ProjectTreeWidget::setCatalog(KnxprodCatalog *catalog)
{
    m_catWidget->setCatalog(catalog);
}

void ProjectTreeWidget::refresh()
{
    m_model->setProject(m_model->project());
    updateRootIndices();
}

void ProjectTreeWidget::updateRootIndices()
{
    // Row 0 = Topology, Row 1 = GA, Row 2 = Buildings (see ProjectTreeModel::rebuild())
    const QModelIndex topoRoot     = m_model->index(0, 0, {});
    const QModelIndex gaRoot       = m_model->index(1, 0, {});
    const QModelIndex buildingRoot = m_model->index(2, 0, {});
    m_topoView->setRootIndex(topoRoot);
    m_gaView->setRootIndex(gaRoot);
    m_buildingView->setRootIndex(buildingRoot);
    m_topoView->expandAll();
    m_gaView->expandAll();
    m_buildingView->expandAll();
}

void ProjectTreeWidget::onTopoSelectionChanged()
{
    const QModelIndexList sel = m_topoView->selectionModel()->selectedIndexes();
    if (sel.isEmpty()) {
        emit selectionCleared();
        return;
    }
    if (DeviceInstance *dev = m_model->deviceAt(sel.first())) {
        emit deviceSelected(dev);
        return;
    }
    emit selectionCleared();
}

void ProjectTreeWidget::onGaSelectionChanged()
{
    const QModelIndexList sel = m_gaView->selectionModel()->selectedIndexes();
    if (sel.isEmpty()) {
        emit selectionCleared();
        return;
    }
    if (::GroupAddress *ga = m_model->groupAddressAt(sel.first())) {
        emit groupAddressSelected(ga);
        return;
    }
    emit selectionCleared();
}

void ProjectTreeWidget::onTopoContextMenu(const QPoint &pos)
{
    const QModelIndex idx = m_topoView->indexAt(pos);
    const ProjectTreeModel::ItemKind kind = idx.isValid()
                                            ? m_model->kindAt(idx)
                                            : ProjectTreeModel::TopologyRoot;

    QMenu menu(this);

    if (!idx.isValid() || kind == ProjectTreeModel::TopologyRoot) {
        menu.addAction(tr("Neuen Bereich…"), this, [this](){
            emit addAreaRequested();
        });
    } else if (kind == ProjectTreeModel::Area) {
        auto *tn = m_model->topologyNodeAt(idx);
        menu.addAction(tr("Neue Linie…"), this, [this, tn](){
            emit addLineRequested(tn);
        });
        menu.addSeparator();
        menu.addAction(tr("Umbenennen"), this, [this, idx](){
            m_topoView->edit(idx);
        });
        menu.addAction(tr("Löschen"), this, [this, tn](){
            emit deleteAreaRequested(tn);
        });
    } else if (kind == ProjectTreeModel::Line) {
        auto *tn = m_model->topologyNodeAt(idx);
        menu.addAction(tr("Umbenennen"), this, [this, idx](){
            m_topoView->edit(idx);
        });
        menu.addAction(tr("Löschen"), this, [this, tn](){
            emit deleteLineRequested(tn);
        });
    } else if (kind == ProjectTreeModel::Device) {
        auto *dev  = m_model->deviceAt(idx);
        auto *line = m_model->topologyNodeAt(idx.parent());
        menu.addAction(tr("Duplizieren"), this, [this, dev, line](){
            emit duplicateDeviceRequested(dev, line);
        });
        menu.addSeparator();
        menu.addAction(tr("Umbenennen"), this, [this, idx](){
            m_topoView->edit(idx);
        });
        menu.addAction(tr("Löschen"), this, [this, dev](){
            emit deleteDeviceRequested(dev);
        });
    }

    if (!menu.actions().isEmpty())
        menu.exec(m_topoView->viewport()->mapToGlobal(pos));
}

void ProjectTreeWidget::onGaContextMenu(const QPoint &pos)
{
    const QModelIndex idx = m_gaView->indexAt(pos);
    const ProjectTreeModel::ItemKind kind = idx.isValid()
                                            ? m_model->kindAt(idx)
                                            : ProjectTreeModel::GaRoot;

    QMenu menu(this);

    if (!idx.isValid() || kind == ProjectTreeModel::GaRoot) {
        menu.addAction(tr("Neue Hauptgruppe…"), this, [this](){
            emit addMainGroupRequested();
        });
    } else if (kind == ProjectTreeModel::MainGroup) {
        // Extract main group number from label (e.g. "2 Hauptgruppe" → 2)
        const QString label = m_model->data(idx, Qt::DisplayRole).toString();
        const int main = label.section(QLatin1Char(' '), 0, 0).toInt();
        menu.addAction(tr("Neue Mittelgruppe…"), this, [this, main](){
            emit addMiddleGroupRequested(main);
        });
    } else if (kind == ProjectTreeModel::MiddleGroup) {
        const QString label = m_model->data(idx, Qt::DisplayRole).toString();
        // Label is "M/MID Mittelgruppe"
        const QStringList parts = label.section(QLatin1Char(' '), 0, 0).split(QLatin1Char('/'));
        const int main = parts.value(0).toInt();
        const int mid  = parts.value(1).toInt();
        menu.addAction(tr("Neue Gruppenadresse…"), this, [this, main, mid](){
            emit addGroupAddressRequested(main, mid);
        });
    } else if (kind == ProjectTreeModel::GroupAddress) {
        auto *ga = m_model->groupAddressAt(idx);
        menu.addAction(tr("Umbenennen"), this, [this, idx](){
            m_gaView->edit(idx);
        });
        menu.addAction(tr("Löschen"), this, [this, ga](){
            emit deleteGroupAddressRequested(ga);
        });
    }

    if (!menu.actions().isEmpty())
        menu.exec(m_gaView->viewport()->mapToGlobal(pos));
}

void ProjectTreeWidget::onBuildingContextMenu(const QPoint &pos)
{
    const QModelIndex idx = m_buildingView->indexAt(pos);
    const ProjectTreeModel::ItemKind kind = idx.isValid()
                                            ? m_model->kindAt(idx)
                                            : ProjectTreeModel::BuildingRoot;

    QMenu menu(this);

    if (!idx.isValid() || kind == ProjectTreeModel::BuildingRoot) {
        menu.addAction(tr("Neues Gebäude…"), this, [this](){
            emit addBuildingRequested();
        });
    } else if (kind == ProjectTreeModel::BuildingNode) {
        auto *bp = m_model->buildingPartAt(idx);
        const BuildingPart::Type bpType = bp ? bp->type() : BuildingPart::Type::Building;

        if (bpType == BuildingPart::Type::Building) {
            menu.addAction(tr("Neue Etage…"), this, [this, bp](){
                emit addBuildingChildRequested(bp, BuildingPart::Type::Floor);
            });
        } else if (bpType == BuildingPart::Type::Floor) {
            menu.addAction(tr("Neuen Raum…"), this, [this, bp](){
                emit addBuildingChildRequested(bp, BuildingPart::Type::Room);
            });
        } else if (bpType == BuildingPart::Type::Room) {
            // Rooms can have further sub-rooms or other children
            menu.addAction(tr("Neuen Unterraum…"), this, [this, bp](){
                emit addBuildingChildRequested(bp, BuildingPart::Type::Room);
            });
        }
        menu.addSeparator();
        menu.addAction(tr("Umbenennen"), this, [this, idx](){
            m_buildingView->edit(idx);
        });
        menu.addAction(tr("Löschen"), this, [this, bp](){
            emit deleteBuildingPartRequested(bp);
        });
    }

    if (!menu.actions().isEmpty())
        menu.exec(m_buildingView->viewport()->mapToGlobal(pos));
}

void ProjectTreeWidget::onTopoKeyPressed(int key, const QModelIndex &index)
{
    if (key != Qt::Key_Delete || !index.isValid()) return;
    const auto kind = m_model->kindAt(index);
    if (kind == ProjectTreeModel::Area)
        emit deleteAreaRequested(m_model->topologyNodeAt(index));
    else if (kind == ProjectTreeModel::Line)
        emit deleteLineRequested(m_model->topologyNodeAt(index));
    else if (kind == ProjectTreeModel::Device)
        emit deleteDeviceRequested(m_model->deviceAt(index));
}

void ProjectTreeWidget::onGaKeyPressed(int key, const QModelIndex &index)
{
    if (key != Qt::Key_Delete || !index.isValid()) return;
    if (m_model->kindAt(index) == ProjectTreeModel::GroupAddress)
        emit deleteGroupAddressRequested(m_model->groupAddressAt(index));
}

void ProjectTreeWidget::onBuildingKeyPressed(int key, const QModelIndex &index)
{
    if (key != Qt::Key_Delete || !index.isValid()) return;
    if (m_model->kindAt(index) == ProjectTreeModel::BuildingNode)
        emit deleteBuildingPartRequested(m_model->buildingPartAt(index));
}
