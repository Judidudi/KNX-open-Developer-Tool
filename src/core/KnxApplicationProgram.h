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

    // Visibility (from <ParameterRef> in the .knxprod Application XML)
    enum class Access { ReadWrite, ReadOnly, Hidden };
    Access   access = Access::ReadWrite;

    // Conditional visibility: visible only when conditionParamId's current value
    // satisfies the condition (empty conditionParamId means always visible)
    QString     conditionParamId;
    QVariant    conditionValue;
    enum class ConditionOp { Equal, NotEqual };
    ConditionOp conditionOp = ConditionOp::Equal;

    // ParameterBlock grouping (from <ParameterBlock Text="…"> in .knxprod)
    QString groupName;
};

// ─── Communication object ─────────────────────────────────────────────────────

struct KnxComObject {
    QString     id;
    int         number = 0;
    QString     name;
    QString     dpt;            // active/primary DPT, e.g. "1.001"
    QStringList supportedDpts;  // all DPTs this CO can operate with (multi-DPT devices)
    QStringList flags;          // C, R, W, T, U
};

// ─── Memory layout ────────────────────────────────────────────────────────────

struct KnxMemoryLayout {
    uint32_t addressTable     = 0x4000;
    uint32_t associationTable = 0x4100;
    uint32_t comObjectTable   = 0x4200;
    uint32_t parameterBase    = 0x4400;
    uint32_t parameterSize    = 0;
};

// ─── Parameter block ──────────────────────────────────────────────────────────

// Named group of parameters (corresponds to a <ParameterBlock> in .knxprod).
// Used by the UI to show parameter pages (like ETS's tab/page selector).
struct KnxParameterBlock {
    QString id;
    QString displayText;         // e.g. "General settings", "LED 1"
    QStringList paramRefRefIds;  // ParameterRefRef.RefId entries in this block
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
    QList<KnxParameterBlock>        paramBlocks;  // named parameter groups
    QList<KnxComObject>             comObjects;
    KnxMemoryLayout                 memoryLayout;

    bool isValid() const { return !id.isEmpty(); }

    const KnxParameter     *findParameter(const QString &id) const;
    const KnxComObject     *findComObject(const QString &id) const;
    const KnxParameterType *findType(const QString &typeId) const;

    // Effective parameter size in bytes (falls back to 1 if type not found)
    uint32_t effectiveSize(const KnxParameter &param) const;
};
