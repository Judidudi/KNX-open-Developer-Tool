#pragma once

#include "GroupAddress.h"
#include <QString>
#include <QList>
#include <QDate>
#include <vector>
#include <memory>

class TopologyNode;
class BuildingPart;

class Project
{
public:
    Project();
    ~Project();

    QString name()    const { return m_name;    }
    QDate   created() const { return m_created; }

    void setName(const QString &name) { m_name = name; }
    void setCreated(const QDate &d)   { m_created = d; }

    // Stable KNX project ID (P-XXXXXXXX) preserved across save/load
    QString knxprojId()                   const { return m_knxprojId; }
    void    setKnxprojId(const QString &id)      { m_knxprojId = id;  }

    // Topology: list of areas (top-level nodes)
    void         addArea(std::unique_ptr<TopologyNode> area);
    void         removeAreaAt(int index);
    TopologyNode *areaAt(int index);
    int           areaCount() const;

    // Group addresses (flat list, sorted by raw address)
    void                 addGroupAddress(GroupAddress ga);
    void                 removeGroupAddress(const QString &gaString);
    QList<GroupAddress> &groupAddresses()             { return m_groupAddresses; }
    const QList<GroupAddress> &groupAddresses() const { return m_groupAddresses; }
    GroupAddress *findGroupAddress(const QString &gaString);

    // Building structure
    void          addBuilding(std::unique_ptr<BuildingPart> b);
    void          removeBuildingAt(int index);
    int           buildingCount() const;
    BuildingPart *buildingAt(int index) const;

private:
    QString      m_name;
    QDate        m_created;
    QString      m_knxprojId;

    std::vector<std::unique_ptr<TopologyNode>> m_areas;
    QList<GroupAddress>                        m_groupAddresses;
    std::vector<std::unique_ptr<BuildingPart>> m_buildings;
};
