#include "ProjectTreeWidget.h"
#include "ProjectTreeModel.h"
#include "CatalogWidget.h"

#include <QTabWidget>
#include <QTreeView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QItemSelectionModel>

static void setupTreeView(QTreeView *view)
{
    view->setRootIsDecorated(true);
    view->setHeaderHidden(true);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setAnimated(true);
    view->setIndentation(16);
}

ProjectTreeWidget::ProjectTreeWidget(QWidget *parent)
    : QWidget(parent)
    , m_tabs(new QTabWidget(this))
    , m_topoView(new QTreeView(this))
    , m_gaView(new QTreeView(this))
    , m_catWidget(new CatalogWidget(this))
    , m_model(new ProjectTreeModel(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_tabs);

    ::setupTreeView(m_topoView);
    ::setupTreeView(m_gaView);

    m_topoView->setModel(m_model);
    m_gaView->setModel(m_model);

    // Only show the label column (column 1 = "details" is hidden in nav views)
    m_topoView->setColumnHidden(1, true);
    m_gaView->setColumnHidden(1, true);

    m_tabs->addTab(m_topoView,   tr("Topologie"));
    m_tabs->addTab(m_gaView,     tr("Gruppen"));
    m_tabs->addTab(m_catWidget,  tr("Katalog"));

    connect(m_topoView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ProjectTreeWidget::onTopoSelectionChanged);
    connect(m_gaView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ProjectTreeWidget::onGaSelectionChanged);

    // Forward catalog signal up to MainWindow
    connect(m_catWidget, &CatalogWidget::addDeviceRequested,
            this,        &ProjectTreeWidget::addDeviceRequested);
}

void ProjectTreeWidget::setProject(Project *project)
{
    m_model->setProject(project);
    updateRootIndices();
}

void ProjectTreeWidget::setCatalog(DeviceCatalog *catalog)
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
    // Row 0 = Topology root, row 1 = GA root (see ProjectTreeModel::rebuild())
    // setRootIndex hides the root item and shows its children as top-level.
    const QModelIndex topoRoot = m_model->index(0, 0, {});
    const QModelIndex gaRoot   = m_model->index(1, 0, {});
    m_topoView->setRootIndex(topoRoot);
    m_gaView->setRootIndex(gaRoot);
    m_topoView->expandAll();
    m_gaView->expandAll();
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
