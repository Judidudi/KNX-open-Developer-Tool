#pragma once

#include <QWidget>

class Project;
class ProjectTreeModel;
class DeviceInstance;
class GroupAddress;

class QTreeView;

class ProjectTreeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ProjectTreeWidget(QWidget *parent = nullptr);

    void setProject(Project *project);
    void refresh();

    ProjectTreeModel *model() const { return m_model; }

signals:
    void deviceSelected(DeviceInstance *device);
    void groupAddressSelected(GroupAddress *ga);
    void selectionCleared();

private slots:
    void onSelectionChanged();

private:
    QTreeView        *m_view  = nullptr;
    ProjectTreeModel *m_model = nullptr;
};
