#pragma once

#include "Manifest.h"
#include <QString>
#include <QList>
#include <memory>

// Scans one or more directories for *.yaml manifest files and provides
// access to the parsed manifests.
class DeviceCatalog
{
public:
    DeviceCatalog();

    void addSearchPath(const QString &path);
    void reload();

    int                             count() const;
    const Manifest                 *at(int index) const;
    const Manifest                 *findById(const QString &id) const;
    std::shared_ptr<Manifest>       sharedById(const QString &id) const;
    QList<std::shared_ptr<Manifest>> allManifests() const { return m_manifests; }

private:
    QStringList                      m_paths;
    QList<std::shared_ptr<Manifest>> m_manifests;
};
