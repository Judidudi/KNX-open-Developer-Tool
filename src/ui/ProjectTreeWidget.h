#pragma once

#include <QWidget>
#include <memory>

class Project;
class ProjectTreeModel;
class DeviceInstance;
class GroupAddress;
class CatalogWidget;
class KnxprodCatalog;
class KnxApplicationProgram;

class QTabWidget;
class QTreeView;

class ProjectTreeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ProjectTreeWidget(QWidget *parent = nullptr);

    void setProject(Project *project);
    void setCatalog(KnxprodCatalog *catalog);
    void refresh();

    ProjectTreeModel *model() const { return m_model; }

signals:
    void deviceSelected(DeviceInstance *device);
    void groupAddressSelected(GroupAddress *ga);
    void selectionCleared();
    void addDeviceRequested(const QString &productId,
                            const QString &productName,
                            std::shared_ptr<KnxApplicationProgram> appProgram);

private slots:
    void onTopoSelectionChanged();
    void onGaSelectionChanged();

private:
    void updateRootIndices();
    static void setupTreeView(QTreeView *view);

    QTabWidget       *m_tabs     = nullptr;
    QTreeView        *m_topoView = nullptr;
    QTreeView        *m_gaView   = nullptr;
    CatalogWidget    *m_catWidget = nullptr;
    ProjectTreeModel *m_model    = nullptr;
};
