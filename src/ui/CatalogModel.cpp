#include "CatalogModel.h"

#include "KnxprodCatalog.h"

CatalogModel::CatalogModel(QObject *parent)
    : QAbstractListModel(parent)
{}

CatalogModel::~CatalogModel() = default;

void CatalogModel::setCatalog(KnxprodCatalog *catalog)
{
    beginResetModel();
    m_catalog = catalog;
    endResetModel();
}

void CatalogModel::reload()
{
    beginResetModel();
    if (m_catalog)
        m_catalog->reload();
    endResetModel();
}

const KnxHardwareProduct *CatalogModel::productAt(const QModelIndex &index) const
{
    if (!m_catalog || !index.isValid())
        return nullptr;
    return m_catalog->at(index.row());
}

int CatalogModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !m_catalog)
        return 0;
    return m_catalog->count();
}

QVariant CatalogModel::data(const QModelIndex &index, int role) const
{
    if (!m_catalog || !index.isValid())
        return {};
    const KnxHardwareProduct *p = m_catalog->at(index.row());
    if (!p)
        return {};

    switch (role) {
    case Qt::DisplayRole:
        return QStringLiteral("%1\n%2 · %3")
            .arg(p->productName, p->manufacturer, p->productId);
    case Qt::ToolTipRole:
        return tr("Produkt-ID: %1\nApp-Programm: %2")
            .arg(p->productId, p->appProgramRefId);
    case ProductIdRole:
        return p->productId;
    case ProductPtrRole:
        return QVariant::fromValue(const_cast<KnxHardwareProduct *>(p));
    }
    return {};
}
