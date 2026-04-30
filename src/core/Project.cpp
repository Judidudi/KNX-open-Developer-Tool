#include "Project.h"
#include "TopologyNode.h"
#include "DeviceInstance.h"
#include "BuildingPart.h"

Project::Project()
    : m_created(QDate::currentDate())
{}

Project::~Project() = default;

void Project::addArea(std::unique_ptr<TopologyNode> area)
{
    m_areas.push_back(std::move(area));
}

void Project::removeAreaAt(int index)
{
    if (index >= 0 && index < static_cast<int>(m_areas.size()))
        m_areas.erase(m_areas.begin() + index);
}

TopologyNode *Project::areaAt(int index)
{
    if (index < 0 || index >= static_cast<int>(m_areas.size()))
        return nullptr;
    return m_areas[index].get();
}

int Project::areaCount() const
{
    return static_cast<int>(m_areas.size());
}

void Project::addGroupAddress(GroupAddress ga)
{
    m_groupAddresses.append(std::move(ga));
}

void Project::removeGroupAddress(const QString &gaString)
{
    m_groupAddresses.removeIf([&gaString](const GroupAddress &ga){
        return ga.toString() == gaString;
    });
}

GroupAddress *Project::findGroupAddress(const QString &gaString)
{
    for (auto &ga : m_groupAddresses) {
        if (ga.toString() == gaString)
            return &ga;
    }
    return nullptr;
}

void Project::addBuilding(std::unique_ptr<BuildingPart> b)
{
    m_buildings.push_back(std::move(b));
}

void Project::removeBuildingAt(int index)
{
    if (index >= 0 && index < static_cast<int>(m_buildings.size()))
        m_buildings.erase(m_buildings.begin() + index);
}

int Project::buildingCount() const
{
    return static_cast<int>(m_buildings.size());
}

BuildingPart *Project::buildingAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_buildings.size()))
        return nullptr;
    return m_buildings[index].get();
}
