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

    // Builds a full L_Data_Req CEMI frame for group-value writing.
    static QByteArray buildGroupValueWrite(uint16_t groupAddr, const QByteArray &value);
    // Build a broadcast / programming-mode frame for A_IndividualAddress_Write.
    // Sent to group address 0x0000 in broadcast mode. The new physical address is the payload.
    static QByteArray buildIndividualAddressWrite(uint16_t newPhysAddr);
    // Build an A_Memory_Write point-to-point frame.
    // NOTE: real point-to-point transport requires a T_Connect handshake; the
    // frame we produce here uses a numbered data telegram (sequence = 0).
    static QByteArray buildMemoryWrite(uint16_t destPhysAddr,
                                       uint16_t memoryAddress,
                                       const QByteArray &data);
    static QByteArray buildRestart(uint16_t destPhysAddr);

    // APDU extraction helpers (APCI = upper 4 bits of APDU[0] + upper 6 bits of APDU[1])
    uint16_t apci() const;
    bool     isGroupValueWrite() const;
    QByteArray groupValuePayload() const;

    // Human-readable KNX physical address (e.g. "1.1.1")
    static QString  physAddrToString(uint16_t addr);
    static uint16_t physAddrFromString(const QString &s);
    // Human-readable group address (e.g. "0/0/1")
    static QString  groupAddrToString(uint16_t addr);
    static uint16_t groupAddrFromString(const QString &s);
};
