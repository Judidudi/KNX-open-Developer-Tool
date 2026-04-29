#pragma once

#include "ComObjectLink.h"
#include <QString>
#include <QVariant>
#include <QList>
#include <map>
#include <memory>

class KnxApplicationProgram;

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

    QString description() const { return m_description; }
    void    setDescription(const QString &desc) { m_description = desc; }

    // Parameter values keyed by parameter id.
    // std::map is used instead of QMap because Qt 6.4 requires nothrow-destructible
    // value types for Qt containers, and QVariant does not guarantee that.
    std::map<QString, QVariant> &parameters()             { return m_params; }
    const std::map<QString, QVariant> &parameters() const { return m_params; }

    // ComObject links
    void                         addLink(ComObjectLink link);
    QList<ComObjectLink>        &links()             { return m_links; }
    const QList<ComObjectLink>  &links() const       { return m_links; }

    // Resolved application program (set by KnxprodCatalog after loading)
    void setAppProgram(std::shared_ptr<KnxApplicationProgram> p) { m_appProgram = std::move(p); }
    KnxApplicationProgram       *appProgram()       { return m_appProgram.get(); }
    const KnxApplicationProgram *appProgram() const { return m_appProgram.get(); }

private:
    QString                                  m_id;
    QString                                  m_productRefId;
    QString                                  m_appProgramRefId;
    QString                                  m_physAddr;
    QString                                  m_description;
    std::map<QString, QVariant>              m_params;
    QList<ComObjectLink>                     m_links;
    std::shared_ptr<KnxApplicationProgram>   m_appProgram;
};
