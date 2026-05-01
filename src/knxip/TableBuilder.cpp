#include "TableBuilder.h"

#include "../core/DeviceInstance.h"
#include "../core/KnxApplicationProgram.h"
#include "../core/ComObjectLink.h"
#include "../core/GroupAddress.h"
#include "CemiFrame.h"

#include <QHash>
#include <QLoggingCategory>

namespace {

void appendU16(QByteArray &out, uint16_t v)
{
    out.append(static_cast<char>(v >> 8));
    out.append(static_cast<char>(v & 0xFF));
}

// Write `size` bytes of `value` (little-endian) at `offset` within `block`.
// Returns false and logs a warning if the range is out of bounds.
bool writeU32LE(QByteArray &block, uint32_t offset, uint32_t value, uint32_t size)
{
    if (size == 0 || size > 4) {
        qWarning("TableBuilder: writeU32LE invalid size %u at offset %u", size, offset);
        return false;
    }
    if (offset + size > static_cast<uint32_t>(block.size())) {
        qWarning("TableBuilder: writeU32LE out of bounds (offset=%u size=%u blockSize=%d)",
                 offset, size, block.size());
        return false;
    }
    // Clamp value to the declared byte width to catch firmware-side truncation bugs early
    const uint32_t mask = (size < 4) ? ((1u << (8 * size)) - 1u) : 0xFFFFFFFFu;
    if (value != (value & mask)) {
        qWarning("TableBuilder: parameter value 0x%X truncated to %u bytes (mask 0x%X)",
                 value, size, mask);
    }
    value &= mask;
    for (uint32_t i = 0; i < size; ++i)
        block[static_cast<int>(offset + i)] = static_cast<char>((value >> (8 * i)) & 0xFF);
    return true;
}

} // namespace

DeviceMemoryImage TableBuilder::build(const DeviceInstance        &device,
                                      const KnxApplicationProgram &appProgram)
{
    DeviceMemoryImage img;

    // ── Address Table ─────────────────────────────────────────────────────────
    // Format: [count_hi][count_lo] [PA_hi][PA_lo] [GA1_hi][GA1_lo] ...
    // count = number of entries (1 PA + N GAs).  GA index fits in uint8 (0–254).
    QList<uint16_t> gaTable;
    QHash<uint16_t, int> gaIndex;

    for (const ComObjectLink &link : device.links()) {
        if (!link.ga.isValid())
            continue;
        const uint16_t raw = link.ga.toRaw();
        if (!gaIndex.contains(raw)) {
            if (gaTable.size() >= 254) {
                qWarning("TableBuilder: GA table full (254 entries max), skipping GA 0x%04X", raw);
                continue;
            }
            gaIndex.insert(raw, gaTable.size());
            gaTable.append(raw);
        }
    }

    const uint16_t pa = CemiFrame::physAddrFromString(device.physicalAddress());
    appendU16(img.addressTable, static_cast<uint16_t>(1 + gaTable.size()));
    appendU16(img.addressTable, pa);
    for (uint16_t ga : gaTable)
        appendU16(img.addressTable, ga);

    // ── Association Table ──────────────────────────────────────────────────────
    // Format: [count_hi][count_lo] { [ga_idx_uint8][co_number_uint8] } ...
    // Both indices must fit in uint8; skip entries that don't.
    QByteArray assoc;
    int assocCount = 0;

    for (const ComObjectLink &link : device.links()) {
        if (!link.ga.isValid())
            continue;
        const KnxComObject *co = appProgram.findComObject(link.comObjectId);
        if (!co)
            continue;
        const int gaIdx = gaIndex.value(link.ga.toRaw(), -1);
        if (gaIdx < 0)
            continue;
        if (gaIdx > 0xFF) {
            qWarning("TableBuilder: GA index %d > 255, skipping association entry", gaIdx);
            continue;
        }
        if (co->number > 0xFF) {
            qWarning("TableBuilder: ComObject number %d > 255, skipping association entry",
                     co->number);
            continue;
        }
        if (assocCount >= 0xFF) {
            qWarning("TableBuilder: association table full (255 entries max), skipping");
            continue;
        }
        assoc.append(static_cast<char>(gaIdx & 0xFF));
        assoc.append(static_cast<char>(co->number & 0xFF));
        ++assocCount;
    }
    appendU16(img.associationTable, static_cast<uint16_t>(assocCount));
    img.associationTable.append(assoc);

    // ── Parameter Block ────────────────────────────────────────────────────────
    const uint32_t paramSize = appProgram.memoryLayout.parameterSize;
    img.parameterBlock = QByteArray(static_cast<int>(paramSize), char(0));

    for (const KnxParameter &p : appProgram.parameters) {
        auto paramIt = device.parameters().find(p.id);
        const QVariant v = (paramIt != device.parameters().end())
                           ? paramIt->second : p.defaultValue;
        const uint32_t size = appProgram.effectiveSize(p);
        writeU32LE(img.parameterBlock, p.offset, static_cast<uint32_t>(v.toInt()), size);
    }

    return img;
}
