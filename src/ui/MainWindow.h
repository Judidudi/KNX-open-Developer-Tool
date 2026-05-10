#pragma once

#include "BuildingPart.h"
#include "KnxUsbMonitor.h"
#include <QMainWindow>
#include <QHash>
#include <memory>

class KnxdManager;

class Project;
class KnxprodCatalog;
class KnxApplicationProgram;
class DeviceInstance;
class GroupAddress;
class TopologyNode;

class InterfaceManager;
class ProjectTreeWidget;
class DeviceEditorWidget;
class BusMonitorWidget;
class GroupMonitorWidget;
class PropertiesPanel;

class QTabWidget;
class QLabel;
class QAction;
class QUndoStack;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void newProject();
    void openProject();
    void saveProject();
    void saveProjectAs();
    void addGroupAddress();
    void onAddDeviceRequested(const QString &productId,
                              const QString &productName,
                              std::shared_ptr<KnxApplicationProgram> appProgram);
    void onDeviceSelected(DeviceInstance *device);
    void onGroupAddressSelected(GroupAddress *ga);
    void onConnectClicked();
    void onDisconnectClicked();
    void onLineScanClicked();
    void onProgramClicked();
    void onShowBusMonitor();
    void onShowGroupMonitor();
    void onImportCatalogFile();
    void onImportGaCsv();
    void onExportGaCsv();
    void onOpenRecentFile(const QString &path);
    void onInterfaceConnected();
    void onInterfaceDisconnected();
    void onInterfaceError(const QString &message);
    void onTabCloseRequested(int index);

    // G1: Topology management
    void onAddAreaRequested();
    void onAddLineRequested(TopologyNode *area);
    void onDeleteAreaRequested(TopologyNode *area);
    void onDeleteLineRequested(TopologyNode *line);
    void onDeleteDeviceRequested(DeviceInstance *dev);
    void onDuplicateDeviceRequested(DeviceInstance *dev, TopologyNode *line);

    // G1: Group address management
    void onAddMainGroupRequested();
    void onAddMiddleGroupRequested(int mainGroup);
    void onAddGroupAddressRequested(int mainGroup, int middleGroup);
    void onDeleteGroupAddressRequested(GroupAddress *ga);

    // G2: Building management
    void onAddBuildingRequested();
    void onAddBuildingChildRequested(BuildingPart *parent, BuildingPart::Type childType);
    void onDeleteBuildingPartRequested(BuildingPart *bp);

    // AA: USB auto-discovery
    void onUsbDeviceFound(KnxUsbMonitor::DeviceInfo info);
    void onUsbDeviceRemoved(QString path);

    // AC: knxd backend
    void onKnxdReady();
    void onKnxdStopped();

private slots:
    void startCatalogLoad();

private:
    void setupMenuBar();
    void setupToolBar();
    void setupCentralWidget();
    void setupStatusBar();
    void updateWindowTitle();
    void markModified();
    bool maybeSave();
    void updateConnectionUi();
    void refreshGroupMonitor();
    void addToRecentFiles(const QString &path);
    void updateRecentFilesMenu();
    void closeEditorFor(DeviceInstance *dev);
    void closeAllEditorTabs();

    std::unique_ptr<Project>          m_project;
    std::unique_ptr<KnxprodCatalog>   m_catalog;
    std::unique_ptr<InterfaceManager> m_interfaces;

    KnxUsbMonitor *m_usbMonitor       = nullptr;
    KnxdManager   *m_knxdManager     = nullptr;  // nullptr when knxd not installed
    bool           m_udevPromptShown  = false;
    QString        m_autoConnectedPath;
    QString        m_autoConnectedName;

    ProjectTreeWidget  *m_projectTree     = nullptr;
    BusMonitorWidget   *m_busMonitor      = nullptr;
    GroupMonitorWidget *m_groupMonitor    = nullptr;
    QTabWidget         *m_centerTabs      = nullptr;
    PropertiesPanel    *m_propertiesPanel = nullptr;

    // Maps open device editors by device pointer
    QHash<DeviceInstance*, DeviceEditorWidget*> m_openEditors;

    QLabel      *m_connectionStatusLabel = nullptr;
    QString      m_currentFilePath;
    QString      m_writableCatalogPath;
    bool         m_modified = false;
    QUndoStack  *m_undoStack = nullptr;

    DeviceInstance *m_selectedDevice = nullptr;

    QAction *m_actSave           = nullptr;
    QAction *m_actSaveAs         = nullptr;
    QAction *m_actConnect        = nullptr;
    QAction *m_actDisconnect     = nullptr;
    QAction *m_actLineScan       = nullptr;
    QAction *m_actProgram        = nullptr;
    QAction *m_actBusMonitor     = nullptr;
    QAction *m_actGroupMonitor   = nullptr;
    QAction *m_actAddGroupAddr   = nullptr;

    QMenu   *m_recentMenu        = nullptr;
};
