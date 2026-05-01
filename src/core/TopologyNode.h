#pragma once

#include <QString>
#include <vector>
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
    void                             addChild(std::unique_ptr<TopologyNode> child);
    void                             removeChildAt(int index);
    std::unique_ptr<TopologyNode>    takeChildAt(int index);
    void                             insertChildAt(int index, std::unique_ptr<TopologyNode> child);
    TopologyNode       *childAt(int index);
    const TopologyNode *childAt(int index) const;
    int                 childCount() const;
    int                 indexInParent() const;
    int                 indexOfChild(const TopologyNode *child) const;

    // Devices (only valid on Line nodes)
    void                             addDevice(std::unique_ptr<DeviceInstance> device);
    void                             removeDeviceAt(int index);
    std::unique_ptr<DeviceInstance>  takeDeviceAt(int index);
    void                             insertDeviceAt(int index, std::unique_ptr<DeviceInstance> device);
    DeviceInstance       *deviceAt(int index);
    const DeviceInstance *deviceAt(int index) const;
    int                   deviceCount() const;
    int             indexOfDevice(const DeviceInstance *dev) const;

private:
    Type         m_type;
    int          m_id;
    QString      m_name;
    TopologyNode *m_parent = nullptr;

    // std::vector instead of QList: unique_ptr is move-only, QList requires copyable types.
    std::vector<std::unique_ptr<TopologyNode>>   m_children;
    std::vector<std::unique_ptr<DeviceInstance>> m_devices;
};
