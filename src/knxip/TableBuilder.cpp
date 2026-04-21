#include "TableBuilder.h"

#include "../core/DeviceInstance.h"
#include "../core/ComObjectLink.h"
#include "../core/GroupAddress.h"
#include "CemiFrame.h"

#include <QHash>
#include <algorithm>

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

DeviceMemoryImage TableBuilder::build(const DeviceInstance &device, const Manifest &manifest)
{
    DeviceMemoryImage img;

    // -- Address Table --------------------------------------------------
    //
    // Collect all unique GAs that are linked to a ComObject on this device.
    // The table format mirrors classic KNX devices: first entry is the
    // device physical address, followed by the list of group addresses.
    QList<uint16_t> gaTable;
    QHash<uint16_t, int> gaIndex;  // GA raw -> index in table

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
    // Count = 1 (for PA) + number of GAs
    appendU16(img.addressTable, static_cast<uint16_t>(1 + gaTable.size()));
    appendU16(img.addressTable, pa);
    for (uint16_t ga : gaTable)
        appendU16(img.addressTable, ga);

    // -- Association Table ---------------------------------------------
    //
    // One entry per ComObject link that has a valid GA. Entry format:
    //   [ga_index_in_address_table] [com_object_number]
    QByteArray assoc;
    int assocCount = 0;
    for (const ComObjectLink &link : device.links()) {
        if (!link.ga.isValid())
            continue;

        // Resolve ComObject number from the manifest by id
        int coNumber = -1;
        for (const ManifestComObject &co : manifest.comObjects) {
            if (co.id == link.comObjectId) {
                coNumber = co.number;
                break;
            }
        }
        if (coNumber < 0)
            continue;

        const int gaIdx = gaIndex.value(link.ga.toRaw(), -1);
        if (gaIdx < 0)
            continue;

        assoc.append(static_cast<char>(gaIdx & 0xFF));
        assoc.append(static_cast<char>(coNumber & 0xFF));
        ++assocCount;
    }
    appendU16(img.associationTable, static_cast<uint16_t>(assocCount));
    img.associationTable.append(assoc);

    // -- Parameter Block ------------------------------------------------
    //
    // Fixed-size block; parameter values are packed at their declared
    // memoryOffset. Little-endian byte order (STM32-native).
    const uint32_t paramSize = manifest.memoryLayout.parameterSize;
    img.parameterBlock = QByteArray(static_cast<int>(paramSize), char(0));

    for (const ManifestParameter &p : manifest.parameters) {
        auto paramIt = device.parameters().find(p.id);
        const QVariant v = (paramIt != device.parameters().end()) ? paramIt->second : p.defaultValue;
        writeU32LE(img.parameterBlock, p.memoryOffset,
                   static_cast<uint32_t>(v.toInt()), p.effectiveSize());
    }

    return img;
}
