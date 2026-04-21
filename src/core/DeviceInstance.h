#pragma once

#include "ComObjectLink.h"
#include <QString>
#include <QVariant>
#include <QList>
#include <map>
#include <memory>

struct Manifest;

// A concrete device placed in the topology.
// References a manifest (device type) and stores instance-specific data:
// physical address, parameter values and ComObject↔GA links.
class DeviceInstance
{
public:
    DeviceInstance(const QString &id,
                   const QString &catalogRef,
                   const QString &manifestVersion);

    QString id()              const { return m_id;              }
    QString catalogRef()      const { return m_catalogRef;      }
    QString manifestVersion() const { return m_manifestVersion; }

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

    // Resolved manifest (set by DeviceCatalog after loading)
    void            setManifest(std::shared_ptr<Manifest> m) { m_manifest = std::move(m); }
    Manifest       *manifest()       { return m_manifest.get(); }
    const Manifest *manifest() const { return m_manifest.get(); }

private:
    QString                      m_id;
    QString                      m_catalogRef;
    QString                      m_manifestVersion;
    QString                      m_physAddr;
    std::map<QString, QVariant>  m_params;
    QList<ComObjectLink>         m_links;
    std::shared_ptr<Manifest>    m_manifest;
};
