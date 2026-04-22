#pragma once

#include <QDockWidget>

class DeviceInstance;
class GroupAddress;

class QLabel;
class QLineEdit;
class QStackedWidget;

class PropertiesPanel : public QDockWidget
{
    Q_OBJECT

public:
    explicit PropertiesPanel(QWidget *parent = nullptr);

    void showDevice(DeviceInstance *device);
    void showGroupAddress(GroupAddress *ga);
    void clearSelection();

signals:
    void deviceModified();
    void groupAddressModified();

private slots:
    void onPhysAddrEdited();
    void onGaNameEdited();
    void onGaDptEdited();

private:
    QStackedWidget *m_stack = nullptr;

    // Page 1: Device
    QWidget   *m_devicePage     = nullptr;
    QLabel    *m_devTypeLabel   = nullptr;
    QLineEdit *m_devPhysEdit    = nullptr;

    // Page 2: GroupAddress
    QWidget   *m_gaPage     = nullptr;
    QLabel    *m_gaAddrLabel = nullptr;
    QLineEdit *m_gaNameEdit  = nullptr;
    QLineEdit *m_gaDptEdit   = nullptr;

    DeviceInstance *m_currentDevice = nullptr;
    GroupAddress   *m_currentGa     = nullptr;
    bool            m_updating      = false;
};
