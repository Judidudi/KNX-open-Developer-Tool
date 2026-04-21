#include "InterfaceManager.h"
#include "IKnxInterface.h"

InterfaceManager::InterfaceManager(QObject *parent)
    : QObject(parent)
{}

InterfaceManager::~InterfaceManager() = default;

void InterfaceManager::setInterface(std::unique_ptr<IKnxInterface> iface)
{
    if (m_iface) {
        m_iface->disconnectFromInterface();
        m_iface.reset();
    }
    m_iface = std::move(iface);
    if (!m_iface)
        return;

    // Forward all four signals so subscribers can stay bound to the manager
    connect(m_iface.get(), &IKnxInterface::connected,
            this, &InterfaceManager::connected);
    connect(m_iface.get(), &IKnxInterface::disconnected,
            this, &InterfaceManager::disconnected);
    connect(m_iface.get(), &IKnxInterface::cemiFrameReceived,
            this, &InterfaceManager::cemiFrameReceived);
    connect(m_iface.get(), &IKnxInterface::errorOccurred,
            this, &InterfaceManager::errorOccurred);
}

bool InterfaceManager::isConnected() const
{
    return m_iface && m_iface->isConnected();
}
