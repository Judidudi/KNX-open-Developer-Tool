#pragma once

#include <QAbstractListModel>
#include <memory>

class DeviceCatalog;
struct Manifest;

// Flat list model over a DeviceCatalog.
// Each row represents one device manifest; shows localized name + manufacturer.
class CatalogModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        ManifestIdRole = Qt::UserRole + 1,
        ManifestPtrRole,
    };

    explicit CatalogModel(QObject *parent = nullptr);
    ~CatalogModel() override;

    void           setCatalog(DeviceCatalog *catalog);
    DeviceCatalog *catalog() const { return m_catalog; }

    void reload();

    // Convenience: returns manifest at given model index (or nullptr)
    std::shared_ptr<Manifest> manifestAt(const QModelIndex &index) const;

    // QAbstractListModel
    int      rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private:
    DeviceCatalog *m_catalog = nullptr;
};
