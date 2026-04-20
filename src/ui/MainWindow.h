#pragma once

#include <QMainWindow>
#include <memory>

class Project;
class DeviceCatalog;
class DeviceInstance;
struct Manifest;

class ProjectTreeWidget;
class CatalogWidget;
class DeviceEditorWidget;
class BusMonitorWidget;

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
    void onAddDeviceRequested(std::shared_ptr<Manifest> manifest);
    void onDeviceSelected(DeviceInstance *device);

private:
    void setupMenuBar();
    void setupToolBar();
    void setupCentralWidget();
    void setupStatusBar();
    void loadCatalog();
    void updateWindowTitle();
    void markModified();
    bool maybeSave();

    std::unique_ptr<Project>       m_project;
    std::unique_ptr<DeviceCatalog> m_catalog;

    ProjectTreeWidget  *m_projectTree  = nullptr;
    CatalogWidget      *m_catalogView  = nullptr;
    DeviceEditorWidget *m_deviceEditor = nullptr;
    BusMonitorWidget   *m_busMonitor   = nullptr;
    QStackedWidget     *m_centerStack  = nullptr;

    QLabel  *m_connectionStatusLabel = nullptr;
    QString  m_currentFilePath;
    bool     m_modified = false;

    QAction *m_actSave          = nullptr;
    QAction *m_actSaveAs        = nullptr;
    QAction *m_actConnect       = nullptr;
    QAction *m_actProgram       = nullptr;
    QAction *m_actBusMonitor    = nullptr;
    QAction *m_actAddGroupAddr  = nullptr;
};
