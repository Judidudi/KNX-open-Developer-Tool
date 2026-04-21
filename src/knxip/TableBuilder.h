#pragma once

#include "../core/Manifest.h"
#include <QByteArray>
#include <QList>
#include <cstdint>

class DeviceInstance;

// Serializes a DeviceInstance into the three raw memory blocks that get
// written to the device via A_Memory_Write during programming:
//   • addressTable       – device PA followed by used GA list
//   • associationTable   – entries mapping ComObject index → GA table index
//   • parameterBlock     – packed parameter values (byte-exact per manifest)
//
// Layout conventions (matched by the firmware stack in Repo B):
//   addressTable:     [count_hi][count_lo] [PA_hi][PA_lo] [GA1_hi][GA1_lo] ...
//   associationTable: [count_hi][count_lo] { [ga_idx][co_number] } ...
//   parameterBlock:   raw bytes, length = memoryLayout.parameterSize
struct DeviceMemoryImage
{
    QByteArray addressTable;
    QByteArray associationTable;
    QByteArray parameterBlock;
};

class TableBuilder
{
public:
    static DeviceMemoryImage build(const DeviceInstance &device,
                                   const Manifest       &manifest);
};
