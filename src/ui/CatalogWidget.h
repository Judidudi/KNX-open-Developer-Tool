#pragma once

#include <QWidget>
#include <memory>

class KnxprodCatalog;
class KnxApplicationProgram;
struct KnxHardwareProduct;
class CatalogModel;

class QListView;
class QLineEdit;
class QSortFilterProxyModel;
class QPushButton;

class CatalogWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CatalogWidget(QWidget *parent = nullptr);

    void setCatalog(KnxprodCatalog *catalog);

    // Returns the product currently selected in the list (or nullptr)
    const KnxHardwareProduct *selectedProduct() const;

signals:
    // Emitted when the user double-clicks or presses the Add button
    void addDeviceRequested(const QString &productId,
                            const QString &productName,
                            std::shared_ptr<KnxApplicationProgram> appProgram);

private slots:
    void onFilterChanged(const QString &text);
    void onAddClicked();
    void onActivated();

private:
    KnxprodCatalog        *m_catalog    = nullptr;
    CatalogModel          *m_model      = nullptr;
    QSortFilterProxyModel *m_proxy      = nullptr;
    QListView             *m_view       = nullptr;
    QLineEdit             *m_filterEdit = nullptr;
    QPushButton           *m_addButton  = nullptr;
};
