#pragma once

#include <QString>
#include <QByteArray>

struct Manifest;

// Converts a YAML device manifest (Manifest struct) to a .knxprod ZIP file.
// The generated file follows the KNX .knxprod format understood by ETS 6.
// OpenKNX manufacturer ID M-00FA is used; device IDs are derived
// deterministically from the manifest ID.
class YamlToKnxprod
{
public:
    // Generate KNX IDs that will be used for this manifest
    static QString productRefId(const Manifest &m);
    static QString appProgramRefId(const Manifest &m);

    // Convert and write to file; returns true on success
    static bool writeFile(const Manifest &m, const QString &outputPath);

    // Convert to in-memory ZIP bytes
    static QByteArray toZip(const Manifest &m);

private:
    static QString hash4(const QString &s);
    static QString versionHex(const QString &version);

    static QByteArray buildHardwareXml(const Manifest &m,
                                        const QString &h4,
                                        const QString &ver);
    static QByteArray buildApplicationXml(const Manifest &m,
                                           const QString &h4,
                                           const QString &ver);
};
