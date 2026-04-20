#include "TopologyNode.h"
#include "DeviceInstance.h"

TopologyNode::TopologyNode(Type type, int id, const QString &name, TopologyNode *parent)
    : m_type(type), m_id(id), m_name(name), m_parent(parent)
{}

void TopologyNode::addChild(std::unique_ptr<TopologyNode> child)
{
    child->m_parent = this;
    m_children.append(std::move(child));
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
        return 0;
    for (int i = 0; i < m_parent->childCount(); ++i) {
        if (m_parent->childAt(i) == this)
            return i;
    }
    return 0;
}

void TopologyNode::addDevice(std::unique_ptr<DeviceInstance> device)
{
    m_devices.append(std::move(device));
}

DeviceInstance *TopologyNode::deviceAt(int index)
{
    if (index < 0 || index >= m_devices.size())
        return nullptr;
    return m_devices[index].get();
}

int TopologyNode::deviceCount() const
{
    return m_devices.size();
}
