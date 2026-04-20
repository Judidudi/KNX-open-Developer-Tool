#pragma once

#include <QString>
#include <QList>
#include <memory>

class DeviceInstance;

// Tree node for the KNX topology.
// Hierarchy: Area → Line → DeviceInstance (leaves are devices)
class TopologyNode
{
public:
    enum class Type { Area, Line };

    explicit TopologyNode(Type type, int id, const QString &name,
                          TopologyNode *parent = nullptr);

    Type         type()     const { return m_type;     }
    int          id()       const { return m_id;       }
    QString      name()     const { return m_name;     }
    TopologyNode *parent()  const { return m_parent;   }

    void setName(const QString &name) { m_name = name; }

    // Child nodes (Areas contain Lines)
    void                addChild(std::unique_ptr<TopologyNode> child);
    TopologyNode       *childAt(int index);
    const TopologyNode *childAt(int index) const;
    int                 childCount() const;
    int                 indexInParent() const;

    // Devices (only valid on Line nodes)
    void           addDevice(std::unique_ptr<DeviceInstance> device);
    DeviceInstance *deviceAt(int index);
    int             deviceCount() const;

private:
    Type         m_type;
    int          m_id;
    QString      m_name;
    TopologyNode *m_parent = nullptr;

    QList<std::unique_ptr<TopologyNode>>   m_children;
    QList<std::unique_ptr<DeviceInstance>> m_devices;
};
