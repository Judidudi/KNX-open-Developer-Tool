#include "ProjectTreeModel.h"

#include "Project.h"
#include "TopologyNode.h"
#include "DeviceInstance.h"
#include "GroupAddress.h"
#include "BuildingPart.h"

#include <QIcon>
#include <QMap>
#include <vector>

// Internal tree node. Built once in rebuild() and owned by the model.
struct ProjectTreeModel::Node {
    ItemKind              kind;
    QString               label;
    QString               details;
    void                 *rawPtr = nullptr;  // TopologyNode*, DeviceInstance*, GroupAddress*, BuildingPart*
    Node                 *parent = nullptr;
    int                   row    = 0;
    std::vector<std::unique_ptr<Node>> children;

    Node *addChild(std::unique_ptr<Node> c)
    {
        c->parent = this;
        c->row    = children.size();
        Node *ptr = c.get();
        children.push_back(std::move(c));
        return ptr;
    }
};

ProjectTreeModel::ProjectTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_root(std::make_unique<Node>())
{}

ProjectTreeModel::~ProjectTreeModel() = default;

void ProjectTreeModel::setProject(Project *project)
{
    beginResetModel();
    m_project = project;
    rebuild();
    endResetModel();
}

void ProjectTreeModel::rebuild()
{
    m_root = std::make_unique<Node>();
    if (!m_project)
        return;

    // ── Topology root node ──────────────────────────────────────────────────
    auto topNode = std::make_unique<Node>();
    topNode->kind  = TopologyRoot;
    topNode->label = tr("Topologie");
    Node *topology = m_root->addChild(std::move(topNode));

    for (int a = 0; a < m_project->areaCount(); ++a) {
        TopologyNode *area = m_project->areaAt(a);
        auto areaNode = std::make_unique<Node>();
        areaNode->kind    = Area;
        areaNode->label   = tr("%1 %2").arg(QString::number(area->id()), area->name());
        areaNode->details = tr("Bereich");
        areaNode->rawPtr  = area;
        Node *an = topology->addChild(std::move(areaNode));

        for (int l = 0; l < area->childCount(); ++l) {
            TopologyNode *line = area->childAt(l);
            auto lineNode = std::make_unique<Node>();
            lineNode->kind    = Line;
            lineNode->label   = tr("%1.%2 %3").arg(area->id()).arg(line->id()).arg(line->name());
            lineNode->details = tr("Linie");
            lineNode->rawPtr  = line;
            Node *ln = an->addChild(std::move(lineNode));

            for (int d = 0; d < line->deviceCount(); ++d) {
                DeviceInstance *dev = line->deviceAt(d);
                auto devNode = std::make_unique<Node>();
                devNode->kind = Device;
                const QString displayName = dev->description().isEmpty()
                                                ? dev->productRefId()
                                                : dev->description();
                devNode->label   = dev->physicalAddress().isEmpty()
                                       ? tr("(neu) %1").arg(displayName)
                                       : tr("%1 – %2").arg(dev->physicalAddress(), displayName);
                devNode->details = dev->productRefId();
                devNode->rawPtr  = dev;
                ln->addChild(std::move(devNode));
            }
        }
    }

    // ── Group addresses root ────────────────────────────────────────────────
    auto gaRoot = std::make_unique<Node>();
    gaRoot->kind  = GaRoot;
    gaRoot->label = tr("Gruppenadressen");
    Node *ga = m_root->addChild(std::move(gaRoot));

    QMap<int, QMap<int, QList<::GroupAddress *>>> grouped;
    for (::GroupAddress &g : m_project->groupAddresses())
        grouped[g.main()][g.middle()].append(&g);

    for (auto mainIt = grouped.constBegin(); mainIt != grouped.constEnd(); ++mainIt) {
        auto mgNode = std::make_unique<Node>();
        mgNode->kind  = MainGroup;
        mgNode->label = tr("%1 Hauptgruppe").arg(mainIt.key());
        Node *mg = ga->addChild(std::move(mgNode));

        for (auto midIt = mainIt.value().constBegin(); midIt != mainIt.value().constEnd(); ++midIt) {
            auto midNode = std::make_unique<Node>();
            midNode->kind  = MiddleGroup;
            midNode->label = tr("%1/%2 Mittelgruppe").arg(mainIt.key()).arg(midIt.key());
            Node *mid = mg->addChild(std::move(midNode));

            for (::GroupAddress *gaPtr : midIt.value()) {
                auto gaNode = std::make_unique<Node>();
                gaNode->kind    = GroupAddress;
                gaNode->label   = tr("%1 – %2").arg(gaPtr->toString(), gaPtr->name());
                gaNode->details = gaPtr->dpt();
                gaNode->rawPtr  = gaPtr;
                mid->addChild(std::move(gaNode));
            }
        }
    }

    // ── Buildings root ──────────────────────────────────────────────────────
    auto bldRoot = std::make_unique<Node>();
    bldRoot->kind  = BuildingRoot;
    bldRoot->label = tr("Gebäude");
    Node *buildings = m_root->addChild(std::move(bldRoot));

    std::function<void(Node *, BuildingPart *)> addBpNode = [&](Node *parent, BuildingPart *bp) {
        auto bpNode = std::make_unique<Node>();
        bpNode->kind    = BuildingNode;
        bpNode->label   = bp->name();
        bpNode->details = BuildingPart::typeToString(bp->type());
        bpNode->rawPtr  = bp;
        Node *bpn = parent->addChild(std::move(bpNode));
        for (int c = 0; c < bp->childCount(); ++c)
            addBpNode(bpn, bp->childAt(c));
    };

    for (int i = 0; i < m_project->buildingCount(); ++i)
        addBpNode(buildings, m_project->buildingAt(i));
}

ProjectTreeModel::Node *ProjectTreeModel::nodeFromIndex(const QModelIndex &index) const
{
    if (!index.isValid())
        return m_root.get();
    return static_cast<Node *>(index.internalPointer());
}

DeviceInstance *ProjectTreeModel::deviceAt(const QModelIndex &index) const
{
    Node *n = nodeFromIndex(index);
    return (n && n->kind == Device) ? static_cast<DeviceInstance *>(n->rawPtr) : nullptr;
}

::GroupAddress *ProjectTreeModel::groupAddressAt(const QModelIndex &index) const
{
    Node *n = nodeFromIndex(index);
    return (n && n->kind == GroupAddress) ? static_cast<::GroupAddress *>(n->rawPtr) : nullptr;
}

TopologyNode *ProjectTreeModel::topologyNodeAt(const QModelIndex &index) const
{
    Node *n = nodeFromIndex(index);
    if (!n) return nullptr;
    if (n->kind == Area || n->kind == Line)
        return static_cast<TopologyNode *>(n->rawPtr);
    return nullptr;
}

BuildingPart *ProjectTreeModel::buildingPartAt(const QModelIndex &index) const
{
    Node *n = nodeFromIndex(index);
    return (n && n->kind == BuildingNode) ? static_cast<BuildingPart *>(n->rawPtr) : nullptr;
}

ProjectTreeModel::ItemKind ProjectTreeModel::kindAt(const QModelIndex &index) const
{
    Node *n = nodeFromIndex(index);
    return n ? n->kind : TopologyRoot;
}

QModelIndex ProjectTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return {};
    Node *parentNode = nodeFromIndex(parent);
    if (row >= static_cast<int>(parentNode->children.size()))
        return {};
    return createIndex(row, column, parentNode->children[row].get());
}

QModelIndex ProjectTreeModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return {};
    Node *node = static_cast<Node *>(child.internalPointer());
    if (!node || !node->parent || node->parent == m_root.get())
        return {};
    return createIndex(node->parent->row, 0, node->parent);
}

int ProjectTreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;
    return static_cast<int>(nodeFromIndex(parent)->children.size());
}

int ProjectTreeModel::columnCount(const QModelIndex &) const
{
    return 2;
}

Qt::ItemFlags ProjectTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    Qt::ItemFlags f = QAbstractItemModel::flags(index);
    Node *n = nodeFromIndex(index);
    if (!n) return f;
    // Allow inline renaming for topology nodes, devices, GAs, and building nodes
    if (n->kind == Area || n->kind == Line || n->kind == Device ||
        n->kind == GroupAddress || n->kind == BuildingNode)
        f |= Qt::ItemIsEditable;
    return f;
}

QVariant ProjectTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};
    Node *n = static_cast<Node *>(index.internalPointer());
    if (!n)
        return {};

    switch (role) {
    case Qt::DisplayRole:
        return index.column() == 0 ? n->label : n->details;
    case Qt::EditRole:
        // For edit, return just the name part without prefix (for Area/Line: name only)
        if (index.column() == 0) {
            if (n->kind == Area || n->kind == Line) {
                if (auto *tn = static_cast<TopologyNode *>(n->rawPtr))
                    return tn->name();
            } else if (n->kind == Device) {
                if (auto *dev = static_cast<DeviceInstance *>(n->rawPtr))
                    return dev->description();
            } else if (n->kind == GroupAddress) {
                if (auto *gaPtr = static_cast<::GroupAddress *>(n->rawPtr))
                    return gaPtr->name();
            } else if (n->kind == BuildingNode) {
                if (auto *bp = static_cast<BuildingPart *>(n->rawPtr))
                    return bp->name();
            }
        }
        return {};
    case KindRole:
        return static_cast<int>(n->kind);
    case DevicePtrRole:
        return n->kind == Device ? QVariant::fromValue(n->rawPtr) : QVariant();
    case GroupAddressPtrRole:
        return n->kind == GroupAddress ? QVariant::fromValue(n->rawPtr) : QVariant();
    case BuildingPartPtrRole:
        return n->kind == BuildingNode ? QVariant::fromValue(n->rawPtr) : QVariant();
    case TopologyNodePtrRole:
        return (n->kind == Area || n->kind == Line) ? QVariant::fromValue(n->rawPtr) : QVariant();
    }
    return {};
}

bool ProjectTreeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole || !index.isValid())
        return false;
    Node *n = nodeFromIndex(index);
    if (!n) return false;

    const QString newName = value.toString().trimmed();
    if (newName.isEmpty()) return false;

    if ((n->kind == Area || n->kind == Line) && n->rawPtr) {
        static_cast<TopologyNode *>(n->rawPtr)->setName(newName);
        // Rebuild label
        if (n->kind == Area) {
            auto *tn = static_cast<TopologyNode *>(n->rawPtr);
            n->label = tr("%1 %2").arg(QString::number(tn->id()), newName);
        } else {
            auto *tn = static_cast<TopologyNode *>(n->rawPtr);
            if (n->parent && n->parent->rawPtr) {
                auto *area = static_cast<TopologyNode *>(n->parent->rawPtr);
                n->label = tr("%1.%2 %3").arg(area->id()).arg(tn->id()).arg(newName);
            }
        }
    } else if (n->kind == Device && n->rawPtr) {
        auto *dev = static_cast<DeviceInstance *>(n->rawPtr);
        dev->setDescription(newName);
        const QString displayName = newName;
        n->label = dev->physicalAddress().isEmpty()
                       ? tr("(neu) %1").arg(displayName)
                       : tr("%1 – %2").arg(dev->physicalAddress(), displayName);
    } else if (n->kind == GroupAddress && n->rawPtr) {
        auto *gaPtr = static_cast<::GroupAddress *>(n->rawPtr);
        gaPtr->setName(newName);
        n->label = tr("%1 – %2").arg(gaPtr->toString(), newName);
    } else if (n->kind == BuildingNode && n->rawPtr) {
        static_cast<BuildingPart *>(n->rawPtr)->setName(newName);
        n->label = newName;
    } else {
        return false;
    }

    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    return true;
}

QVariant ProjectTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    return section == 0 ? tr("Element") : tr("Typ");
}
