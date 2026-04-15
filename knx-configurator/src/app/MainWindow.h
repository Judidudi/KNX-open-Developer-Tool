#pragma once
/**
 * @file MainWindow.h
 * @brief Main application window
 *
 * Layout:
 *   Toolbar: [Transport ▼] [Verbinden] [Trennen]
 *   Left:    DeviceListWidget
 *   Right:   DeviceEditor
 *   Status bar
 */

#include <QMainWindow>
#include <QComboBox>
#include <QPushButton>
#include <QSplitter>
#include "DeviceListWidget.h"
#include "DeviceEditor.h"
#include "KnxConfigProtocol.h"
#include "KnxTransport.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onTransportChanged(int index);
    void onDiscoverRequested();
    void onDeviceSelected(const Knx::DeviceInfo &info);
    void onDiscoverResult(const Knx::DeviceInfo &info);
    void onObjectCountResult(uint8_t count);
    void onObjectResult(const Knx::GroupObject &obj);
    void onProtocolError(const QString &msg);
    void onTransportError(const QString &msg);
    void onStatusMessage(const QString &msg);

private:
    void setupToolbar();
    void setupCentralWidget();
    void connectProtocolSignals();
    KnxTransport *createTransport();

    // Toolbar
    QComboBox   *m_transportCombo = nullptr;
    QPushButton *m_btnConnect     = nullptr;
    QPushButton *m_btnDisconnect  = nullptr;

    // Central
    DeviceListWidget *m_deviceList   = nullptr;
    DeviceEditor     *m_deviceEditor = nullptr;

    // Protocol stack
    KnxTransport       *m_transport = nullptr;
    KnxConfigProtocol  *m_protocol  = nullptr;
};
