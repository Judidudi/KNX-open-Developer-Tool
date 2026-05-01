#include "CemiFrame.h"
#include <QStringList>
#include <algorithm>

// APCI codes (KNX spec 03_03_07 "Application Layer", Table 2)
static constexpr uint16_t APCI_GROUP_VALUE_READ           = 0x000;
static constexpr uint16_t APCI_GROUP_VALUE_RESPONSE       = 0x040;
static constexpr uint16_t APCI_GROUP_VALUE_WRITE          = 0x080;
static constexpr uint16_t APCI_INDIVIDUAL_ADDRESS_WRITE   = 0x0C0;
static constexpr uint16_t APCI_MEMORY_WRITE               = 0x280;
static constexpr uint16_t APCI_DEVICE_DESCRIPTOR_READ     = 0x300;
static constexpr uint16_t APCI_DEVICE_DESCRIPTOR_RESPONSE = 0x340;
static constexpr uint16_t APCI_RESTART                    = 0x380;

// ---------- raw CEMI encoding / decoding -----------------------------------

CemiFrame CemiFrame::fromBytes(const QByteArray &data)
{
    CemiFrame f;
    if (data.size() < 11)
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

// ---------- high-level frame builders --------------------------------------

QByteArray CemiFrame::buildGroupValueWrite(uint16_t groupAddr, const QByteArray &value)
{
    CemiFrame f;
    f.messageCode    = MessageCode::LDataReq;
    f.sourceAddress  = 0x0000;
    f.destAddress    = groupAddr;
    f.groupAddress   = true;

    if (value.size() <= 1 && !value.isEmpty()) {
        // 6-bit payload inline in APCI low byte
        const uint8_t v = static_cast<uint8_t>(value[0]) & 0x3F;
        f.apdu.append(char(0x00));                                   // TPCI = T_Data_Group
        f.apdu.append(static_cast<char>(0x80 | v));                  // APCI GroupValueWrite + data
    } else {
        // Multi-byte payload: APCI in byte 1, data follows
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
    // APCI 0x000 = GroupValue_Read, 2 zero bytes, no payload
    f.apdu.append(char(0x00));
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
    // APCI 0x300 = DeviceDescriptor_Read, descriptor type 0
    f.apdu.append(char(0x03));  // TPCI=0, APCI bits 9-8 = 3
    f.apdu.append(char(0x00));  // APCI bits 7-0 = 0 (type 0)
    return f.toBytes();
}

QByteArray CemiFrame::buildIndividualAddressWrite(uint16_t newPhysAddr)
{
    CemiFrame f;
    f.messageCode   = MessageCode::LDataReq;
    f.sourceAddress = 0x0000;
    f.destAddress   = 0x0000;           // broadcast to devices in programming mode
    f.groupAddress  = true;             // broadcast uses the group-address bit set

    // APCI 0x0C0: A_IndividualAddress_Write, followed by 2 bytes of new PA
    f.apdu.append(char(0x00));                                       // TPCI = 0
    f.apdu.append(char(0xC0));                                       // APCI high byte (bits 7..6 = 11)
    f.apdu.append(static_cast<char>(newPhysAddr >> 8));
    f.apdu.append(static_cast<char>(newPhysAddr & 0xFF));
    return f.toBytes();
}

QByteArray CemiFrame::buildMemoryWrite(uint16_t destPhysAddr,
                                       uint16_t memoryAddress,
                                       const QByteArray &data)
{
    CemiFrame f;
    f.messageCode   = MessageCode::LDataReq;
    f.sourceAddress = 0x0000;
    f.destAddress   = destPhysAddr;
    f.groupAddress  = false;

    const uint8_t count = static_cast<uint8_t>(std::min<qsizetype>(data.size(), 63));

    // APCI A_Memory_Write = 0x0280..0x02BF with low 6 bits = byte count
    // Byte 0: TPCI(0x42 = T_Data_Connected seq=0) | APCI[9:8]=0b10 -> 0x42|0x02 = 0x42
    //   For unnumbered data use TPCI=0x00; firmware in programming mode typically
    //   accepts both.
    f.apdu.append(char(0x42));                                       // T_Data_Connected, seq=0
    f.apdu.append(static_cast<char>(0x80 | (count & 0x3F)));         // APCI A_Memory_Write | count
    f.apdu.append(static_cast<char>(memoryAddress >> 8));
    f.apdu.append(static_cast<char>(memoryAddress & 0xFF));
    f.apdu.append(data.left(count));
    return f.toBytes();
}

QByteArray CemiFrame::buildRestart(uint16_t destPhysAddr)
{
    CemiFrame f;
    f.messageCode   = MessageCode::LDataReq;
    f.sourceAddress = 0x0000;
    f.destAddress   = destPhysAddr;
    f.groupAddress  = false;

    // APCI 0x380: A_Restart (basic restart, no parameters)
    f.apdu.append(char(0x42));
    f.apdu.append(char(0x80));
    return f.toBytes();
}

QByteArray CemiFrame::buildMemoryRead(uint16_t destPhysAddr, uint16_t memAddr, uint8_t count)
{
    CemiFrame f;
    f.messageCode   = MessageCode::LDataReq;
    f.sourceAddress = 0x0000;
    f.destAddress   = destPhysAddr;
    f.groupAddress  = false;

    const uint8_t cnt = (count == 0 || count > 63) ? 63 : count;
    // APCI A_Memory_Read = 0x0200 | count
    // Byte 0: 0x42 = T_Data_Connected(seq=0) | APCI[9:8]=0b10
    // Byte 1: 0x00 | cnt
    f.apdu.append(char(0x42));
    f.apdu.append(static_cast<char>(cnt & 0x3F));   // APCI[7:0] = count (no high bits set)
    f.apdu.append(static_cast<char>(memAddr >> 8));
    f.apdu.append(static_cast<char>(memAddr & 0xFF));
    return f.toBytes();
}

// ---------- APDU inspection ------------------------------------------------

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
    const uint16_t a = apci();
    return (a & 0x3C0) == APCI_GROUP_VALUE_WRITE;
}

bool CemiFrame::isGroupValueResponse() const
{
    const uint16_t a = apci();
    return (a & 0x3C0) == APCI_GROUP_VALUE_RESPONSE;
}

bool CemiFrame::isDeviceDescriptorResponse() const
{
    if (apdu.size() < 4) return false;
    return (apci() & 0x3FC) == APCI_DEVICE_DESCRIPTOR_RESPONSE;
}

bool CemiFrame::isMemoryResponse() const
{
    if (apdu.size() < 4 || groupAddress) return false;
    // A_Memory_Response APCI = 0x240 | count (bits 9:6 = 0b1001)
    return (apci() & 0x3C0) == 0x240;
}

bool CemiFrame::memoryResponseData(uint16_t &addr, QByteArray &data) const
{
    if (!isMemoryResponse() || apdu.size() < 4) return false;
    const uint8_t count = static_cast<uint8_t>(apdu[1]) & 0x3F;
    addr = (static_cast<uint8_t>(apdu[2]) << 8) | static_cast<uint8_t>(apdu[3]);
    data = apdu.mid(4, count);
    return !data.isEmpty();
}

QByteArray CemiFrame::groupValuePayload() const
{
    if (apdu.size() < 2)
        return {};
    // Payload rules (KNX TP1):
    //  • 1..6 bit value  → packed into APCI byte's lower 6 bits, APDU length = 1
    //  • ≥ 1 byte value → APCI byte has 00 in lower 6 bits, data in APDU bytes 2..N
    if (apdu.size() == 2)
        return QByteArray(1, static_cast<char>(apdu[1] & 0x3F));
    return apdu.mid(2);
}

// ---------- address conversion ---------------------------------------------

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
