#pragma once

#include <QByteArray>
#include <QString>
#include <cstdint>

// Minimal CEMI L_Data frame representation.
// KNX spec: 03_06_03 EMI/IMI section 4.
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

    // ── High-level frame builders (Application Layer) ────────────────────────

    // Group-oriented (broadcast or multicast)
    static QByteArray buildGroupValueWrite(uint16_t groupAddr, const QByteArray &value);
    static QByteArray buildGroupValueRead(uint16_t groupAddr);

    // Broadcast: A_IndividualAddress_Write (program new PA into prog-mode device)
    static QByteArray buildIndividualAddressWrite(uint16_t newPhysAddr);
    // Broadcast: A_IndividualAddress_Read (asks all prog-mode devices to respond)
    static QByteArray buildIndividualAddressRead();

    // Connection-less point-to-point
    static QByteArray buildDeviceDescriptorRead(uint16_t physAddr);

    // ── Transport Layer (KNX spec 03_03_04) ──────────────────────────────────

    // T_Connect: opens a transport-layer connection to destPhysAddr.
    static QByteArray buildTConnect(uint16_t destPhysAddr);
    // T_Disconnect: closes the connection.
    static QByteArray buildTDisconnect(uint16_t destPhysAddr);
    // T_ACK[seqNum]: acknowledges a received T_Data_Connected.
    static QByteArray buildTAck(uint16_t destPhysAddr, uint8_t seqNum);
    // T_NAK[seqNum]: negative acknowledgment (used to request retransmit).
    static QByteArray buildTNak(uint16_t destPhysAddr, uint8_t seqNum);

    // ── Connection-oriented point-to-point (use within an open T_Connect) ───

    // A_Memory_Write at memoryAddress (data ≤ 12 bytes per KNX spec); seqNum = 0..15
    static QByteArray buildMemoryWrite(uint16_t destPhysAddr,
                                       uint16_t memoryAddress,
                                       const QByteArray &data,
                                       uint8_t seqNum = 0);
    // A_Memory_Read of `count` bytes at memAddr; seqNum = 0..15
    static QByteArray buildMemoryRead(uint16_t destPhysAddr,
                                      uint16_t memAddr,
                                      uint8_t count,
                                      uint8_t seqNum = 0);

    // A_Restart (basic).
    static QByteArray buildRestart(uint16_t destPhysAddr, uint8_t seqNum = 0);

    // A_PropertyValue_Read (Interface Object access). objIndex selects which
    // Application Object (0=AddrTable, 1=AssocTable, 2=AppProgram, 3=BAU).
    // propId 5 = LoadStateControl. count = number of elements.
    static QByteArray buildPropertyValueRead(uint16_t destPhysAddr,
                                             uint8_t  objIndex,
                                             uint8_t  propId,
                                             uint8_t  count,
                                             uint16_t startIndex,
                                             uint8_t  seqNum = 0);
    static QByteArray buildPropertyValueWrite(uint16_t destPhysAddr,
                                              uint8_t  objIndex,
                                              uint8_t  propId,
                                              uint8_t  count,
                                              uint16_t startIndex,
                                              const QByteArray &data,
                                              uint8_t  seqNum = 0);

    // ── APDU inspection helpers ──────────────────────────────────────────────

    // 10-bit APCI (extracted from APDU bytes 0..1).
    uint16_t apci() const;

    bool isGroupValueWrite() const;
    bool isGroupValueResponse() const;
    bool isDeviceDescriptorResponse() const;
    bool isMemoryResponse() const;
    bool isPropertyValueResponse() const;
    bool isIndividualAddressResponse() const;

    // Transport-layer inspection
    bool    isTConnect() const;
    bool    isTDisconnect() const;
    bool    isTAck() const;
    bool    isTNak() const;
    bool    isTDataConnected() const;
    uint8_t tSeqNumber() const;   // 0..15, valid for T_Data_Connected / T_ACK / T_NAK

    // Decoded payloads
    QByteArray groupValuePayload() const;
    bool       memoryResponseData(uint16_t &addr, QByteArray &data) const;
    bool       propertyValueResponseData(uint8_t &objIndex, uint8_t &propId,
                                          uint8_t &count, uint16_t &startIndex,
                                          QByteArray &data) const;

    // ── Address conversion ───────────────────────────────────────────────────

    static QString  physAddrToString(uint16_t addr);
    static uint16_t physAddrFromString(const QString &s);
    static QString  groupAddrToString(uint16_t addr);
    static uint16_t groupAddrFromString(const QString &s);
};
