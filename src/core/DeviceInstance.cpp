#include "DeviceInstance.h"

DeviceInstance::DeviceInstance(const QString &id,
                               const QString &productRefId,
                               const QString &appProgramRefId)
    : m_id(id)
    , m_productRefId(productRefId)
    , m_appProgramRefId(appProgramRefId)
{}

void DeviceInstance::addLink(ComObjectLink link)
{
    m_links.append(std::move(link));
}
