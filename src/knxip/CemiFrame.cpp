#include "CemiFrame.h"
#include <QStringList>
#include <algorithm>

// APCI codes (KNX spec 03_03_07 "Application Layer", Table 2)
static constexpr uint16_t APCI_GROUP_VALUE_READ            = 0x000;
static constexpr uint16_t APCI_GROUP_VALUE_RESPONSE        = 0x040;
static constexpr uint16_t APCI_GROUP_VALUE_WRITE           = 0x080;
static constexpr uint16_t APCI_INDIVIDUAL_ADDRESS_WRITE    = 0x0C0;
static constexpr uint16_t APCI_INDIVIDUAL_ADDRESS_READ     = 0x100;
static constexpr uint16_t APCI_INDIVIDUAL_ADDRESS_RESPONSE = 0x140;
static constexpr uint16_t APCI_MEMORY_READ                 = 0x200;
static constexpr uint16_t APCI_MEMORY_RESPONSE             = 0x240;
static constexpr uint16_t APCI_MEMORY_WRITE                = 0x280;
static constexpr uint16_t APCI_DEVICE_DESCRIPTOR_READ      = 0x300;
static constexpr uint16_t APCI_DEVICE_DESCRIPTOR_RESPONSE  = 0x340;
static constexpr uint16_t APCI_RESTART                     = 0x380;
static constexpr uint16_t APCI_PROPERTY_VALUE_READ         = 0x3D5;
static constexpr uint16_t APCI_PROPERTY_VALUE_RESPONSE     = 0x3D6;
static constexpr uint16_t APCI_PROPERTY_VALUE_WRITE        = 0x3D7;

// TPCI helpers ---------------------------------------------------------------
//
// TPCI byte layout (KNX spec 03_03_04):
//   Bit 7-6 = TPCI type:
//     00 = T_Data_(Tag_)Group / T_Data_Individual
//     01 = T_Data_Connected (with sequence in bits 5..2)
//     10 = T_Connect (0x80) / T_Disconnect (0x81)
//     11 = T_ACK (xx10) / T_NAK (xx11) (with sequence in bits 5..2)
//   Bits 1-0 = APCI bits 9..8 (only for data frames)

static inline uint8_t makeTpciDataConnected(uint8_t seq, uint8_t apciHi)
{
    return static_cast<uint8_t>(0x40 | ((seq & 0x0F) << 2) | (apciHi & 0x03));
}

// ─── Encoding / decoding ──────────────────────────────────────────────────────

CemiFrame CemiFrame::fromBytes(const QByteArray &data)
{
    CemiFrame f;
    // Minimum: 1(mc)+1(addLen)+1(ctrl1)+1(ctrl2)+2(src)+2(dst)+1(apduLen)+1(apdu) = 10.
    // T_Connect/T_Disconnect/T_ACK/T_NAK all have 1-byte APDUs → exactly 10 bytes.
    if (data.size() < 10)
        return f;

    f.messageCode = static_cast<MessageCode>(static_cast<uint8_t>(data[0]));
    int addLen = static_cast<uint8_t>(data[1]);
    int offset = 2 + addLen;
    if (data.size() < offset + 8)
        return f;

    uint8_t ctrl2 = static_cast<uint8_t>(data[offset + 1]);
    f.groupAddress = (ctrl2 & 0x80) != 0;
    f.hopCount     = (ctrl2 >> 4) & 0x07;

    f.sourceAddress = (static_cast<uint8_t>(data[offset + 2]) << 8)
                    |  static_cast<uint8_t>(data[offset + 3]);
    f.destAddress   = (static_cast<uint8_t>(data[offset + 4]) << 8)
                    |  static_cast<uint8_t>(data[offset + 5]);

    int apduLen = static_cast<uint8_t>(data[offset + 6]);
    if (data.size() >= offset + 7 + apduLen)
        f.apdu = data.mid(offset + 7, apduLen);

    return f;
}

QByteArray CemiFrame::toBytes() const
{
    QByteArray out;
    out.append(static_cast<char>(static_cast<uint8_t>(messageCode)));
    out.append(char(0));                                       // add.info length = 0
    out.append(char(0xBC));                                    // ctrl1
    uint8_t ctrl2 = static_cast<uint8_t>((hopCount & 0x07) << 4) | 0x00;
    if (groupAddress) ctrl2 |= 0x80;
    out.append(static_cast<char>(ctrl2));
    out.append(static_cast<char>(sourceAddress >> 8));
    out.append(static_cast<char>(sourceAddress & 0xFF));
    out.append(static_cast<char>(destAddress >> 8));
    out.append(static_cast<char>(destAddress & 0xFF));
    out.append(static_cast<char>(apdu.size()));
    out.append(apdu);
    return out;
}

// ─── Group / broadcast frame builders ─────────────────────────────────────────

QByteArray CemiFrame::buildGroupValueWrite(uint16_t groupAddr, const QByteArray &value)
{
    CemiFrame f;
    f.messageCode    = MessageCode::LDataReq;
    f.sourceAddress  = 0x0000;
    f.destAddress    = groupAddr;
    f.groupAddress   = true;

    if (value.size() <= 1 && !value.isEmpty()) {
        const uint8_t v = static_cast<uint8_t>(value[0]) & 0x3F;
        f.apdu.append(char(0x00));
        f.apdu.append(static_cast<char>(0x80 | v));
    } else {
        f.apdu.append(char(0x00));
        f.apdu.append(char(0x80));
        f.apdu.append(value);
    }
    return f.toBytes();
}

QByteArray CemiFrame::buildGroupValueRead(uint16_t groupAddr)
{
    CemiFrame f;
    f.messageCode   = MessageCode::LDataReq;
    f.sourceAddress = 0x0000;
    f.destAddress   = groupAddr;
    f.groupAddress  = true;
    f.apdu.append(char(0x00));
    f.apdu.append(char(0x00));
    return f.toBytes();
}

QByteArray CemiFrame::buildIndividualAddressWrite(uint16_t newPhysAddr)
{
    CemiFrame f;
    f.messageCode   = MessageCode::LDataReq;
    f.sourceAddress = 0x0000;
    f.destAddress   = 0x0000;
    f.groupAddress  = true;
    f.apdu.append(char(0x00));
    f.apdu.append(char(0xC0));
    f.apdu.append(static_cast<char>(newPhysAddr >> 8));
    f.apdu.append(static_cast<char>(newPhysAddr & 0xFF));
    return f.toBytes();
}

QByteArray CemiFrame::buildIndividualAddressRead()
{
    CemiFrame f;
    f.messageCode   = MessageCode::LDataReq;
    f.sourceAddress = 0x0000;
    f.destAddress   = 0x0000;            // broadcast
    f.groupAddress  = true;
    // APCI 0x100 = A_IndividualAddress_Read; APCI[9:8]=01, APCI[7:0]=00
    f.apdu.append(char(0x01));
    f.apdu.append(char(0x00));
    return f.toBytes();
}

QByteArray CemiFrame::buildDeviceDescriptorRead(uint16_t physAddr)
{
    CemiFrame f;
    f.messageCode   = MessageCode::LDataReq;
    f.sourceAddress = 0x0000;
    f.destAddress   = physAddr;
    f.groupAddress  = false;
    f.apdu.append(char(0x03));  // unconnected: TPCI=0, APCI[9:8]=11
    f.apdu.append(char(0x00));
    return f.toBytes();
}

// ─── Transport-layer frame builders ──────────────────────────────────────────

QByteArray CemiFrame::buildTConnect(uint16_t destPa)
{
    CemiFrame f;
    f.messageCode   = MessageCode::LDataReq;
    f.sourceAddress = 0x0000;
    f.destAddress   = destPa;
    f.groupAddress  = false;
    f.apdu.append(char(0x80));   // T_Connect
    return f.toBytes();
}

QByteArray CemiFrame::buildTDisconnect(uint16_t destPa)
{
    CemiFrame f;
    f.messageCode   = MessageCode::LDataReq;
    f.sourceAddress = 0x0000;
    f.destAddress   = destPa;
    f.groupAddress  = false;
    f.apdu.append(char(0x81));   // T_Disconnect
    return f.toBytes();
}

QByteArray CemiFrame::buildTAck(uint16_t destPa, uint8_t seqNum)
{
    CemiFrame f;
    f.messageCode   = MessageCode::LDataReq;
    f.sourceAddress = 0x0000;
    f.destAddress   = destPa;
    f.groupAddress  = false;
    // T_ACK = 11_NNNN_10 = 0xC2 | (seq << 2)
    f.apdu.append(static_cast<char>(0xC2 | ((seqNum & 0x0F) << 2)));
    return f.toBytes();
}

QByteArray CemiFrame::buildTNak(uint16_t destPa, uint8_t seqNum)
{
    CemiFrame f;
    f.messageCode   = MessageCode::LDataReq;
    f.sourceAddress = 0x0000;
    f.destAddress   = destPa;
    f.groupAddress  = false;
    // T_NAK = 11_NNNN_11 = 0xC3 | (seq << 2)
    f.apdu.append(static_cast<char>(0xC3 | ((seqNum & 0x0F) << 2)));
    return f.toBytes();
}

// ─── Connection-oriented application frames ─────────────────────────────────

QByteArray CemiFrame::buildMemoryWrite(uint16_t destPhysAddr,
                                       uint16_t memoryAddress,
                                       const QByteArray &data,
                                       uint8_t seqNum)
{
    CemiFrame f;
    f.messageCode   = MessageCode::LDataReq;
    f.sourceAddress = 0x0000;
    f.destAddress   = destPhysAddr;
    f.groupAddress  = false;

    const uint8_t count = static_cast<uint8_t>(std::min<qsizetype>(data.size(), 63));

    // TPCI = T_Data_Connected[seq] | APCI[9:8]=10
    f.apdu.append(static_cast<char>(makeTpciDataConnected(seqNum, 0x02)));
    // APCI[7:0] = A_Memory_Write low byte (0x80) OR'd with 6-bit count
    f.apdu.append(static_cast<char>(0x80 | (count & 0x3F)));
    f.apdu.append(static_cast<char>(memoryAddress >> 8));
    f.apdu.append(static_cast<char>(memoryAddress & 0xFF));
    f.apdu.append(data.left(count));
    return f.toBytes();
}

QByteArray CemiFrame::buildMemoryRead(uint16_t destPhysAddr,
                                       uint16_t memAddr,
                                       uint8_t count,
                                       uint8_t seqNum)
{
    CemiFrame f;
    f.messageCode   = MessageCode::LDataReq;
    f.sourceAddress = 0x0000;
    f.destAddress   = destPhysAddr;
    f.groupAddress  = false;

    const uint8_t cnt = (count == 0 || count > 63) ? 63 : count;
    f.apdu.append(static_cast<char>(makeTpciDataConnected(seqNum, 0x02)));
    f.apdu.append(static_cast<char>(cnt & 0x3F));   // A_Memory_Read APCI[7:0] = count
    f.apdu.append(static_cast<char>(memAddr >> 8));
    f.apdu.append(static_cast<char>(memAddr & 0xFF));
    return f.toBytes();
}

QByteArray CemiFrame::buildRestart(uint16_t destPhysAddr, uint8_t seqNum)
{
    CemiFrame f;
    f.messageCode   = MessageCode::LDataReq;
    f.sourceAddress = 0x0000;
    f.destAddress   = destPhysAddr;
    f.groupAddress  = false;
    f.apdu.append(static_cast<char>(makeTpciDataConnected(seqNum, 0x03)));   // APCI[9:8]=11
    f.apdu.append(char(0x80));   // APCI[7:0] = 0x80 → APCI = 0x380 = A_Restart
    return f.toBytes();
}

QByteArray CemiFrame::buildPropertyValueRead(uint16_t destPhysAddr,
                                              uint8_t  objIndex,
                                              uint8_t  propId,
                                              uint8_t  count,
                                              uint16_t startIndex,
                                              uint8_t  seqNum)
{
    CemiFrame f;
    f.messageCode   = MessageCode::LDataReq;
    f.sourceAddress = 0x0000;
    f.destAddress   = destPhysAddr;
    f.groupAddress  = false;

    // APCI 0x3D5 → APCI[9:8]=11, APCI[7:0]=0xD5
    f.apdu.append(static_cast<char>(makeTpciDataConnected(seqNum, 0x03)));
    f.apdu.append(char(0xD5));
    f.apdu.append(static_cast<char>(objIndex));
    f.apdu.append(static_cast<char>(propId));
    // 4-bit count + 12-bit start index
    f.apdu.append(static_cast<char>(((count & 0x0F) << 4) | ((startIndex >> 8) & 0x0F)));
    f.apdu.append(static_cast<char>(startIndex & 0xFF));
    return f.toBytes();
}

QByteArray CemiFrame::buildPropertyValueWrite(uint16_t destPhysAddr,
                                               uint8_t  objIndex,
                                               uint8_t  propId,
                                               uint8_t  count,
                                               uint16_t startIndex,
                                               const QByteArray &data,
                                               uint8_t  seqNum)
{
    CemiFrame f;
    f.messageCode   = MessageCode::LDataReq;
    f.sourceAddress = 0x0000;
    f.destAddress   = destPhysAddr;
    f.groupAddress  = false;

    // APCI 0x3D7 → APCI[9:8]=11, APCI[7:0]=0xD7
    f.apdu.append(static_cast<char>(makeTpciDataConnected(seqNum, 0x03)));
    f.apdu.append(char(0xD7));
    f.apdu.append(static_cast<char>(objIndex));
    f.apdu.append(static_cast<char>(propId));
    f.apdu.append(static_cast<char>(((count & 0x0F) << 4) | ((startIndex >> 8) & 0x0F)));
    f.apdu.append(static_cast<char>(startIndex & 0xFF));
    f.apdu.append(data);
    return f.toBytes();
}

// ─── APDU inspection ─────────────────────────────────────────────────────────

uint16_t CemiFrame::apci() const
{
    if (apdu.size() < 2)
        return 0;
    const uint8_t b0 = static_cast<uint8_t>(apdu[0]);
    const uint8_t b1 = static_cast<uint8_t>(apdu[1]);
    return static_cast<uint16_t>(((b0 & 0x03) << 8) | b1);
}

bool CemiFrame::isGroupValueWrite() const
{
    return (apci() & 0x3C0) == APCI_GROUP_VALUE_WRITE;
}

bool CemiFrame::isGroupValueResponse() const
{
    return (apci() & 0x3C0) == APCI_GROUP_VALUE_RESPONSE;
}

bool CemiFrame::isDeviceDescriptorResponse() const
{
    if (apdu.size() < 2) return false;
    return (apci() & 0x3FC) == APCI_DEVICE_DESCRIPTOR_RESPONSE;
}

bool CemiFrame::isMemoryResponse() const
{
    if (apdu.size() < 4 || groupAddress) return false;
    // A_Memory_Response APCI = 0x240 | count → bits 9:6 = 1001
    return (apci() & 0x3C0) == APCI_MEMORY_RESPONSE;
}

bool CemiFrame::isPropertyValueResponse() const
{
    if (apdu.size() < 6 || groupAddress) return false;
    return apci() == APCI_PROPERTY_VALUE_RESPONSE;
}

bool CemiFrame::isIndividualAddressResponse() const
{
    if (apdu.size() < 2 || !groupAddress) return false;
    return apci() == APCI_INDIVIDUAL_ADDRESS_RESPONSE;
}

bool CemiFrame::isTConnect() const
{
    return apdu.size() >= 1 && static_cast<uint8_t>(apdu[0]) == 0x80;
}

bool CemiFrame::isTDisconnect() const
{
    return apdu.size() >= 1 && static_cast<uint8_t>(apdu[0]) == 0x81;
}

bool CemiFrame::isTAck() const
{
    if (apdu.size() < 1) return false;
    const uint8_t b = static_cast<uint8_t>(apdu[0]);
    // 11_NNNN_10
    return (b & 0xC3) == 0xC2;
}

bool CemiFrame::isTNak() const
{
    if (apdu.size() < 1) return false;
    const uint8_t b = static_cast<uint8_t>(apdu[0]);
    // 11_NNNN_11
    return (b & 0xC3) == 0xC3;
}

bool CemiFrame::isTDataConnected() const
{
    if (apdu.size() < 1) return false;
    const uint8_t b = static_cast<uint8_t>(apdu[0]);
    // 01_NNNN_xx
    return (b & 0xC0) == 0x40;
}

uint8_t CemiFrame::tSeqNumber() const
{
    if (apdu.size() < 1) return 0;
    return static_cast<uint8_t>((static_cast<uint8_t>(apdu[0]) >> 2) & 0x0F);
}

bool CemiFrame::memoryResponseData(uint16_t &addr, QByteArray &data) const
{
    if (!isMemoryResponse() || apdu.size() < 4) return false;
    const uint8_t count = static_cast<uint8_t>(apdu[1]) & 0x3F;
    addr = (static_cast<uint8_t>(apdu[2]) << 8) | static_cast<uint8_t>(apdu[3]);
    data = apdu.mid(4, count);
    return !data.isEmpty();
}

bool CemiFrame::propertyValueResponseData(uint8_t &objIndex, uint8_t &propId,
                                            uint8_t &count, uint16_t &startIndex,
                                            QByteArray &data) const
{
    if (!isPropertyValueResponse() || apdu.size() < 6) return false;
    objIndex   = static_cast<uint8_t>(apdu[2]);
    propId     = static_cast<uint8_t>(apdu[3]);
    count      = static_cast<uint8_t>((apdu[4] >> 4) & 0x0F);
    startIndex = static_cast<uint16_t>(((apdu[4] & 0x0F) << 8) | static_cast<uint8_t>(apdu[5]));
    data = apdu.mid(6);
    return true;
}

QByteArray CemiFrame::groupValuePayload() const
{
    if (apdu.size() < 2)
        return {};
    if (apdu.size() == 2)
        return QByteArray(1, static_cast<char>(apdu[1] & 0x3F));
    return apdu.mid(2);
}

// ─── Address conversion ──────────────────────────────────────────────────────

QString CemiFrame::physAddrToString(uint16_t addr)
{
    return QStringLiteral("%1.%2.%3")
        .arg((addr >> 12) & 0x0F)
        .arg((addr >>  8) & 0x0F)
        .arg( addr        & 0xFF);
}

uint16_t CemiFrame::physAddrFromString(const QString &s)
{
    const QStringList parts = s.split('.');
    if (parts.size() != 3) return 0;
    const uint16_t a = parts[0].toUShort() & 0x0F;
    const uint16_t l = parts[1].toUShort() & 0x0F;
    const uint16_t m = parts[2].toUShort() & 0xFF;
    return static_cast<uint16_t>((a << 12) | (l << 8) | m);
}

QString CemiFrame::groupAddrToString(uint16_t addr)
{
    return QStringLiteral("%1/%2/%3")
        .arg((addr >> 11) & 0x1F)
        .arg((addr >>  8) & 0x07)
        .arg( addr        & 0xFF);
}

uint16_t CemiFrame::groupAddrFromString(const QString &s)
{
    const QStringList parts = s.split('/');
    if (parts.size() != 3) return 0;
    const uint16_t main_   = parts[0].toUShort() & 0x1F;
    const uint16_t middle_ = parts[1].toUShort() & 0x07;
    const uint16_t sub_    = parts[2].toUShort() & 0xFF;
    return static_cast<uint16_t>((main_ << 11) | (middle_ << 8) | sub_);
}
