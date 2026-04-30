#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <vector>
#include <memory>

// Represents a node in the ETS building structure (Building → Floor → Room etc.)
class BuildingPart
{
public:
    enum class Type { Building, Floor, Room, DistributionBoard, Corridor, Other };

    BuildingPart(Type type, const QString &name);

    Type    type() const { return m_type; }
    QString name() const { return m_name; }
    void    setName(const QString &name) { m_name = name; }

    void         addChild(std::unique_ptr<BuildingPart> child);
    void         removeChildAt(int index);
    int          childCount() const;
    BuildingPart *childAt(int index) const;
    BuildingPart *parent() const { return m_parent; }
    int          indexInParent() const;

    void              addGroupAddressRef(const QString &gaId);
    void              removeGroupAddressRef(const QString &gaId);
    const QStringList &groupAddressRefs() const { return m_gaRefs; }

    void              addDeviceRef(const QString &deviceId);
    void              removeDeviceRef(const QString &deviceId);
    const QStringList &deviceRefs() const { return m_deviceRefs; }

    // ETS-style type string for serialization
    static QString typeToString(Type t);
    static Type    typeFromString(const QString &s);

private:
    Type         m_type;
    QString      m_name;
    BuildingPart *m_parent = nullptr;
    // std::vector: unique_ptr is move-only, QList requires copyable types
    std::vector<std::unique_ptr<BuildingPart>> m_children;
    QStringList  m_gaRefs;
    QStringList  m_deviceRefs;
};
