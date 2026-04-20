#include "DeviceCatalog.h"
#include "Manifest.h"
#include <QDir>
#include <QFileInfo>

DeviceCatalog::DeviceCatalog() = default;

void DeviceCatalog::addSearchPath(const QString &path)
{
    if (!m_paths.contains(path))
        m_paths.append(path);
}

void DeviceCatalog::reload()
{
    m_manifests.clear();
    for (const QString &path : m_paths) {
        const QDir dir(path);
        if (!dir.exists())
            continue;
        const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.yaml")}, QDir::Files);
        for (const QFileInfo &fi : files) {
            auto result = loadManifest(fi.absoluteFilePath());
            if (result)
                m_manifests.append(std::make_shared<Manifest>(std::move(*result)));
        }
    }
}

int DeviceCatalog::count() const
{
    return m_manifests.size();
}

const Manifest *DeviceCatalog::at(int index) const
{
    if (index < 0 || index >= m_manifests.size())
        return nullptr;
    return m_manifests[index].get();
}

const Manifest *DeviceCatalog::findById(const QString &id) const
{
    for (const auto &m : m_manifests)
        if (m->id == id) return m.get();
    return nullptr;
}

std::shared_ptr<Manifest> DeviceCatalog::sharedById(const QString &id) const
{
    for (const auto &m : m_manifests)
        if (m->id == id) return m;
    return nullptr;
}
