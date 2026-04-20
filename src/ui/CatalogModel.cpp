#include "CatalogModel.h"

#include "DeviceCatalog.h"
#include "Manifest.h"

CatalogModel::CatalogModel(QObject *parent)
    : QAbstractListModel(parent)
{}

CatalogModel::~CatalogModel() = default;

void CatalogModel::setCatalog(DeviceCatalog *catalog)
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

std::shared_ptr<Manifest> CatalogModel::manifestAt(const QModelIndex &index) const
{
    if (!m_catalog || !index.isValid())
        return nullptr;
    const Manifest *m = m_catalog->at(index.row());
    return m ? m_catalog->sharedById(m->id) : nullptr;
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
    const Manifest *m = m_catalog->at(index.row());
    if (!m)
        return {};

    switch (role) {
    case Qt::DisplayRole:
        return QStringLiteral("%1\n%2 · v%3")
            .arg(m->name.get(), m->manufacturer, m->version);
    case Qt::ToolTipRole:
        return tr("ID: %1\nHardware: %2 (%3)")
            .arg(m->id, m->hardware.target, m->hardware.transceiver);
    case ManifestIdRole:
        return m->id;
    case ManifestPtrRole:
        return QVariant::fromValue(const_cast<Manifest *>(m));
    }
    return {};
}
