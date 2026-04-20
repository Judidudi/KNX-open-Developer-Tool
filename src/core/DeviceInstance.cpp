#include "DeviceInstance.h"

DeviceInstance::DeviceInstance(const QString &id,
                               const QString &catalogRef,
                               const QString &manifestVersion)
    : m_id(id)
    , m_catalogRef(catalogRef)
    , m_manifestVersion(manifestVersion)
{}

void DeviceInstance::addLink(ComObjectLink link)
{
    m_links.append(std::move(link));
}
