#pragma once

#include "BuildingPart.h"
#include <QWidget>
#include <memory>

class Project;
class ProjectTreeModel;
class DeviceInstance;
class GroupAddress;
class TopologyNode;
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

    // G1: Topology management
    void addAreaRequested();
    void addLineRequested(TopologyNode *area);
    void deleteAreaRequested(TopologyNode *area);
    void deleteLineRequested(TopologyNode *line);
    void deleteDeviceRequested(DeviceInstance *dev);

    // G1: Group address management
    void addMainGroupRequested();
    void addMiddleGroupRequested(int mainGroup);
    void addGroupAddressRequested(int mainGroup, int middleGroup);
    void deleteGroupAddressRequested(GroupAddress *ga);

    // G2: Building management
    void addBuildingRequested();
    void addBuildingChildRequested(BuildingPart *parent, BuildingPart::Type childType);
    void deleteBuildingPartRequested(BuildingPart *bp);

    // Signals that something was modified (rename etc.)
    void projectModified();

public slots:
    // Called by the key event filters installed on each tree view
    void onTopoKeyPressed(int key, const QModelIndex &index);
    void onGaKeyPressed(int key, const QModelIndex &index);
    void onBuildingKeyPressed(int key, const QModelIndex &index);

private slots:
    void onTopoSelectionChanged();
    void onGaSelectionChanged();
    void onTopoContextMenu(const QPoint &pos);
    void onGaContextMenu(const QPoint &pos);
    void onBuildingContextMenu(const QPoint &pos);

private:
    void updateRootIndices();
    static void setupTreeView(QTreeView *view);
    void installKeyFilter(QTreeView *view);

    QTabWidget       *m_tabs         = nullptr;
    QTreeView        *m_topoView     = nullptr;
    QTreeView        *m_gaView       = nullptr;
    QTreeView        *m_buildingView = nullptr;
    CatalogWidget    *m_catWidget    = nullptr;
    ProjectTreeModel *m_model        = nullptr;
};
