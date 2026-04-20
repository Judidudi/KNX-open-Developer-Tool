#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QVariant>
#include <QMap>
#include <optional>

// Parsed representation of a device YAML manifest.

struct ManifestLocalizedString {
    QString de;
    QString en;
    QString get(const QString &lang = QStringLiteral("de")) const {
        return lang == QStringLiteral("en") ? en : de;
    }
};

struct ManifestComObject {
    QString               id;
    QString               channel;
    ManifestLocalizedString name;
    QString               dpt;
    QStringList           flags;  // C, R, W, T, U
};

struct ManifestParameterEnumValue {
    int     value;
    ManifestLocalizedString label;
};

struct ManifestParameter {
    QString               id;
    ManifestLocalizedString name;
    QString               type;    // bool, uint8, uint16, uint32, enum
    QString               unit;
    QVariant              defaultValue;
    QVariant              rangeMin;
    QVariant              rangeMax;
    QList<ManifestParameterEnumValue> enumValues;
    QString               visibilityCondition; // e.g. "p_mode == 1"
};

struct ManifestChannel {
    QString               id;
    ManifestLocalizedString name;
};

struct ManifestHardware {
    QString target;
    QString transceiver;
};

struct ManifestMemoryLayout {
    uint32_t baseAddress = 0x4000;
};

struct Manifest {
    QString               id;
    QString               version;
    QString               manufacturer;
    ManifestLocalizedString name;
    ManifestHardware      hardware;
    QList<ManifestChannel>    channels;
    QList<ManifestComObject>  comObjects;
    QList<ManifestParameter>  parameters;
    ManifestMemoryLayout  memoryLayout;

    bool isValid() const { return !id.isEmpty() && !version.isEmpty(); }
};

// Parses a YAML manifest file. Returns nullopt on error.
std::optional<Manifest> loadManifest(const QString &filePath);
