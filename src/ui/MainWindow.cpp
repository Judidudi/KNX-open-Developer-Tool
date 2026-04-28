#include "MainWindow.h"

#include "ProjectTreeWidget.h"
#include "DeviceEditorWidget.h"
#include "BusMonitorWidget.h"
#include "ProgramDialog.h"
#include "PropertiesPanel.h"
#include "dialogs/NewProjectDialog.h"
#include "dialogs/ConnectDialog.h"
#include "dialogs/GroupAddressDialog.h"

#include "Project.h"
#include "TopologyNode.h"
#include "DeviceInstance.h"
#include "DeviceCatalog.h"
#include "Manifest.h"
#include "KnxprojSerializer.h"
#include "GroupAddress.h"

#include "InterfaceManager.h"
#include "IKnxInterface.h"
#include "DeviceProgrammer.h"
#include "ConnectInterfaceFactory.h"

#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSplitter>
#include <QStackedWidget>
#include <QLabel>
#include <QAction>
#include <QDockWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_project    = std::make_unique<Project>();
    m_catalog    = std::make_unique<DeviceCatalog>();
    m_interfaces = std::make_unique<InterfaceManager>();

    setupCentralWidget();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();

    loadCatalog();

    m_projectTree->setProject(m_project.get());
    m_projectTree->setCatalog(m_catalog.get());
    m_busMonitor->setInterfaceManager(m_interfaces.get());

    connect(m_interfaces.get(), &InterfaceManager::connected,
            this, &MainWindow::onInterfaceConnected);
    connect(m_interfaces.get(), &InterfaceManager::disconnected,
            this, &MainWindow::onInterfaceDisconnected);
    connect(m_interfaces.get(), &InterfaceManager::errorOccurred,
            this, &MainWindow::onInterfaceError);

    updateConnectionUi();
    updateWindowTitle();
    resize(1280, 800);
}

MainWindow::~MainWindow() = default;

void MainWindow::loadCatalog()
{
    QString envPath = qEnvironmentVariable("KNXODT_CATALOG_PATH");
    if (!envPath.isEmpty())
        m_catalog->addSearchPath(envPath);

    const QDir appDir(QCoreApplication::applicationDirPath());
    m_catalog->addSearchPath(appDir.absoluteFilePath(QStringLiteral("catalog/devices")));
    m_catalog->addSearchPath(appDir.absoluteFilePath(QStringLiteral("../catalog/devices")));
    m_catalog->addSearchPath(appDir.absoluteFilePath(QStringLiteral("../../catalog/devices")));

    const QString userPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                             + QStringLiteral("/catalog/devices");
    QDir().mkpath(userPath);
    m_catalog->addSearchPath(userPath);

    m_catalog->reload();
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
    fileMenu->addAction(tr("&Beenden"), qApp, &QApplication::quit, QKeySequence::Quit);

    QMenu *projectMenu = menuBar()->addMenu(tr("&Projekt"));
    m_actAddGroupAddr = projectMenu->addAction(tr("Gruppenadresse hinzufügen…"),
                                               this, &MainWindow::addGroupAddress);

    QMenu *busMenu = menuBar()->addMenu(tr("&Bus"));
    m_actConnect    = busMenu->addAction(tr("&Verbinden…"),     this, &MainWindow::onConnectClicked);
    m_actDisconnect = busMenu->addAction(tr("&Trennen"),         this, &MainWindow::onDisconnectClicked);
    m_actBusMonitor = busMenu->addAction(tr("&Busmonitor"),      this, &MainWindow::onShowBusMonitor);
    busMenu->addSeparator();
    m_actProgram    = busMenu->addAction(tr("Gerät &programmieren…"), this, &MainWindow::onProgramClicked);

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

    m_centerStack = new QStackedWidget(this);
    m_centerStack->addWidget(m_deviceEditor);
    m_centerStack->addWidget(m_busMonitor);
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

    m_projectTree->setProject(m_project.get());
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

    auto loaded = KnxprojSerializer::load(path);
    if (!loaded) {
        QMessageBox::critical(this, tr("Fehler"),
            tr("Projekt konnte nicht geöffnet werden:\n%1").arg(path));
        return;
    }

    m_project = std::move(loaded);
    m_currentFilePath = path;
    m_modified = false;

    m_projectTree->setProject(m_project.get());
    m_deviceEditor->clearDevice();
    m_propertiesPanel->clearSelection();
    updateWindowTitle();
    statusBar()->showMessage(tr("Projekt geladen: %1").arg(path));
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
    m_project->addGroupAddress(ga);
    m_projectTree->refresh();
    markModified();
}

void MainWindow::onAddDeviceRequested(std::shared_ptr<Manifest> manifest)
{
    if (!manifest)
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
    auto dev = std::make_unique<DeviceInstance>(instanceId, manifest->id, manifest->version);
    dev->setPhysicalAddress(physAddr);
    dev->setManifest(manifest);

    for (const ManifestParameter &p : manifest->parameters)
        dev->parameters()[p.id] = p.defaultValue;

    for (const ManifestComObject &co : manifest->comObjects) {
        ComObjectLink link;
        link.comObjectId = co.id;
        dev->addLink(link);
    }

    line->addDevice(std::move(dev));
    m_projectTree->refresh();
    markModified();
    statusBar()->showMessage(tr("Gerät hinzugefügt: %1 (%2)").arg(manifest->name.get(), physAddr));
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

    if (!device->manifest())
        device->setManifest(m_catalog->sharedById(device->productRefId()));

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

void MainWindow::onProgramClicked()
{
    if (!m_selectedDevice) {
        QMessageBox::information(this, tr("Kein Gerät ausgewählt"),
            tr("Bitte wählen Sie im Projekt-Browser ein Gerät aus."));
        return;
    }
    if (!m_interfaces->isConnected()) {
        QMessageBox::warning(this, tr("Nicht verbunden"),
            tr("Bitte zuerst mit einem Bus-Interface verbinden."));
        return;
    }
    if (!m_selectedDevice->manifest())
        m_selectedDevice->setManifest(m_catalog->sharedById(m_selectedDevice->productRefId()));
    if (!m_selectedDevice->manifest()) {
        QMessageBox::critical(this, tr("Manifest fehlt"),
            tr("Für das Gerät %1 wurde kein Manifest gefunden.").arg(m_selectedDevice->productRefId()));
        return;
    }

    auto *programmer = new DeviceProgrammer(
        m_interfaces->activeInterface(),
        m_selectedDevice,
        m_selectedDevice->manifest(),
        this);

    auto *dlg = new ProgramDialog(programmer, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
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
