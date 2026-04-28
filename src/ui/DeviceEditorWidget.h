#pragma once

#include <QWidget>
#include <QHash>
#include <QString>

class Project;
class DeviceInstance;
class KnxApplicationProgram;

class QTabWidget;
class QFormLayout;
class QLabel;
class QTableWidget;
class QWidget;

class DeviceEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceEditorWidget(QWidget *parent = nullptr);

    void setDevice(DeviceInstance *device, Project *project);
    void clearDevice();

signals:
    void deviceModified();

private slots:
    void onParameterChanged();
    void onComObjectLinkChanged(int row);

private:
    void buildParameterTab();
    void buildComObjectTab();
    void clearTabs();

    Project        *m_project = nullptr;
    DeviceInstance *m_device  = nullptr;

    QLabel      *m_title        = nullptr;
    QTabWidget  *m_tabs         = nullptr;
    QWidget     *m_paramTab     = nullptr;
    QWidget     *m_comObjTab    = nullptr;
    QFormLayout *m_paramLayout  = nullptr;
    QTableWidget *m_comObjTable = nullptr;

    // Maps parameter id → widget for value read-back
    QHash<QString, QWidget *> m_paramWidgets;

    // Guard to prevent feedback loops while programmatically updating widgets
    bool m_updating = false;
};
