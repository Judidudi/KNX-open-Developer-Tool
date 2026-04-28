#pragma once

#include "KnxApplicationProgram.h"
#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include <memory>

// Catalog entry: one hardware product from a .knxprod file.
struct KnxHardwareProduct {
    QString productId;         // e.g. "M-00FA_H-1234_HP-1234"
    QString productName;
    QString manufacturer;
    QString appProgramRefId;   // references KnxApplicationProgram::id
    std::shared_ptr<KnxApplicationProgram> appProgram; // resolved after load
};

// Scans directories for *.knxprod files and provides access to the parsed
// application programs. YAML manifests in the same directories are
// auto-converted to .knxprod on first encounter.
class KnxprodCatalog
{
public:
    KnxprodCatalog();

    void addSearchPath(const QString &path);
    void reload();

    int count() const { return m_products.size(); }
    const KnxHardwareProduct *at(int index) const;

    // Lookup by product ref ID (M-XXXX_H-YYYY_HP-YYYY)
    std::shared_ptr<KnxApplicationProgram> sharedByProductRef(const QString &productId) const;
    const KnxHardwareProduct *findProduct(const QString &productId) const;

    QList<KnxHardwareProduct> products() const { return m_products; }

private:
    bool loadKnxprod(const QString &path);

    QStringList                    m_paths;
    QList<KnxHardwareProduct>      m_products;
    QMap<QString, std::shared_ptr<KnxApplicationProgram>> m_appPrograms;
};
