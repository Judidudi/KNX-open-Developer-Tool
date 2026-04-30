#pragma once

#include <QAbstractItemModel>
#include <memory>

class Project;
class TopologyNode;
class DeviceInstance;
class GroupAddress;
class BuildingPart;

// Tree model exposing the project's topology, group address hierarchy, and
// building structure for QTreeView. Root has three virtual sections:
//   Topology        ─ Area ─ Line ─ Device
//   Group Addresses ─ MainGroup ─ MiddleGroup ─ GroupAddress
//   Buildings       ─ Building ─ Floor ─ Room  (BuildingPart tree)
class ProjectTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum ItemKind {
        TopologyRoot,
        GaRoot,
        BuildingRoot,
        Area,
        Line,
        Device,
        MainGroup,
        MiddleGroup,
        GroupAddress,
        BuildingNode,   // Building, Floor, Room, etc.
    };

    enum Roles {
        KindRole = Qt::UserRole + 1,
        DevicePtrRole,
        GroupAddressPtrRole,
        BuildingPartPtrRole,
        TopologyNodePtrRole,
    };

    explicit ProjectTreeModel(QObject *parent = nullptr);
    ~ProjectTreeModel() override;

    void     setProject(Project *project);
    Project *project() const { return m_project; }

    void rebuild();

    DeviceInstance     *deviceAt(const QModelIndex &index) const;
    ::GroupAddress     *groupAddressAt(const QModelIndex &index) const;
    TopologyNode       *topologyNodeAt(const QModelIndex &index) const;
    BuildingPart       *buildingPartAt(const QModelIndex &index) const;
    ItemKind            kindAt(const QModelIndex &index) const;

    // QAbstractItemModel
    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int         rowCount(const QModelIndex &parent = {}) const override;
    int         columnCount(const QModelIndex &parent = {}) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant    data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool        setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QVariant    headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    struct Node;
    Node     *nodeFromIndex(const QModelIndex &index) const;

    Project                *m_project = nullptr;
    std::unique_ptr<Node>   m_root;
};
