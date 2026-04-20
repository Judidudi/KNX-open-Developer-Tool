#pragma once

#include <QWidget>
#include <memory>

class DeviceCatalog;
class CatalogModel;
struct Manifest;

class QListView;
class QLineEdit;
class QSortFilterProxyModel;
class QPushButton;

class CatalogWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CatalogWidget(QWidget *parent = nullptr);

    void setCatalog(DeviceCatalog *catalog);

    // Returns the manifest currently selected in the list (or nullptr)
    std::shared_ptr<Manifest> selectedManifest() const;

signals:
    // Emitted when the user double-clicks or presses the Add button
    void addDeviceRequested(std::shared_ptr<Manifest> manifest);

private slots:
    void onFilterChanged(const QString &text);
    void onAddClicked();
    void onActivated();

private:
    DeviceCatalog         *m_catalog    = nullptr;
    CatalogModel          *m_model      = nullptr;
    QSortFilterProxyModel *m_proxy      = nullptr;
    QListView             *m_view       = nullptr;
    QLineEdit             *m_filterEdit = nullptr;
    QPushButton           *m_addButton  = nullptr;
};
