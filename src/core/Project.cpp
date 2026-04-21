#include "Project.h"
#include "TopologyNode.h"

Project::Project()
    : m_created(QDate::currentDate())
{}

Project::~Project() = default;

void Project::addArea(std::unique_ptr<TopologyNode> area)
{
    m_areas.push_back(std::move(area));
}

TopologyNode *Project::areaAt(int index)
{
    if (index < 0 || index >= m_areas.size())
        return nullptr;
    return m_areas[index].get();
}

int Project::areaCount() const
{
    return m_areas.size();
}

void Project::addGroupAddress(GroupAddress ga)
{
    m_groupAddresses.append(std::move(ga));
}

GroupAddress *Project::findGroupAddress(const QString &gaString)
{
    for (auto &ga : m_groupAddresses) {
        if (ga.toString() == gaString)
            return &ga;
    }
    return nullptr;
}
