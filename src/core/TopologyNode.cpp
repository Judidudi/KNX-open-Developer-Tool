#include "TopologyNode.h"
#include "DeviceInstance.h"
#include <QtAlgorithms>

TopologyNode::TopologyNode(Type type, int id, const QString &name, TopologyNode *parent)
    : m_type(type), m_id(id), m_name(name), m_parent(parent)
{}

void TopologyNode::addChild(std::unique_ptr<TopologyNode> child)
{
    child->m_parent = this;
    m_children.push_back(std::move(child));
}

void TopologyNode::removeChildAt(int index)
{
    if (index >= 0 && index < static_cast<int>(m_children.size()))
        m_children.erase(m_children.begin() + index);
}

std::unique_ptr<TopologyNode> TopologyNode::takeChildAt(int index)
{
    if (index < 0 || index >= static_cast<int>(m_children.size()))
        return nullptr;
    auto node = std::move(m_children[index]);
    m_children.erase(m_children.begin() + index);
    node->m_parent = nullptr;
    return node;
}

void TopologyNode::insertChildAt(int index, std::unique_ptr<TopologyNode> child)
{
    child->m_parent = this;
    const int clamped = qBound(0, index, static_cast<int>(m_children.size()));
    m_children.insert(m_children.begin() + clamped, std::move(child));
}

int TopologyNode::indexOfChild(const TopologyNode *child) const
{
    for (int i = 0; i < static_cast<int>(m_children.size()); ++i) {
        if (m_children[i].get() == child)
            return i;
    }
    return -1;
}

TopologyNode *TopologyNode::childAt(int index)
{
    if (index < 0 || index >= m_children.size())
        return nullptr;
    return m_children[index].get();
}

const TopologyNode *TopologyNode::childAt(int index) const
{
    if (index < 0 || index >= m_children.size())
        return nullptr;
    return m_children[index].get();
}

int TopologyNode::childCount() const
{
    return m_children.size();
}

int TopologyNode::indexInParent() const
{
    if (!m_parent)
        return -1;
    for (int i = 0; i < m_parent->childCount(); ++i) {
        if (m_parent->childAt(i) == this)
            return i;
    }
    return -1;
}

void TopologyNode::addDevice(std::unique_ptr<DeviceInstance> device)
{
    m_devices.push_back(std::move(device));
}

void TopologyNode::removeDeviceAt(int index)
{
    if (index >= 0 && index < static_cast<int>(m_devices.size()))
        m_devices.erase(m_devices.begin() + index);
}

std::unique_ptr<DeviceInstance> TopologyNode::takeDeviceAt(int index)
{
    if (index < 0 || index >= static_cast<int>(m_devices.size()))
        return nullptr;
    auto dev = std::move(m_devices[index]);
    m_devices.erase(m_devices.begin() + index);
    return dev;
}

void TopologyNode::insertDeviceAt(int index, std::unique_ptr<DeviceInstance> device)
{
    const int clamped = qBound(0, index, static_cast<int>(m_devices.size()));
    m_devices.insert(m_devices.begin() + clamped, std::move(device));
}

DeviceInstance *TopologyNode::deviceAt(int index)
{
    if (index < 0 || index >= static_cast<int>(m_devices.size()))
        return nullptr;
    return m_devices[index].get();
}

const DeviceInstance *TopologyNode::deviceAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_devices.size()))
        return nullptr;
    return m_devices[index].get();
}

int TopologyNode::deviceCount() const
{
    return static_cast<int>(m_devices.size());
}

int TopologyNode::indexOfDevice(const DeviceInstance *dev) const
{
    for (int i = 0; i < static_cast<int>(m_devices.size()); ++i) {
        if (m_devices[i].get() == dev)
            return i;
    }
    return -1;
}
