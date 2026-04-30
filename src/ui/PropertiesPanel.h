#pragma once

#include <QDockWidget>

class DeviceInstance;
class GroupAddress;
class InterfaceManager;
class Project;

class QLabel;
class QLineEdit;
class QStackedWidget;
class QPushButton;
class QTimer;

class PropertiesPanel : public QDockWidget
{
    Q_OBJECT

public:
    explicit PropertiesPanel(QWidget *parent = nullptr);

    void setProject(Project *project);
    void setInterfaceManager(InterfaceManager *mgr);

    void showDevice(DeviceInstance *device);
    void showGroupAddress(GroupAddress *ga);
    void clearSelection();

signals:
    void deviceModified();
    void groupAddressModified();

private slots:
    void onDevDescEdited();
    void onPhysAddrEdited();
    void onGaNameEdited();
    void onGaDptEdited();
    void onPingClicked();
    void onPingTimeout();

private:
    void checkPhysAddrConflict(const QString &pa);

    Project          *m_project = nullptr;
    InterfaceManager *m_iface   = nullptr;

    QStackedWidget *m_stack = nullptr;

    // Page 1: Device
    QWidget    *m_devicePage      = nullptr;
    QLabel     *m_devTypeLabel    = nullptr;
    QLineEdit  *m_devNameEdit     = nullptr;
    QLineEdit  *m_devPhysEdit     = nullptr;
    QLabel     *m_devConflictLbl  = nullptr;
    QPushButton *m_pingBtn        = nullptr;
    QLabel     *m_pingResultLbl   = nullptr;
    QTimer     *m_pingTimer       = nullptr;

    // Page 2: GroupAddress
    QWidget   *m_gaPage      = nullptr;
    QLabel    *m_gaAddrLabel = nullptr;
    QLineEdit *m_gaNameEdit  = nullptr;
    QLineEdit *m_gaDptEdit   = nullptr;

    DeviceInstance *m_currentDevice = nullptr;
    GroupAddress   *m_currentGa     = nullptr;
    bool            m_updating      = false;
};
