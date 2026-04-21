#pragma once

#include "GroupAddress.h"
#include <QString>
#include <QList>
#include <QDate>
#include <vector>
#include <memory>

class TopologyNode;

class Project
{
public:
    Project();
    ~Project();

    QString name()    const { return m_name;    }
    QDate   created() const { return m_created; }

    void setName(const QString &name) { m_name = name; }
    void setCreated(const QDate &d)   { m_created = d; }

    // Topology: list of areas (top-level nodes)
    void         addArea(std::unique_ptr<TopologyNode> area);
    TopologyNode *areaAt(int index);
    int           areaCount() const;

    // Group addresses (flat list, sorted by raw address)
    void                 addGroupAddress(GroupAddress ga);
    QList<GroupAddress> &groupAddresses()             { return m_groupAddresses; }
    const QList<GroupAddress> &groupAddresses() const { return m_groupAddresses; }
    GroupAddress *findGroupAddress(const QString &gaString);

private:
    QString      m_name;
    QDate        m_created;

    std::vector<std::unique_ptr<TopologyNode>> m_areas;
    QList<GroupAddress>                  m_groupAddresses;
};
