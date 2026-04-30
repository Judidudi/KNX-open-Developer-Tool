#include "CatalogWidget.h"

#include "CatalogModel.h"
#include "KnxprodCatalog.h"
#include "KnxApplicationProgram.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QListView>
#include <QLineEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>

CatalogWidget::CatalogWidget(QWidget *parent)
    : QWidget(parent)
    , m_model(new CatalogModel(this))
    , m_proxy(new QSortFilterProxyModel(this))
    , m_view(new QListView(this))
    , m_filterEdit(new QLineEdit(this))
    , m_addButton(new QPushButton(tr("Zur Topologie hinzufügen"), this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto *header = new QLabel(tr("Geräte-Katalog"), this);
    header->setStyleSheet(QStringLiteral("font-weight: bold; padding: 4px;"));
    layout->addWidget(header);

    m_filterEdit->setPlaceholderText(tr("Filter…"));
    layout->addWidget(m_filterEdit);

    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_view->setModel(m_proxy);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setUniformItemSizes(false);
    layout->addWidget(m_view);

    m_addButton->setEnabled(false);
    layout->addWidget(m_addButton);

    connect(m_filterEdit, &QLineEdit::textChanged, this, &CatalogWidget::onFilterChanged);
    connect(m_addButton,  &QPushButton::clicked,   this, &CatalogWidget::onAddClicked);
    connect(m_view, &QListView::doubleClicked, this, [this](const QModelIndex &){ onActivated(); });
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this](){ m_addButton->setEnabled(selectedProduct() != nullptr); });
}

void CatalogWidget::setCatalog(KnxprodCatalog *catalog)
{
    m_catalog = catalog;
    m_model->setCatalog(catalog);
}

const KnxHardwareProduct *CatalogWidget::selectedProduct() const
{
    const QModelIndexList sel = m_view->selectionModel()->selectedIndexes();
    if (sel.isEmpty())
        return nullptr;
    const QModelIndex src = m_proxy->mapToSource(sel.first());
    return m_model->productAt(src);
}

void CatalogWidget::onFilterChanged(const QString &text)
{
    m_proxy->setFilterFixedString(text);
}

void CatalogWidget::onAddClicked()
{
    onActivated();
}

void CatalogWidget::onActivated()
{
    const KnxHardwareProduct *p = selectedProduct();
    if (p && p->appProgram)
        emit addDeviceRequested(p->productId, p->productName, p->appProgram);
}
