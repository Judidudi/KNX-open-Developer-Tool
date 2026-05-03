#include "MainWindow.h"

#include "ProjectTreeWidget.h"
#include "DeviceEditorWidget.h"
#include "BusMonitorWidget.h"
#include "GroupMonitorWidget.h"
#include "UndoCommands.h"
#include "ProgramDialog.h"
#include "PropertiesPanel.h"
#include "dialogs/NewProjectDialog.h"
#include "dialogs/ConnectDialog.h"
#include "dialogs/GroupAddressDialog.h"
#include "dialogs/LineScanDialog.h"
#include "dialogs/GaCsvImportDialog.h"
#include "dialogs/BatchProgramDialog.h"
#include "dialogs/SettingsDialog.h"

#include "Project.h"
#include "TopologyNode.h"
#include "DeviceInstance.h"
#include "BuildingPart.h"
#include "KnxprodCatalog.h"
#include "KnxApplicationProgram.h"
#include "KnxprojSerializer.h"
#include "GroupAddress.h"

#include "InterfaceManager.h"
#include "IKnxInterface.h"
#include "DeviceProgrammer.h"
#include "ConnectInterfaceFactory.h"

#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QUndoStack>
#include <QSplitter>
#include <QStackedWidget>
#include <QLabel>
#include <QAction>
#include <QDockWidget>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QSettings>
#include <QFileInfo>
#include <QCoreApplication>
#include <QThread>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_project    = std::make_unique<Project>();
    m_catalog    = std::make_unique<KnxprodCatalog>();
    m_interfaces = std::make_unique<InterfaceManager>();
    m_undoStack  = new QUndoStack(this);

    setupCentralWidget();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();

    m_projectTree->setProject(m_project.get());
    m_projectTree->setCatalog(m_catalog.get());  // empty catalog initially
    m_busMonitor->setInterfaceManager(m_interfaces.get());
    m_busMonitor->setProject(m_project.get());
    m_busMonitor->applySettings();
    m_groupMonitor->setInterfaceManager(m_interfaces.get());
    m_groupMonitor->setProject(m_project.get());
    m_propertiesPanel->setProject(m_project.get());
    m_propertiesPanel->setInterfaceManager(m_interfaces.get());
    m_deviceEditor->setInterfaceManager(m_interfaces.get());

    connect(m_interfaces.get(), &InterfaceManager::connected,
            this, &MainWindow::onInterfaceConnected);
    connect(m_interfaces.get(), &InterfaceManager::disconnected,
            this, &MainWindow::onInterfaceDisconnected);
    connect(m_interfaces.get(), &InterfaceManager::errorOccurred,
            this, &MainWindow::onInterfaceError);

    updateConnectionUi();
    updateWindowTitle();
    resize(1280, 800);

    // Deferred startup: start catalog load and (optionally) open last project
    // after the window is shown so the UI is never blank while loading.
    QTimer::singleShot(0, this, [this]() {
        startCatalogLoad();
        const QSettings s;
        if (s.value(QStringLiteral("project/openLastOnStart"), false).toBool()) {
            const QStringList recent = s.value(QStringLiteral("recentFiles")).toStringList();
            if (!recent.isEmpty() && QFile::exists(recent.first()))
                onOpenRecentFile(recent.first());
        }
    });
}

MainWindow::~MainWindow() = default;

void MainWindow::startCatalogLoad()
{
    const QDir appDir(QCoreApplication::applicationDirPath());
    m_writableCatalogPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                            + QStringLiteral("/catalog/devices");

    QStringList searchPaths;
    const QString envPath = qEnvironmentVariable("KNXODT_CATALOG_PATH");
    if (!envPath.isEmpty())
        searchPaths << envPath;
    searchPaths << appDir.absoluteFilePath(QStringLiteral("catalog/devices"))
                << appDir.absoluteFilePath(QStringLiteral("../catalog/devices"))
                << appDir.absoluteFilePath(QStringLiteral("../../catalog/devices"))
                << m_writableCatalogPath;

    // Create a fresh catalog exclusively for the background thread.
    // The main thread's m_catalog stays empty (but valid) until loading is done.
    auto *fresh = new KnxprodCatalog;
    for (const QString &path : searchPaths) {
        QDir().mkpath(path);
        fresh->addSearchPath(path);
    }

    statusBar()->showMessage(tr("Lade Katalog…"));

    auto *thread = QThread::create([fresh]() { fresh->reload(); });
    connect(thread, &QThread::finished, this, [this, fresh, thread]() {
        thread->deleteLater();
        m_projectTree->setCatalog(nullptr);  // detach model before swap
        m_catalog.reset(fresh);
        m_projectTree->setCatalog(m_catalog.get());
        if (m_catalog->count() > 0)
            statusBar()->showMessage(tr("Katalog: %1 Produkt(e)").arg(m_catalog->count()));
        else
            statusBar()->showMessage(tr("Kein Katalog – .knxprod-Datei in catalog/devices/ ablegen"));
        if (!m_catalog->lastErrors().isEmpty())
            qWarning() << "Catalog load errors:" << m_catalog->lastErrors();
    });
    thread->start();
}

void MainWindow::setupMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&Datei"));

    QAction *actNew = fileMenu->addAction(tr("&Neu…"), this, &MainWindow::newProject);
    actNew->setShortcut(QKeySequence::New);

    QAction *actOpen = fileMenu->addAction(tr("&Öffnen…"), this, &MainWindow::openProject);
    actOpen->setShortcut(QKeySequence::Open);

    fileMenu->addSeparator();

    m_actSave = fileMenu->addAction(tr("&Speichern"), this, &MainWindow::saveProject);
    m_actSave->setShortcut(QKeySequence::Save);

    m_actSaveAs = fileMenu->addAction(tr("Speichern &unter…"), this, &MainWindow::saveProjectAs);
    m_actSaveAs->setShortcut(QKeySequence::SaveAs);

    fileMenu->addSeparator();
    m_recentMenu = fileMenu->addMenu(tr("Zuletzt geöffnet"));
    updateRecentFilesMenu();
    fileMenu->addSeparator();
    fileMenu->addAction(tr("Katalogdatei &importieren…"), this, &MainWindow::onImportCatalogFile);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Beenden"), qApp, &QApplication::quit, QKeySequence::Quit);

    QMenu *editMenu = menuBar()->addMenu(tr("&Bearbeiten"));
    editMenu->addAction(m_undoStack->createUndoAction(this, tr("&Rückgängig")));
    editMenu->addAction(m_undoStack->createRedoAction(this, tr("&Wiederholen")));
    editMenu->actions().at(0)->setShortcut(QKeySequence::Undo);
    editMenu->actions().at(1)->setShortcut(QKeySequence::Redo);

    QMenu *projectMenu = menuBar()->addMenu(tr("&Projekt"));
    m_actAddGroupAddr = projectMenu->addAction(tr("Gruppenadresse hinzufügen…"),
                                               this, &MainWindow::addGroupAddress);
    projectMenu->addAction(tr("Gruppenadressen aus CSV importieren…"),
                           this, &MainWindow::onImportGaCsv);
    projectMenu->addAction(tr("Gruppenadressen als CSV exportieren…"),
                           this, &MainWindow::onExportGaCsv);

    QMenu *busMenu = menuBar()->addMenu(tr("&Bus"));
    m_actConnect    = busMenu->addAction(tr("&Verbinden…"),     this, &MainWindow::onConnectClicked);
    m_actDisconnect = busMenu->addAction(tr("&Trennen"),         this, &MainWindow::onDisconnectClicked);
    m_actBusMonitor   = busMenu->addAction(tr("&Busmonitor"),          this, &MainWindow::onShowBusMonitor);
    m_actGroupMonitor = busMenu->addAction(tr("&Gruppenadress-Monitor"), this, &MainWindow::onShowGroupMonitor);
    busMenu->addSeparator();
    m_actLineScan     = busMenu->addAction(tr("&Leitungsscan…"),        this, &MainWindow::onLineScanClicked);
    busMenu->addSeparator();
    m_actProgram      = busMenu->addAction(tr("Gerät &programmieren…"), this, &MainWindow::onProgramClicked);

    QMenu *extrasMenu = menuBar()->addMenu(tr("&Extras"));
    extrasMenu->addAction(tr("&Einstellungen…"), this, [this](){
        SettingsDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted)
            m_busMonitor->applySettings();
    });

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

    tb->addAction(tr("Neu"),       this, &MainWindow::newProject);
    tb->addAction(tr("Öffnen"),    this, &MainWindow::openProject);
    tb->addAction(tr("Speichern"), this, &MainWindow::saveProject);
    tb->addSeparator();
    tb->addAction(m_actAddGroupAddr);
    tb->addSeparator();
    tb->addAction(m_actConnect);
    tb->addAction(m_actDisconnect);
    tb->addAction(m_actBusMonitor);
    tb->addAction(m_actProgram);
}

void MainWindow::setupCentralWidget()
{
    m_projectTree  = new ProjectTreeWidget(this);
    m_deviceEditor = new DeviceEditorWidget(this);
    m_busMonitor   = new BusMonitorWidget(this);
    m_groupMonitor = new GroupMonitorWidget(this);

    m_centerStack = new QStackedWidget(this);
    m_centerStack->addWidget(m_deviceEditor);
    m_centerStack->addWidget(m_busMonitor);
    m_centerStack->addWidget(m_groupMonitor);
    m_centerStack->setCurrentWidget(m_deviceEditor);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_projectTree);
    splitter->addWidget(m_centerStack);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({300, 980});
    setCentralWidget(splitter);

    // Properties dock on the right
    m_propertiesPanel = new PropertiesPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesPanel);

    connect(m_projectTree, &ProjectTreeWidget::deviceSelected,
            this, &MainWindow::onDeviceSelected);
    connect(m_projectTree, &ProjectTreeWidget::groupAddressSelected,
            this, &MainWindow::onGroupAddressSelected);
    connect(m_projectTree, &ProjectTreeWidget::selectionCleared,
            this, [this]() {
                m_deviceEditor->clearDevice();
                m_propertiesPanel->clearSelection();
            });
    connect(m_projectTree, &ProjectTreeWidget::addDeviceRequested,
            this, &MainWindow::onAddDeviceRequested);
    connect(m_deviceEditor, &DeviceEditorWidget::deviceModified,
            this, [this](){ markModified(); });

    connect(m_propertiesPanel, &PropertiesPanel::deviceModified,
            this, [this]() {
                m_projectTree->refresh();
                markModified();
            });
    connect(m_propertiesPanel, &PropertiesPanel::groupAddressModified,
            this, [this]() {
                m_projectTree->refresh();
                markModified();
            });

    // G1: Topology signals
    connect(m_projectTree, &ProjectTreeWidget::addAreaRequested,
            this, &MainWindow::onAddAreaRequested);
    connect(m_projectTree, &ProjectTreeWidget::addLineRequested,
            this, &MainWindow::onAddLineRequested);
    connect(m_projectTree, &ProjectTreeWidget::deleteAreaRequested,
            this, &MainWindow::onDeleteAreaRequested);
    connect(m_projectTree, &ProjectTreeWidget::deleteLineRequested,
            this, &MainWindow::onDeleteLineRequested);
    connect(m_projectTree, &ProjectTreeWidget::deleteDeviceRequested,
            this, &MainWindow::onDeleteDeviceRequested);
    connect(m_projectTree, &ProjectTreeWidget::duplicateDeviceRequested,
            this, &MainWindow::onDuplicateDeviceRequested);

    // G1: Group address signals
    connect(m_projectTree, &ProjectTreeWidget::addMainGroupRequested,
            this, &MainWindow::onAddMainGroupRequested);
    connect(m_projectTree, &ProjectTreeWidget::addMiddleGroupRequested,
            this, &MainWindow::onAddMiddleGroupRequested);
    connect(m_projectTree, &ProjectTreeWidget::addGroupAddressRequested,
            this, &MainWindow::onAddGroupAddressRequested);
    connect(m_projectTree, &ProjectTreeWidget::deleteGroupAddressRequested,
            this, &MainWindow::onDeleteGroupAddressRequested);

    // Catalog import
    connect(m_projectTree, &ProjectTreeWidget::catalogImportRequested,
            this, &MainWindow::onImportCatalogFile);

    // G2: Building signals
    connect(m_projectTree, &ProjectTreeWidget::addBuildingRequested,
            this, &MainWindow::onAddBuildingRequested);
    connect(m_projectTree, &ProjectTreeWidget::addBuildingChildRequested,
            this, &MainWindow::onAddBuildingChildRequested);
    connect(m_projectTree, &ProjectTreeWidget::deleteBuildingPartRequested,
            this, &MainWindow::onDeleteBuildingPartRequested);

    // Inline renames via setData already call markModified via projectModified signal
    connect(m_projectTree, &ProjectTreeWidget::projectModified,
            this, &MainWindow::markModified);
}

void MainWindow::setupStatusBar()
{
    m_connectionStatusLabel = new QLabel(this);
    m_connectionStatusLabel->setTextFormat(Qt::RichText);
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

void MainWindow::markModified()
{
    m_modified = true;
    updateWindowTitle();
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

    auto area = std::make_unique<TopologyNode>(TopologyNode::Type::Area, 1, tr("Bereich 1"));
    auto line = std::make_unique<TopologyNode>(TopologyNode::Type::Line, 1, tr("Linie 1"));
    area->addChild(std::move(line));
    m_project->addArea(std::move(area));

    m_currentFilePath.clear();
    m_modified = false;
    m_undoStack->clear();

    m_projectTree->setProject(m_project.get());
    m_busMonitor->setProject(m_project.get());
    m_groupMonitor->setProject(m_project.get());
    m_propertiesPanel->setProject(m_project.get());
    m_deviceEditor->clearDevice();
    m_propertiesPanel->clearSelection();
    updateWindowTitle();
    statusBar()->showMessage(tr("Neues Projekt angelegt"));
}

void MainWindow::openProject()
{
    if (!maybeSave())
        return;

    const QString path = QFileDialog::getOpenFileName(
        this, tr("Projekt öffnen"), QString(),
        tr("KNX Projekte (*.knxproj);;Alle Dateien (*)"));
    if (path.isEmpty())
        return;

    statusBar()->showMessage(tr("Öffne %1…").arg(QFileInfo(path).fileName()));
    QApplication::setOverrideCursor(Qt::WaitCursor);

    struct LoadResult { std::unique_ptr<Project> project; QString error; };
    auto *res = new LoadResult;
    auto *thread = QThread::create([path, res]() {
        res->project = KnxprojSerializer::load(path, &res->error);
    });
    connect(thread, &QThread::finished, this, [this, path, res, thread]() {
        thread->deleteLater();
        QApplication::restoreOverrideCursor();
        auto loaded  = std::move(res->project);
        const QString err = res->error;
        delete res;
        if (!loaded) {
            statusBar()->showMessage(tr("Ladefehler: %1").arg(QFileInfo(path).fileName()));
            QMessageBox::critical(this, tr("Projekt konnte nicht geöffnet werden"),
                tr("Datei: %1\n\n%2").arg(path, err.isEmpty() ? tr("Unbekannter Fehler.") : err));
            return;
        }
        m_project = std::move(loaded);
        m_currentFilePath = path;
        m_modified = false;
        m_undoStack->clear();
        m_projectTree->setProject(m_project.get());
        m_busMonitor->setProject(m_project.get());
        m_groupMonitor->setProject(m_project.get());
        m_propertiesPanel->setProject(m_project.get());
        m_deviceEditor->clearDevice();
        m_propertiesPanel->clearSelection();
        addToRecentFiles(path);
        updateWindowTitle();
        statusBar()->showMessage(tr("Projekt geladen: %1").arg(path));
    });
    thread->start();
}

void MainWindow::saveProject()
{
    if (m_currentFilePath.isEmpty()) {
        saveProjectAs();
        return;
    }
    if (!KnxprojSerializer::save(*m_project, m_currentFilePath)) {
        QMessageBox::critical(this, tr("Fehler"),
            tr("Projekt konnte nicht gespeichert werden:\n%1").arg(m_currentFilePath));
        return;
    }
    m_modified = false;
    addToRecentFiles(m_currentFilePath);
    updateWindowTitle();
    statusBar()->showMessage(tr("Gespeichert: %1").arg(m_currentFilePath));
}

void MainWindow::saveProjectAs()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Projekt speichern unter"), QString(),
        tr("KNX Projekte (*.knxproj);;Alle Dateien (*)"));
    if (path.isEmpty())
        return;
    m_currentFilePath = path;
    saveProject();
}

void MainWindow::addGroupAddress()
{
    GroupAddressDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    GroupAddress ga(dlg.mainGroup(), dlg.middleGroup(), dlg.subGroup(),
                    dlg.name(), dlg.dpt());
    if (m_project->findGroupAddress(ga.toString())) {
        QMessageBox::warning(this, tr("Gruppenadresse existiert bereits"),
            tr("Die Adresse %1 ist im Projekt bereits vorhanden.").arg(ga.toString()));
        return;
    }
    m_undoStack->push(new AddGroupAddressCommand(m_project.get(), ga));
    m_projectTree->refresh();
    refreshGroupMonitor();
    markModified();
}

void MainWindow::onAddDeviceRequested(const QString &productId,
                                       const QString &productName,
                                       std::shared_ptr<KnxApplicationProgram> appProgram)
{
    if (!appProgram)
        return;

    if (m_project->areaCount() == 0)
        m_project->addArea(std::make_unique<TopologyNode>(TopologyNode::Type::Area, 1, tr("Bereich 1")));
    TopologyNode *area = m_project->areaAt(0);
    if (area->childCount() == 0)
        area->addChild(std::make_unique<TopologyNode>(TopologyNode::Type::Line, 1, tr("Linie 1")));
    TopologyNode *line = area->childAt(0);

    const int nextMember = line->deviceCount() + 1;
    const QString physAddr = QStringLiteral("%1.%2.%3").arg(area->id()).arg(line->id()).arg(nextMember);

    const QString instanceId = QStringLiteral("d%1").arg(nextMember);
    auto dev = std::make_unique<DeviceInstance>(instanceId, productId, appProgram->id);
    dev->setPhysicalAddress(physAddr);
    dev->setDescription(productName);
    dev->setAppProgram(appProgram);

    for (const KnxParameter &p : appProgram->parameters)
        dev->parameters()[p.id] = p.defaultValue;

    for (const KnxComObject &co : appProgram->comObjects) {
        ComObjectLink link;
        link.comObjectId = co.id;
        // Derive initial direction from ComObject flags:
        // Contains W → Send; R or U (without W) → Receive; default → Send
        if (co.flags.contains(QStringLiteral("W"))) {
            link.direction = ComObjectLink::Direction::Send;
        } else if (co.flags.contains(QStringLiteral("R")) || co.flags.contains(QStringLiteral("U"))) {
            link.direction = ComObjectLink::Direction::Receive;
        }
        dev->addLink(link);
    }

    line->addDevice(std::move(dev));
    m_projectTree->refresh();
    markModified();
    statusBar()->showMessage(tr("Gerät hinzugefügt: %1 (%2)").arg(productName, physAddr));
}

void MainWindow::onDeviceSelected(DeviceInstance *device)
{
    m_selectedDevice = device;
    if (!device) {
        m_deviceEditor->clearDevice();
        m_propertiesPanel->clearSelection();
        updateConnectionUi();
        return;
    }

    if (!device->appProgram())
        device->setAppProgram(m_catalog->sharedByProductRef(device->productRefId()));

    m_deviceEditor->setDevice(device, m_project.get());
    m_centerStack->setCurrentWidget(m_deviceEditor);
    m_propertiesPanel->showDevice(device);
    updateConnectionUi();
}

void MainWindow::onGroupAddressSelected(GroupAddress *ga)
{
    m_selectedDevice = nullptr;
    m_deviceEditor->clearDevice();
    m_propertiesPanel->showGroupAddress(ga);
    updateConnectionUi();
}

void MainWindow::onConnectClicked()
{
    if (m_interfaces->isConnected()) {
        onDisconnectClicked();
        return;
    }

    ConnectDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    std::unique_ptr<IKnxInterface> iface;
    QString statusMsg;

    if (dlg.connectionType() == ConnectDialog::ConnectionType::Usb) {
        iface     = ConnectInterfaceFactory::createUsb(dlg.usbTransport(), dlg.usbDevicePath());
        statusMsg = tr("Verbindungsaufbau zu %1…").arg(dlg.usbDevicePath());
    } else {
        iface     = ConnectInterfaceFactory::createKnxIp(dlg.selectedHost(), dlg.selectedPort());
        statusMsg = tr("Verbindungsaufbau zu %1…").arg(dlg.selectedHost().toString());
    }

    m_interfaces->setInterface(std::move(iface));
    if (m_interfaces->activeInterface())
        m_interfaces->activeInterface()->connectToInterface();
    statusBar()->showMessage(statusMsg);
}

void MainWindow::onDisconnectClicked()
{
    if (m_interfaces->activeInterface())
        m_interfaces->activeInterface()->disconnectFromInterface();
}

void MainWindow::onShowBusMonitor()
{
    m_centerStack->setCurrentWidget(m_busMonitor);
}

void MainWindow::onShowGroupMonitor()
{
    m_centerStack->setCurrentWidget(m_groupMonitor);
}

void MainWindow::refreshGroupMonitor()
{
    if (m_groupMonitor && m_project)
        m_groupMonitor->setProject(m_project.get());
}

void MainWindow::onImportCatalogFile()
{
    const QString src = QFileDialog::getOpenFileName(
        this, tr("Katalogdatei importieren"), QString(),
        tr("KNX Produktdateien (*.knxprod);;Alle Dateien (*)"));
    if (src.isEmpty())
        return;

    QDir().mkpath(m_writableCatalogPath);
    const QString dst = m_writableCatalogPath + QStringLiteral("/")
                        + QFileInfo(src).fileName();

    if (QFile::exists(dst)) {
        const auto btn = QMessageBox::question(this, tr("Datei bereits vorhanden"),
            tr("Die Datei \"%1\" ist bereits im Katalog vorhanden. Überschreiben?")
                .arg(QFileInfo(dst).fileName()),
            QMessageBox::Yes | QMessageBox::Cancel);
        if (btn != QMessageBox::Yes)
            return;
        QFile::remove(dst);
    }

    if (!QFile::copy(src, dst)) {
        QMessageBox::critical(this, tr("Fehler"),
            tr("Die Datei konnte nicht kopiert werden:\n%1").arg(src));
        return;
    }

    // Try to load JUST the imported file first so we can give a specific report.
    const int countBefore = m_catalog->count();
    const int added = m_catalog->importFile(dst);
    const QStringList errs = m_catalog->lastErrors();

    // Re-scan all paths so any other catalog files stay loaded.
    m_catalog->reload();
    m_projectTree->setCatalog(m_catalog.get());

    if (added <= 0) {
        const QString detail = errs.isEmpty()
            ? tr("Die Datei enthält keine ladbaren KNX-Produkte.")
            : errs.join(QStringLiteral("\n"));
        QMessageBox::warning(this, tr("Katalog-Import fehlgeschlagen"),
            tr("Aus der Datei \"%1\" konnten keine Produkte geladen werden.\n\n%2")
                .arg(QFileInfo(src).fileName(), detail));
        // Remove the imported (broken) file so the user can retry cleanly
        QFile::remove(dst);
        return;
    }

    const int totalNow = m_catalog->count();
    statusBar()->showMessage(
        tr("%1 Produkt(e) importiert (Gesamt: %2)").arg(added).arg(totalNow));
    Q_UNUSED(countBefore);
}

void MainWindow::onImportGaCsv()
{
    if (!m_project) return;

    const QString path = QFileDialog::getOpenFileName(
        this, tr("Gruppenadressen aus CSV importieren"), QString(),
        tr("CSV-Dateien (*.csv);;Alle Dateien (*)"));
    if (path.isEmpty()) return;

    GaCsvImportDialog dlg(path, this);
    if (dlg.exec() != QDialog::Accepted) return;

    const QList<GroupAddress> &imported = dlg.addresses();
    if (imported.isEmpty()) return;

    int added = 0;
    for (const GroupAddress &ga : imported) {
        if (!m_project->findGroupAddress(ga.toString())) {
            m_project->addGroupAddress(ga);
            ++added;
        }
    }

    if (added > 0) {
        m_projectTree->refresh();
        refreshGroupMonitor();
        markModified();
        statusBar()->showMessage(tr("%1 Gruppenadresse(n) importiert (%2 übersprungen)")
            .arg(added).arg(imported.size() - added));
    } else {
        statusBar()->showMessage(tr("Alle %1 Adressen bereits vorhanden – nichts importiert.")
            .arg(imported.size()));
    }
}

void MainWindow::onExportGaCsv()
{
    if (!m_project || m_project->groupAddresses().isEmpty()) {
        QMessageBox::information(this, tr("Export"),
            tr("Das Projekt enthält keine Gruppenadressen."));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this, tr("Gruppenadressen als CSV exportieren"), QString(),
        tr("CSV-Dateien (*.csv);;Alle Dateien (*)"));
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Fehler"),
            tr("Die Datei konnte nicht geschrieben werden:\n%1").arg(path));
        return;
    }

    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    // ETS-compatible header
    ts << "\"Name\";\"Adresse\";\"DPT\"\n";
    for (const GroupAddress &ga : m_project->groupAddresses()) {
        const auto esc = [](const QString &s) {
            return QStringLiteral("\"") + QString(s).replace(QLatin1Char('"'), QStringLiteral("\"\"")) + QLatin1Char('"');
        };
        ts << esc(ga.name()) << ';' << esc(ga.toString()) << ';' << esc(ga.dpt()) << '\n';
    }
    f.close();

    statusBar()->showMessage(tr("%1 Gruppenadresse(n) exportiert nach %2")
        .arg(m_project->groupAddresses().size()).arg(QFileInfo(path).fileName()));
}

void MainWindow::addToRecentFiles(const QString &path)
{
    if (path.isEmpty()) return;
    QSettings s;
    QStringList recent = s.value(QStringLiteral("recentFiles")).toStringList();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 8)
        recent.removeLast();
    s.setValue(QStringLiteral("recentFiles"), recent);
    updateRecentFilesMenu();
}

void MainWindow::updateRecentFilesMenu()
{
    if (!m_recentMenu) return;
    m_recentMenu->clear();
    const QStringList recent = QSettings().value(QStringLiteral("recentFiles")).toStringList();
    if (recent.isEmpty()) {
        m_recentMenu->addAction(tr("(Keine)"))->setEnabled(false);
        return;
    }
    for (const QString &path : recent) {
        const QString label = QFileInfo(path).fileName()
                              + QStringLiteral("  [") + QFileInfo(path).dir().dirName() + QLatin1Char(']');
        m_recentMenu->addAction(label, this, [this, path](){ onOpenRecentFile(path); });
    }
    m_recentMenu->addSeparator();
    m_recentMenu->addAction(tr("Liste leeren"), this, [this](){
        QSettings().remove(QStringLiteral("recentFiles"));
        updateRecentFilesMenu();
    });
}

void MainWindow::onOpenRecentFile(const QString &path)
{
    if (!QFile::exists(path)) {
        QMessageBox::warning(this, tr("Datei nicht gefunden"),
            tr("Die Datei wurde nicht gefunden:\n%1").arg(path));
        // Remove from recent list
        QSettings s;
        QStringList recent = s.value(QStringLiteral("recentFiles")).toStringList();
        recent.removeAll(path);
        s.setValue(QStringLiteral("recentFiles"), recent);
        updateRecentFilesMenu();
        return;
    }
    if (!maybeSave()) return;

    QString loadError;
    auto loaded = KnxprojSerializer::load(path, &loadError);
    if (!loaded) {
        QMessageBox::critical(this, tr("Projekt konnte nicht geöffnet werden"),
            tr("Datei: %1\n\n%2")
                .arg(path,
                     loadError.isEmpty() ? tr("Unbekannter Fehler.") : loadError));
        return;
    }

    m_project = std::move(loaded);
    m_currentFilePath = path;
    m_modified = false;
    m_undoStack->clear();

    m_projectTree->setProject(m_project.get());
    m_groupMonitor->setProject(m_project.get());
    m_propertiesPanel->setProject(m_project.get());
    m_deviceEditor->clearDevice();
    m_propertiesPanel->clearSelection();
    addToRecentFiles(path);
    updateWindowTitle();
    statusBar()->showMessage(tr("Projekt geladen: %1").arg(path));
}

void MainWindow::onLineScanClicked()
{
    if (!m_interfaces->isConnected()) {
        QMessageBox::warning(this, tr("Nicht verbunden"),
            tr("Bitte zuerst mit einem Bus-Interface verbinden."));
        return;
    }
    LineScanDialog dlg(m_interfaces.get(), this);
    dlg.exec();
}

void MainWindow::onProgramClicked()
{
    if (!m_interfaces->isConnected()) {
        QMessageBox::warning(this, tr("Nicht verbunden"),
            tr("Bitte zuerst mit einem Bus-Interface verbinden."));
        return;
    }

    // Collect all selected devices from the topology view
    QList<DeviceInstance *> selected = m_projectTree->selectedDevices();

    // If nothing multi-selected but a single device is active, use that
    if (selected.isEmpty() && m_selectedDevice)
        selected.append(m_selectedDevice);

    if (selected.isEmpty()) {
        QMessageBox::information(this, tr("Kein Gerät ausgewählt"),
            tr("Bitte wählen Sie im Projekt-Browser ein oder mehrere Geräte aus."));
        return;
    }

    if (selected.size() == 1) {
        DeviceInstance *dev = selected.first();
        if (!dev->appProgram())
            dev->setAppProgram(m_catalog->sharedByProductRef(dev->productRefId()));
        if (!dev->appProgram()) {
            QMessageBox::critical(this, tr("Anwendungsprogramm fehlt"),
                tr("Für das Gerät %1 wurde kein Anwendungsprogramm gefunden.").arg(dev->productRefId()));
            return;
        }
        auto *programmer = new DeviceProgrammer(
            m_interfaces->activeInterface(), dev, dev->appProgram(), this);
        auto *dlg = new ProgramDialog(programmer, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    } else {
        auto *dlg = new BatchProgramDialog(
            selected, m_interfaces->activeInterface(), m_catalog.get(), this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    }
}

void MainWindow::onInterfaceConnected()
{
    statusBar()->showMessage(tr("Verbunden"), 3000);
    updateConnectionUi();
}

void MainWindow::onInterfaceDisconnected()
{
    statusBar()->showMessage(tr("Getrennt"), 3000);
    updateConnectionUi();
}

void MainWindow::onInterfaceError(const QString &message)
{
    statusBar()->showMessage(tr("Interface-Fehler: %1").arg(message), 5000);
    QMessageBox::warning(this, tr("Interface-Fehler"), message);
}

void MainWindow::updateConnectionUi()
{
    const bool connected = m_interfaces && m_interfaces->isConnected();
    if (m_actConnect)    m_actConnect->setEnabled(!connected);
    if (m_actDisconnect) m_actDisconnect->setEnabled(connected);
    if (m_actLineScan)   m_actLineScan->setEnabled(connected);
    if (m_actProgram)    m_actProgram->setEnabled(connected && m_selectedDevice != nullptr);

    if (m_connectionStatusLabel) {
        const QString dot = connected
            ? QStringLiteral("<span style='color:#4CAF50; font-size:14px;'>&#9679;</span>")
            : QStringLiteral("<span style='color:#888888; font-size:14px;'>&#9679;</span>");
        const QString text = connected ? tr("Verbunden") : tr("Nicht verbunden");
        m_connectionStatusLabel->setText(dot + QStringLiteral(" ") + text);
    }
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
        return !m_modified;
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

// ─── G1: Topology management ──────────────────────────────────────────────────

void MainWindow::onAddAreaRequested()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Neuen Bereich anlegen"),
                                               tr("Name des Bereichs:"),
                                               QLineEdit::Normal, tr("Bereich"), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    const int nextId = m_project->areaCount() + 1;
    m_project->addArea(std::make_unique<TopologyNode>(TopologyNode::Type::Area, nextId, name.trimmed()));
    m_projectTree->refresh();
    markModified();
}

void MainWindow::onAddLineRequested(TopologyNode *area)
{
    if (!area) return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Neue Linie anlegen"),
                                               tr("Name der Linie:"),
                                               QLineEdit::Normal, tr("Linie"), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    const int nextId = area->childCount() + 1;
    area->addChild(std::make_unique<TopologyNode>(TopologyNode::Type::Line, nextId, name.trimmed()));
    m_projectTree->refresh();
    markModified();
}

void MainWindow::onDeleteAreaRequested(TopologyNode *area)
{
    if (!area) return;
    const auto btn = QMessageBox::question(this, tr("Bereich löschen"),
        tr("Bereich \"%1\" und alle enthaltenen Linien und Geräte wirklich löschen?").arg(area->name()),
        QMessageBox::Yes | QMessageBox::Cancel);
    if (btn != QMessageBox::Yes) return;

    for (int i = 0; i < m_project->areaCount(); ++i) {
        if (m_project->areaAt(i) == area) {
            if (m_selectedDevice) {
                m_selectedDevice = nullptr;
                m_deviceEditor->clearDevice();
                m_propertiesPanel->clearSelection();
            }
            m_undoStack->push(new DeleteAreaCommand(m_project.get(), i));
            m_projectTree->refresh();
            markModified();
            return;
        }
    }
}

void MainWindow::onDeleteLineRequested(TopologyNode *line)
{
    if (!line || !line->parent()) return;
    const auto btn = QMessageBox::question(this, tr("Linie löschen"),
        tr("Linie \"%1\" und alle enthaltenen Geräte wirklich löschen?").arg(line->name()),
        QMessageBox::Yes | QMessageBox::Cancel);
    if (btn != QMessageBox::Yes) return;

    TopologyNode *area = line->parent();
    const int idx = area->indexOfChild(line);
    if (idx >= 0) {
        if (m_selectedDevice) {
            m_selectedDevice = nullptr;
            m_deviceEditor->clearDevice();
            m_propertiesPanel->clearSelection();
        }
        m_undoStack->push(new DeleteLineCommand(area, idx));
        m_projectTree->refresh();
        markModified();
    }
}

void MainWindow::onDeleteDeviceRequested(DeviceInstance *dev)
{
    if (!dev) return;

    for (int a = 0; a < m_project->areaCount(); ++a) {
        TopologyNode *area = m_project->areaAt(a);
        for (int l = 0; l < area->childCount(); ++l) {
            TopologyNode *line = area->childAt(l);
            const int idx = line->indexOfDevice(dev);
            if (idx >= 0) {
                if (m_selectedDevice == dev) {
                    m_selectedDevice = nullptr;
                    m_deviceEditor->clearDevice();
                    m_propertiesPanel->clearSelection();
                }
                m_undoStack->push(new DeleteDeviceCommand(line, idx));
                m_projectTree->refresh();
                markModified();
                return;
            }
        }
    }
}

void MainWindow::onDuplicateDeviceRequested(DeviceInstance *src, TopologyNode *line)
{
    if (!src || !line) return;

    // Build a clone: same product refs + app program, copy all parameters + com-object links
    auto clone = std::make_unique<DeviceInstance>(
        QString(),               // new unique id assigned by DeviceInstance constructor
        src->productRefId(),
        src->appProgramRefId());
    clone->setAppProgram(src->appProgramShared());

    // Deep-copy parameters and links
    for (const auto &[id, val] : src->parameters())
        clone->parameters()[id] = val;
    for (const ComObjectLink &lnk : src->links())
        clone->addLink(lnk);

    // Increment physical address by 1 if it fits the range
    clone->setDescription(src->description().isEmpty()
                          ? tr("Kopie") : src->description() + tr(" (Kopie)"));
    {
        const QStringList parts = src->physicalAddress().split(QLatin1Char('.'));
        if (parts.size() == 3) {
            bool ok = false;
            int sub = parts[2].toInt(&ok);
            if (ok && sub < 255)
                clone->setPhysicalAddress(
                    QStringLiteral("%1.%2.%3").arg(parts[0]).arg(parts[1]).arg(sub + 1));
            else
                clone->setPhysicalAddress(src->physicalAddress());
        }
    }

    line->addDevice(std::move(clone));
    m_projectTree->refresh();
    markModified();
}

// ─── G1: Group address management ─────────────────────────────────────────────

void MainWindow::onAddMainGroupRequested()
{
    bool ok = false;
    // Determine next available main group number
    int nextMain = 0;
    for (const GroupAddress &ga : m_project->groupAddresses())
        nextMain = qMax(nextMain, ga.main() + 1);

    const QString label = QInputDialog::getText(this, tr("Neue Hauptgruppe"),
        tr("Bezeichnung der Hauptgruppe %1:").arg(nextMain),
        QLineEdit::Normal, tr("Hauptgruppe %1").arg(nextMain), &ok);
    if (!ok) return;

    // Add a placeholder GA at sub-address 0 to create the main group in the tree
    GroupAddress ga(nextMain, 0, 0, label.trimmed().isEmpty()
                    ? tr("Hauptgruppe %1").arg(nextMain) : label.trimmed(),
                    QString());
    m_project->addGroupAddress(ga);
    m_projectTree->refresh();
    markModified();
}

void MainWindow::onAddMiddleGroupRequested(int mainGroup)
{
    bool ok = false;
    int nextMid = 0;
    for (const GroupAddress &ga : m_project->groupAddresses())
        if (ga.main() == mainGroup) nextMid = qMax(nextMid, ga.middle() + 1);

    const QString label = QInputDialog::getText(this, tr("Neue Mittelgruppe"),
        tr("Bezeichnung der Mittelgruppe %1/%2:").arg(mainGroup).arg(nextMid),
        QLineEdit::Normal, tr("Mittelgruppe %1").arg(nextMid), &ok);
    if (!ok) return;

    GroupAddress ga(mainGroup, nextMid, 0, label.trimmed().isEmpty()
                    ? tr("Mittelgruppe %1/%2").arg(mainGroup).arg(nextMid) : label.trimmed(),
                    QString());
    m_project->addGroupAddress(ga);
    m_projectTree->refresh();
    markModified();
}

void MainWindow::onAddGroupAddressRequested(int mainGroup, int middleGroup)
{
    GroupAddressDialog dlg(this);
    // Pre-fill the main and middle group — GroupAddressDialog needs to support this
    // For now use the existing dialog without pre-fill (full manual entry)
    if (dlg.exec() != QDialog::Accepted)
        return;

    GroupAddress ga(dlg.mainGroup(), dlg.middleGroup(), dlg.subGroup(),
                    dlg.name(), dlg.dpt());
    if (m_project->findGroupAddress(ga.toString())) {
        QMessageBox::warning(this, tr("Gruppenadresse existiert bereits"),
            tr("Die Adresse %1 ist im Projekt bereits vorhanden.").arg(ga.toString()));
        return;
    }
    m_project->addGroupAddress(ga);
    m_projectTree->refresh();
    markModified();
}

void MainWindow::onDeleteGroupAddressRequested(GroupAddress *ga)
{
    if (!ga) return;
    const auto btn = QMessageBox::question(this, tr("Gruppenadresse löschen"),
        tr("Gruppenadresse \"%1\" (%2) wirklich löschen?").arg(ga->name(), ga->toString()),
        QMessageBox::Yes | QMessageBox::Cancel);
    if (btn != QMessageBox::Yes) return;

    const int idx = m_project->groupAddresses().indexOf(*ga);
    if (idx >= 0)
        m_undoStack->push(new DeleteGroupAddressCommand(m_project.get(), idx));
    m_projectTree->refresh();
    refreshGroupMonitor();
    markModified();
}

// ─── G2: Building management ──────────────────────────────────────────────────

void MainWindow::onAddBuildingRequested()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Neues Gebäude anlegen"),
                                               tr("Name des Gebäudes:"),
                                               QLineEdit::Normal, tr("Gebäude"), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    m_project->addBuilding(std::make_unique<BuildingPart>(BuildingPart::Type::Building, name.trimmed()));
    m_projectTree->refresh();
    markModified();
}

void MainWindow::onAddBuildingChildRequested(BuildingPart *parent, BuildingPart::Type childType)
{
    if (!parent) return;

    QString title, defaultName;
    switch (childType) {
    case BuildingPart::Type::Floor:
        title       = tr("Neue Etage anlegen");
        defaultName = tr("Etage");
        break;
    case BuildingPart::Type::Room:
        title       = tr("Neuen Raum anlegen");
        defaultName = tr("Raum");
        break;
    default:
        title       = tr("Neues Element anlegen");
        defaultName = tr("Element");
        break;
    }

    bool ok = false;
    const QString name = QInputDialog::getText(this, title, tr("Name:"),
                                               QLineEdit::Normal, defaultName, &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    parent->addChild(std::make_unique<BuildingPart>(childType, name.trimmed()));
    m_projectTree->refresh();
    markModified();
}

void MainWindow::onDeleteBuildingPartRequested(BuildingPart *bp)
{
    if (!bp) return;
    const auto btn = QMessageBox::question(this, tr("Element löschen"),
        tr("\"%1\" und alle enthaltenen Elemente wirklich löschen?").arg(bp->name()),
        QMessageBox::Yes | QMessageBox::Cancel);
    if (btn != QMessageBox::Yes) return;

    BuildingPart *parent = bp->parent();
    if (parent) {
        const int idx = bp->indexInParent();
        if (idx >= 0)
            parent->removeChildAt(idx);
    } else {
        // Top-level building
        for (int i = 0; i < m_project->buildingCount(); ++i) {
            if (m_project->buildingAt(i) == bp) {
                m_project->removeBuildingAt(i);
                break;
            }
        }
    }
    m_projectTree->refresh();
    markModified();
}
