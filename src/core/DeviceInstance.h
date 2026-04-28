#pragma once

#include "ComObjectLink.h"
#include <QString>
#include <QVariant>
#include <QList>
#include <map>
#include <memory>

struct Manifest;

// A concrete device placed in the topology.
// Stores instance-specific data: physical address, KNX product/app refs,
// parameter values and ComObject↔GA links.
class DeviceInstance
{
public:
    DeviceInstance(const QString &id,
                   const QString &productRefId,
                   const QString &appProgramRefId);

    QString id()             const { return m_id;             }
    QString productRefId()   const { return m_productRefId;   }
    QString appProgramRefId()const { return m_appProgramRefId;}

    QString physicalAddress() const { return m_physAddr; }
    void    setPhysicalAddress(const QString &addr) { m_physAddr = addr; }

    // Parameter values keyed by parameter id.
    // std::map is used instead of QMap because Qt 6.4 requires nothrow-destructible
    // value types for Qt containers, and QVariant does not guarantee that.
    std::map<QString, QVariant> &parameters()             { return m_params; }
    const std::map<QString, QVariant> &parameters() const { return m_params; }

    // ComObject links
    void                         addLink(ComObjectLink link);
    QList<ComObjectLink>        &links()             { return m_links; }
    const QList<ComObjectLink>  &links() const       { return m_links; }

    // Resolved manifest (set by DeviceCatalog after loading, removed in Phase B)
    void            setManifest(std::shared_ptr<Manifest> m) { m_manifest = std::move(m); }
    Manifest       *manifest()       { return m_manifest.get(); }
    const Manifest *manifest() const { return m_manifest.get(); }

private:
    QString                      m_id;
    QString                      m_productRefId;
    QString                      m_appProgramRefId;
    QString                      m_physAddr;
    std::map<QString, QVariant>  m_params;
    QList<ComObjectLink>         m_links;
    std::shared_ptr<Manifest>    m_manifest;
};
