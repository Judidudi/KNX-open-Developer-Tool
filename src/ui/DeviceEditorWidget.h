#pragma once

#include <QWidget>
#include <QHash>
#include <QString>

class Project;
class DeviceInstance;
class KnxApplicationProgram;
class InterfaceManager;

class QTabWidget;
class QFormLayout;
class QLabel;
class QPushButton;
class QTableWidget;

class DeviceEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceEditorWidget(QWidget *parent = nullptr);

    void setDevice(DeviceInstance *device, Project *project);
    void clearDevice();
    void setInterfaceManager(InterfaceManager *mgr);

signals:
    void deviceModified();

private slots:
    void onParameterChanged();
    void onComObjectLinkChanged(int row);
    void onReadParametersClicked();
    void onCemiReceived(const QByteArray &cemi);

private:
    void buildParameterTab();
    void rebuildParameterTab();
    void buildComObjectTab();
    void clearTabs();
    void applyReadbackValues(const QByteArray &memory, uint16_t baseAddr);

    bool isParameterVisible(const struct KnxParameter &p) const;

    Project          *m_project = nullptr;
    DeviceInstance   *m_device  = nullptr;
    InterfaceManager *m_iface   = nullptr;

    QLabel      *m_title        = nullptr;
    QTabWidget  *m_tabs         = nullptr;
    QWidget     *m_paramTab     = nullptr;
    QWidget     *m_comObjTab    = nullptr;
    QFormLayout *m_paramLayout  = nullptr;
    QTableWidget *m_comObjTable = nullptr;
    QPushButton  *m_readBtn     = nullptr;

    // Maps parameter id → widget for value read-back
    QHash<QString, QWidget *> m_paramWidgets;

    bool m_updating = false;
    // Accumulates memory response bytes across potentially multiple frames
    uint16_t   m_readBaseAddr = 0;
    QByteArray m_readBuffer;
    int        m_readExpected = 0;
};
