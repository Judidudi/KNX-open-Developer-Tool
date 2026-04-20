#include "ProjectTreeWidget.h"
#include "ProjectTreeModel.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QTreeView>
#include <QHeaderView>
#include <QItemSelectionModel>

ProjectTreeWidget::ProjectTreeWidget(QWidget *parent)
    : QWidget(parent)
    , m_view(new QTreeView(this))
    , m_model(new ProjectTreeModel(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto *header = new QLabel(tr("Projekt-Browser"), this);
    header->setStyleSheet(QStringLiteral("font-weight: bold; padding: 4px;"));
    layout->addWidget(header);

    m_view->setModel(m_model);
    m_view->setRootIsDecorated(true);
    m_view->setHeaderHidden(false);
    m_view->header()->setStretchLastSection(true);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(m_view);

    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ProjectTreeWidget::onSelectionChanged);
}

void ProjectTreeWidget::setProject(Project *project)
{
    m_model->setProject(project);
    m_view->expandAll();
}

void ProjectTreeWidget::refresh()
{
    m_model->setProject(m_model->project());
    m_view->expandAll();
}

void ProjectTreeWidget::onSelectionChanged()
{
    const QModelIndexList sel = m_view->selectionModel()->selectedIndexes();
    if (sel.isEmpty()) {
        emit selectionCleared();
        return;
    }

    const QModelIndex idx = sel.first();
    if (DeviceInstance *dev = m_model->deviceAt(idx)) {
        emit deviceSelected(dev);
        return;
    }
    if (::GroupAddress *ga = m_model->groupAddressAt(idx)) {
        emit groupAddressSelected(ga);
        return;
    }
    emit selectionCleared();
}
