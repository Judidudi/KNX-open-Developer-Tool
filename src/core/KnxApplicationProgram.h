#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include <QVariant>
#include <memory>
#include <cstdint>

// ─── Parameter type ───────────────────────────────────────────────────────────

struct KnxEnumValue {
    int     value = 0;
    QString text;
};

struct KnxParameterType {
    QString id;

    enum class Kind { Bool, UInt, Enum };
    Kind    kind     = Kind::UInt;
    uint8_t size     = 1;      // bytes (1, 2 or 4)
    int     minValue = 0;
    int     maxValue = 255;

    QList<KnxEnumValue> enumValues;
};

// ─── Parameter ────────────────────────────────────────────────────────────────

struct KnxParameter {
    QString  id;
    QString  name;
    QString  typeId;           // references KnxParameterType::id in the enclosing program
    uint32_t offset       = 0; // byte offset within parameter block
    QVariant defaultValue;
};

// ─── Communication object ─────────────────────────────────────────────────────

struct KnxComObject {
    QString     id;
    int         number = 0;
    QString     name;
    QString     dpt;           // e.g. "DPT-1" or "1.001"
    QStringList flags;         // C, R, W, T, U
};

// ─── Memory layout ────────────────────────────────────────────────────────────

struct KnxMemoryLayout {
    uint32_t addressTable     = 0x4000;
    uint32_t associationTable = 0x4100;
    uint32_t comObjectTable   = 0x4200;
    uint32_t parameterBase    = 0x4400;
    uint32_t parameterSize    = 0;
};

// ─── Application program ──────────────────────────────────────────────────────

// Parsed representation of a KNX application program (from .knxprod XML or
// converted from a YAML manifest). Shared between the catalog, the device
// editor and the device programmer.
class KnxApplicationProgram
{
public:
    QString id;           // e.g. "M-00FA_A-1234-0001"
    QString name;
    QString manufacturer;

    QMap<QString, KnxParameterType> paramTypes;
    QList<KnxParameter>             parameters;
    QList<KnxComObject>             comObjects;
    KnxMemoryLayout                 memoryLayout;

    bool isValid() const { return !id.isEmpty(); }

    const KnxParameter     *findParameter(const QString &id) const;
    const KnxComObject     *findComObject(const QString &id) const;
    const KnxParameterType *findType(const QString &typeId) const;

    // Effective parameter size in bytes (falls back to 1 if type not found)
    uint32_t effectiveSize(const KnxParameter &param) const;
};
