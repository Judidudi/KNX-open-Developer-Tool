#pragma once

#include "UsbKnxInterface.h"
#include <QDialog>
#include <QHostAddress>

class QListWidget;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QLabel;
class QTabWidget;
class QComboBox;
class KnxIpDiscovery;
struct KnxIpDevice;

// Tabbed dialog for picking the KNX bus interface.
//  Tab 0 – KNXnet/IP: discovery (SEARCH_REQUEST) + manual IP entry
//  Tab 1 – USB:       serial port (CDC-ACM) or HID (/dev/hidrawN) selection
//
// After accept(), read connectionType() to decide which factory method to use.
class ConnectDialog : public QDialog
{
    Q_OBJECT

public:
    enum class ConnectionType { KnxIp, Usb };

    explicit ConnectDialog(QWidget *parent = nullptr);

    ConnectionType connectionType() const;

    // KNXnet/IP results (valid when connectionType() == KnxIp)
    QHostAddress selectedHost() const { return m_host; }
    quint16      selectedPort() const { return m_port; }

    // USB results (valid when connectionType() == Usb)
    UsbKnxInterface::Transport usbTransport() const;
    QString                    usbDevicePath() const;

private slots:
    void startDiscovery();
    void onDeviceFound(KnxIpDevice dev);
    void onSearchFinished();
    void onKnxIpSelectionChanged();
    void refreshUsbDevices();
    void onAccept();

private:
    QTabWidget     *m_tabs          = nullptr;

    // KNXnet/IP tab
    QListWidget    *m_discoveryList = nullptr;
    QPushButton    *m_searchButton  = nullptr;
    QLabel         *m_statusLabel   = nullptr;
    QLineEdit      *m_manualIp      = nullptr;
    QSpinBox       *m_manualPort    = nullptr;
    KnxIpDiscovery *m_discovery     = nullptr;

    // USB tab
    QComboBox      *m_usbTransport  = nullptr;
    QComboBox      *m_usbDevice     = nullptr;
    QPushButton    *m_usbRefresh    = nullptr;
    QLabel         *m_usbHint       = nullptr;

    QHostAddress    m_host;
    quint16         m_port = 3671;
};
