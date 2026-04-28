#pragma once

#include <QMainWindow>
#include <memory>

class Project;
class KnxprodCatalog;
class KnxApplicationProgram;
class DeviceInstance;
class GroupAddress;

class InterfaceManager;
class ProjectTreeWidget;
class DeviceEditorWidget;
class BusMonitorWidget;
class PropertiesPanel;

class QStackedWidget;
class QLabel;
class QAction;

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
    void onProgramClicked();
    void onShowBusMonitor();
    void onInterfaceConnected();
    void onInterfaceDisconnected();
    void onInterfaceError(const QString &message);

private:
    void setupMenuBar();
    void setupToolBar();
    void setupCentralWidget();
    void setupStatusBar();
    void loadCatalog();
    void updateWindowTitle();
    void markModified();
    bool maybeSave();
    void updateConnectionUi();

    std::unique_ptr<Project>          m_project;
    std::unique_ptr<KnxprodCatalog>   m_catalog;
    std::unique_ptr<InterfaceManager> m_interfaces;

    ProjectTreeWidget  *m_projectTree     = nullptr;
    DeviceEditorWidget *m_deviceEditor    = nullptr;
    BusMonitorWidget   *m_busMonitor      = nullptr;
    QStackedWidget     *m_centerStack     = nullptr;
    PropertiesPanel    *m_propertiesPanel = nullptr;

    QLabel  *m_connectionStatusLabel = nullptr;
    QString  m_currentFilePath;
    bool     m_modified = false;

    DeviceInstance *m_selectedDevice = nullptr;

    QAction *m_actSave         = nullptr;
    QAction *m_actSaveAs       = nullptr;
    QAction *m_actConnect      = nullptr;
    QAction *m_actDisconnect   = nullptr;
    QAction *m_actProgram      = nullptr;
    QAction *m_actBusMonitor   = nullptr;
    QAction *m_actAddGroupAddr = nullptr;
};
