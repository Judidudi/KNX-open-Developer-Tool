#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QVariant>
#include <QMap>
#include <optional>
#include <cstdint>

// Parsed representation of a device YAML manifest.

struct ManifestLocalizedString {
    QString de;
    QString en;
    QString get(const QString &lang = QStringLiteral("de")) const {
        return lang == QStringLiteral("en") ? en : de;
    }
};

struct ManifestComObject {
    QString                 id;
    int                     number = 0;   // fixed index used in the association table
    QString                 channel;
    ManifestLocalizedString name;
    QString                 dpt;
    QStringList             flags;        // C, R, W, T, U
};

struct ManifestParameterEnumValue {
    int                     value;
    ManifestLocalizedString label;
};

struct ManifestParameter {
    QString                 id;
    ManifestLocalizedString name;
    QString                 type;             // bool, uint8, uint16, uint32, enum
    QString                 unit;
    QVariant                defaultValue;
    QVariant                rangeMin;
    QVariant                rangeMax;
    QList<ManifestParameterEnumValue> enumValues;
    QString                 visibilityCondition;  // e.g. "p_mode == 1"

    // Memory layout – byte offset from memoryLayout.parameterBase
    uint32_t                memoryOffset = 0;
    uint32_t                size         = 0;  // bytes; 0 means "derive from type"

    // Resolves parameter size in bytes. Falls back to type-derived default
    // (bool/uint8/enum = 1, uint16 = 2, uint32 = 4) if `size` is unset.
    uint32_t effectiveSize() const;
};

struct ManifestChannel {
    QString                 id;
    ManifestLocalizedString name;
};

struct ManifestHardware {
    QString target;
    QString transceiver;
};

// Memory addresses used by the standard KNX tables. All values are device-local
// memory addresses; the programmer writes data blocks to these addresses via
// A_Memory_Write.
struct ManifestMemoryLayout {
    uint32_t addressTable     = 0x4000; // PA + used GAs (16-bit big-endian entries)
    uint32_t associationTable = 0x4100; // (co_number, ga_index) pairs
    uint32_t comObjectTable   = 0x4200; // ComObject descriptor table
    uint32_t parameterBase    = 0x4400; // Parameter value block
    uint32_t parameterSize    = 0;      // total size of parameter block (bytes)
};

struct Manifest {
    QString                   id;
    QString                   version;
    QString                   manufacturer;
    ManifestLocalizedString   name;
    ManifestHardware          hardware;
    QList<ManifestChannel>    channels;
    QList<ManifestComObject>  comObjects;
    QList<ManifestParameter>  parameters;
    ManifestMemoryLayout      memoryLayout;

    bool isValid() const { return !id.isEmpty() && !version.isEmpty(); }
};

// Parses a YAML manifest file. Returns nullopt on error.
std::optional<Manifest> loadManifest(const QString &filePath);
