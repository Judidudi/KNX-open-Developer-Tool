#include "MainWindow.h"

#include "ProjectTreeWidget.h"
#include "CatalogWidget.h"
#include "DeviceEditorWidget.h"
#include "BusMonitorWidget.h"
#include "dialogs/NewProjectDialog.h"
#include "dialogs/ConnectDialog.h"

#include "Project.h"
#include "ProjectXmlSerializer.h"

#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSplitter>
#include <QStackedWidget>
#include <QLabel>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_project = std::make_unique<Project>();

    setupCentralWidget();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    updateWindowTitle();

    resize(1200, 750);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupMenuBar()
{
    // --- Datei ---
    QMenu *fileMenu = menuBar()->addMenu(tr("&Datei"));

    QAction *actNew = fileMenu->addAction(tr("&Neu…"), this, &MainWindow::newProject);
    actNew->setShortcut(QKeySequence::New);

    QAction *actOpen = fileMenu->addAction(tr("&Öffnen…"), this, &MainWindow::openProject);
    actOpen->setShortcut(QKeySequence::Open);

    fileMenu->addSeparator();

    m_actSave = fileMenu->addAction(tr("&Speichern"), this, &MainWindow::saveProject);
    m_actSave->setShortcut(QKeySequence::Save);
    m_actSave->setEnabled(false);

    m_actSaveAs = fileMenu->addAction(tr("Speichern &unter…"), this, &MainWindow::saveProjectAs);
    m_actSaveAs->setShortcut(QKeySequence::SaveAs);
    m_actSaveAs->setEnabled(false);

    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Beenden"), qApp, &QApplication::quit, QKeySequence::Quit);

    // --- Projekt ---
    QMenu *projectMenu = menuBar()->addMenu(tr("&Projekt"));
    projectMenu->addAction(tr("Gerät hinzufügen"), this, [this](){
        // Placeholder – wird in Phase 2 implementiert
    });

    // --- Bus ---
    QMenu *busMenu = menuBar()->addMenu(tr("&Bus"));

    m_actConnect = busMenu->addAction(tr("Verbinden…"), this, [this](){
        ConnectDialog dlg(this);
        dlg.exec();
    });

    m_actBusMonitor = busMenu->addAction(tr("Busmonitor"), this, [this](){
        m_centerStack->setCurrentWidget(m_busMonitor);
    });

    busMenu->addSeparator();

    m_actProgram = busMenu->addAction(tr("Gerät programmieren…"), this, [this](){
        // Placeholder – wird in Phase 4 implementiert
    });

    // --- Hilfe ---
    QMenu *helpMenu = menuBar()->addMenu(tr("&Hilfe"));
    helpMenu->addAction(tr("Über KNX open Developer Tool…"), this, [this](){
        QMessageBox::about(this,
            tr("Über KNX open Developer Tool"),
            tr("<b>KNX open Developer Tool</b> v0.1.0<br><br>"
               "Open-Source-Alternative zum KNX-zertifizierten Ökosystem.<br>"
               "Lizenz: GPLv3"));
    });
}

void MainWindow::setupToolBar()
{
    QToolBar *tb = addToolBar(tr("Hauptwerkzeugleiste"));
    tb->setObjectName(QStringLiteral("mainToolBar"));
    tb->setMovable(false);

    tb->addAction(tr("Neu"), this, &MainWindow::newProject);
    tb->addAction(tr("Öffnen"), this, &MainWindow::openProject);
    tb->addAction(tr("Speichern"), this, &MainWindow::saveProject);
    tb->addSeparator();
    tb->addAction(m_actConnect);
    tb->addAction(m_actProgram);
}

void MainWindow::setupCentralWidget()
{
    // Left pane
    m_projectTree = new ProjectTreeWidget(this);

    // Center stack
    m_centerStack = new QStackedWidget(this);
    m_deviceEditor = new DeviceEditorWidget(this);
    m_busMonitor   = new BusMonitorWidget(this);
    m_centerStack->addWidget(m_deviceEditor);
    m_centerStack->addWidget(m_busMonitor);
    m_centerStack->setCurrentWidget(m_deviceEditor);

    // Right pane
    m_catalog = new CatalogWidget(this);

    // Horizontal splitter
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_projectTree);
    splitter->addWidget(m_centerStack);
    splitter->addWidget(m_catalog);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setStretchFactor(2, 1);
    splitter->setSizes({240, 720, 240});

    setCentralWidget(splitter);
}

void MainWindow::setupStatusBar()
{
    m_connectionStatusLabel = new QLabel(tr("● Nicht verbunden"), this);
    statusBar()->addPermanentWidget(m_connectionStatusLabel);
    statusBar()->showMessage(tr("Bereit"));
}

void MainWindow::updateWindowTitle()
{
    QString title = QStringLiteral("KNX open Developer Tool");
    if (!m_project->name().isEmpty())
        title = m_project->name() + QStringLiteral(" – ") + title;
    if (m_modified)
        title.prepend(QStringLiteral("* "));
    setWindowTitle(title);
}

void MainWindow::newProject()
{
    if (!maybeSave())
        return;

    NewProjectDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    m_project = std::make_unique<Project>();
    m_project->setName(dlg.projectName());
    m_currentFilePath.clear();
    m_modified = false;

    m_projectTree->setProject(m_project.get());
    m_actSave->setEnabled(true);
    m_actSaveAs->setEnabled(true);
    updateWindowTitle();
    statusBar()->showMessage(tr("Neues Projekt angelegt"));
}

void MainWindow::openProject()
{
    if (!maybeSave())
        return;

    const QString path = QFileDialog::getOpenFileName(
        this, tr("Projekt öffnen"), QString(),
        tr("KNX open Developer Tool Projekte (*.kodtproj);;Alle Dateien (*)"));

    if (path.isEmpty())
        return;

    auto loaded = ProjectXmlSerializer::load(path);
    if (!loaded) {
        QMessageBox::critical(this, tr("Fehler"),
            tr("Projekt konnte nicht geöffnet werden:\n%1").arg(path));
        return;
    }

    m_project = std::move(loaded);
    m_currentFilePath = path;
    m_modified = false;

    m_projectTree->setProject(m_project.get());
    m_actSave->setEnabled(true);
    m_actSaveAs->setEnabled(true);
    updateWindowTitle();
    statusBar()->showMessage(tr("Projekt geladen: %1").arg(path));
}

void MainWindow::saveProject()
{
    if (m_currentFilePath.isEmpty()) {
        saveProjectAs();
        return;
    }
    if (!ProjectXmlSerializer::save(*m_project, m_currentFilePath)) {
        QMessageBox::critical(this, tr("Fehler"),
            tr("Projekt konnte nicht gespeichert werden:\n%1").arg(m_currentFilePath));
        return;
    }
    m_modified = false;
    updateWindowTitle();
    statusBar()->showMessage(tr("Gespeichert: %1").arg(m_currentFilePath));
}

void MainWindow::saveProjectAs()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Projekt speichern unter"), QString(),
        tr("KNX open Developer Tool Projekte (*.kodtproj);;Alle Dateien (*)"));

    if (path.isEmpty())
        return;

    m_currentFilePath = path;
    saveProject();
}

bool MainWindow::maybeSave()
{
    if (!m_modified)
        return true;

    const auto btn = QMessageBox::question(
        this, tr("Ungespeicherte Änderungen"),
        tr("Das Projekt wurde geändert. Möchten Sie die Änderungen speichern?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (btn == QMessageBox::Save) {
        saveProject();
        return true;
    }
    return btn != QMessageBox::Cancel;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave())
        event->accept();
    else
        event->ignore();
}
