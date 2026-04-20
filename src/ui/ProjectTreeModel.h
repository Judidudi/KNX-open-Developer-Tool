#pragma once

#include <QAbstractItemModel>
#include <memory>

class Project;
class TopologyNode;
class DeviceInstance;
class GroupAddress;

// Tree model exposing the project's topology and group address hierarchy
// for QTreeView. Root has two virtual sections:
//   Topology        ─ Area ─ Line ─ Device
//   Group Addresses ─ MainGroup ─ MiddleGroup ─ GroupAddress
class ProjectTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum ItemKind {
        TopologyRoot,
        GaRoot,
        Area,
        Line,
        Device,
        MainGroup,
        MiddleGroup,
        GroupAddress,
    };

    enum Roles {
        KindRole = Qt::UserRole + 1,
        DevicePtrRole,
        GroupAddressPtrRole,
    };

    explicit ProjectTreeModel(QObject *parent = nullptr);
    ~ProjectTreeModel() override;

    void     setProject(Project *project);
    Project *project() const { return m_project; }

    void rebuild();

    DeviceInstance     *deviceAt(const QModelIndex &index) const;
    ::GroupAddress     *groupAddressAt(const QModelIndex &index) const;
    ItemKind            kindAt(const QModelIndex &index) const;

    // QAbstractItemModel
    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int         rowCount(const QModelIndex &parent = {}) const override;
    int         columnCount(const QModelIndex &parent = {}) const override;
    QVariant    data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant    headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    struct Node;
    Node     *nodeFromIndex(const QModelIndex &index) const;

    Project                *m_project = nullptr;
    std::unique_ptr<Node>   m_root;
};
