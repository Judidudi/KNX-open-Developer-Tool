#pragma once

#include <QAbstractListModel>
#include <memory>

class KnxprodCatalog;
class KnxApplicationProgram;
struct KnxHardwareProduct;

// Flat list model over a KnxprodCatalog.
// Each row represents one hardware product; shows product name + manufacturer.
class CatalogModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        ProductIdRole  = Qt::UserRole + 1,
        ProductPtrRole,
    };

    explicit CatalogModel(QObject *parent = nullptr);
    ~CatalogModel() override;

    void            setCatalog(KnxprodCatalog *catalog);
    KnxprodCatalog *catalog() const { return m_catalog; }

    void reload();

    // Returns product entry at given model index (or nullptr)
    const KnxHardwareProduct *productAt(const QModelIndex &index) const;

    // QAbstractListModel
    int      rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private:
    KnxprodCatalog *m_catalog = nullptr;
};
