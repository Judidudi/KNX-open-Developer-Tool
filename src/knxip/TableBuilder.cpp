#include "TableBuilder.h"

#include "../core/DeviceInstance.h"
#include "../core/KnxApplicationProgram.h"
#include "../core/ComObjectLink.h"
#include "../core/GroupAddress.h"
#include "CemiFrame.h"

#include <QHash>

namespace {

void appendU16(QByteArray &out, uint16_t v)
{
    out.append(static_cast<char>(v >> 8));
    out.append(static_cast<char>(v & 0xFF));
}

void writeU32LE(QByteArray &block, uint32_t offset, uint32_t value, uint32_t size)
{
    if (offset + size > static_cast<uint32_t>(block.size()))
        return;
    for (uint32_t i = 0; i < size; ++i)
        block[offset + i] = static_cast<char>((value >> (8 * i)) & 0xFF);
}

} // namespace

DeviceMemoryImage TableBuilder::build(const DeviceInstance        &device,
                                      const KnxApplicationProgram &appProgram)
{
    DeviceMemoryImage img;

    // -- Address Table --------------------------------------------------
    QList<uint16_t> gaTable;
    QHash<uint16_t, int> gaIndex;

    for (const ComObjectLink &link : device.links()) {
        if (!link.ga.isValid())
            continue;
        const uint16_t raw = link.ga.toRaw();
        if (!gaIndex.contains(raw)) {
            gaIndex.insert(raw, gaTable.size());
            gaTable.append(raw);
        }
    }

    const uint16_t pa = CemiFrame::physAddrFromString(device.physicalAddress());
    appendU16(img.addressTable, static_cast<uint16_t>(1 + gaTable.size()));
    appendU16(img.addressTable, pa);
    for (uint16_t ga : gaTable)
        appendU16(img.addressTable, ga);

    // -- Association Table ---------------------------------------------
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

        assoc.append(static_cast<char>(gaIdx & 0xFF));
        assoc.append(static_cast<char>(co->number & 0xFF));
        ++assocCount;
    }
    appendU16(img.associationTable, static_cast<uint16_t>(assocCount));
    img.associationTable.append(assoc);

    // -- Parameter Block ------------------------------------------------
    const uint32_t paramSize = appProgram.memoryLayout.parameterSize;
    img.parameterBlock = QByteArray(static_cast<int>(paramSize), char(0));

    for (const KnxParameter &p : appProgram.parameters) {
        auto paramIt = device.parameters().find(p.id);
        const QVariant v = (paramIt != device.parameters().end())
                           ? paramIt->second : p.defaultValue;
        writeU32LE(img.parameterBlock, p.offset,
                   static_cast<uint32_t>(v.toInt()),
                   appProgram.effectiveSize(p));
    }

    return img;
}
