#include "BuildingPart.h"

BuildingPart::BuildingPart(Type type, const QString &name)
    : m_type(type), m_name(name)
{}

void BuildingPart::addChild(std::unique_ptr<BuildingPart> child)
{
    child->m_parent = this;
    m_children.push_back(std::move(child));
}

void BuildingPart::removeChildAt(int index)
{
    if (index >= 0 && index < static_cast<int>(m_children.size()))
        m_children.erase(m_children.begin() + index);
}

int BuildingPart::childCount() const
{
    return static_cast<int>(m_children.size());
}

BuildingPart *BuildingPart::childAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_children.size()))
        return nullptr;
    return m_children[index].get();
}

int BuildingPart::indexInParent() const
{
    if (!m_parent)
        return -1;
    for (int i = 0; i < m_parent->childCount(); ++i) {
        if (m_parent->childAt(i) == this)
            return i;
    }
    return -1;
}

void BuildingPart::addGroupAddressRef(const QString &gaId)
{
    if (!m_gaRefs.contains(gaId))
        m_gaRefs.append(gaId);
}

void BuildingPart::removeGroupAddressRef(const QString &gaId)
{
    m_gaRefs.removeAll(gaId);
}

void BuildingPart::addDeviceRef(const QString &deviceId)
{
    if (!m_deviceRefs.contains(deviceId))
        m_deviceRefs.append(deviceId);
}

void BuildingPart::removeDeviceRef(const QString &deviceId)
{
    m_deviceRefs.removeAll(deviceId);
}

QString BuildingPart::typeToString(Type t)
{
    switch (t) {
    case Type::Building:          return QStringLiteral("Building");
    case Type::Floor:             return QStringLiteral("Floor");
    case Type::Room:              return QStringLiteral("Room");
    case Type::DistributionBoard: return QStringLiteral("DistributionBoard");
    case Type::Corridor:          return QStringLiteral("Corridor");
    case Type::Other:
    default:                      return QStringLiteral("Other");
    }
}

BuildingPart::Type BuildingPart::typeFromString(const QString &s)
{
    if (s == QLatin1String("Building"))          return Type::Building;
    if (s == QLatin1String("Floor"))             return Type::Floor;
    if (s == QLatin1String("Room"))              return Type::Room;
    if (s == QLatin1String("DistributionBoard")) return Type::DistributionBoard;
    if (s == QLatin1String("Corridor"))          return Type::Corridor;
    return Type::Other;
}
