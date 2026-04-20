#pragma once

#include <QMainWindow>

class Project;
class ProjectTreeWidget;
class CatalogWidget;
class DeviceEditorWidget;
class BusMonitorWidget;

class QStackedWidget;
class QSplitter;
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

private:
    void setupMenuBar();
    void setupToolBar();
    void setupCentralWidget();
    void setupStatusBar();
    void updateWindowTitle();
    bool maybeSave();

    std::unique_ptr<Project> m_project;

    ProjectTreeWidget  *m_projectTree  = nullptr;
    CatalogWidget      *m_catalog      = nullptr;
    DeviceEditorWidget *m_deviceEditor = nullptr;
    BusMonitorWidget   *m_busMonitor   = nullptr;
    QStackedWidget     *m_centerStack  = nullptr;

    QLabel  *m_connectionStatusLabel = nullptr;
    QString  m_currentFilePath;
    bool     m_modified = false;

    // Actions
    QAction *m_actSave     = nullptr;
    QAction *m_actSaveAs   = nullptr;
    QAction *m_actConnect  = nullptr;
    QAction *m_actProgram  = nullptr;
    QAction *m_actBusMonitor = nullptr;
};
