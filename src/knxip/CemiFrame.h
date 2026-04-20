#pragma once

#include <QByteArray>
#include <QString>
#include <cstdint>

// Minimal CEMI L_Data frame representation.
// KNX spec: 03_06_03 EMI_IMI, section 4.
struct CemiFrame
{
    enum class MessageCode : uint8_t {
        LDataReq = 0x11,
        LDataCon = 0x2E,
        LDataInd = 0x29,
    };

    MessageCode  messageCode = MessageCode::LDataInd;
    uint16_t     sourceAddress   = 0;
    uint16_t     destAddress     = 0;
    bool         groupAddress    = true;
    uint8_t      hopCount        = 6;
    QByteArray   apdu;

    static CemiFrame fromBytes(const QByteArray &data);
    QByteArray       toBytes() const;

    // Human-readable KNX physical address (e.g. "1.1.1")
    static QString physAddrToString(uint16_t addr);
    // Human-readable group address (e.g. "0/0/1")
    static QString groupAddrToString(uint16_t addr);
};
