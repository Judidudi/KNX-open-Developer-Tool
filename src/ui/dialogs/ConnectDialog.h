#pragma once

#include <QDialog>
#include <QHostAddress>

class QListWidget;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QLabel;
class KnxIpDiscovery;
struct KnxIpDevice;

// Dialog for picking the KNX bus interface to connect to.
// Supports KNXnet/IP discovery (SEARCH_REQUEST) and manual IP entry.
// The chosen address is retrievable via selectedHost()/selectedPort() after
// the dialog is accepted.
class ConnectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConnectDialog(QWidget *parent = nullptr);

    QHostAddress selectedHost() const { return m_host; }
    quint16      selectedPort() const { return m_port; }

private slots:
    void startDiscovery();
    void onDeviceFound(KnxIpDevice dev);
    void onSearchFinished();
    void onSelectionChanged();
    void onAccept();

private:
    QListWidget    *m_discoveryList = nullptr;
    QPushButton    *m_searchButton  = nullptr;
    QLabel         *m_statusLabel   = nullptr;
    QLineEdit      *m_manualIp      = nullptr;
    QSpinBox       *m_manualPort    = nullptr;
    KnxIpDiscovery *m_discovery     = nullptr;

    QHostAddress    m_host;
    quint16         m_port = 3671;
};
