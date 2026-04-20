#include "CemiFrame.h"

CemiFrame CemiFrame::fromBytes(const QByteArray &data)
{
    CemiFrame f;
    if (data.size() < 11)
        return f;

    f.messageCode = static_cast<MessageCode>(static_cast<uint8_t>(data[0]));
    // Byte 1: add.info length
    int addLen = static_cast<uint8_t>(data[1]);
    int offset = 2 + addLen;
    if (data.size() < offset + 8)
        return f;

    // ctrl1 byte
    // ctrl2 byte
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

QString CemiFrame::physAddrToString(uint16_t addr)
{
    return QStringLiteral("%1.%2.%3")
        .arg((addr >> 12) & 0x0F)
        .arg((addr >>  8) & 0x0F)
        .arg( addr        & 0xFF);
}

QString CemiFrame::groupAddrToString(uint16_t addr)
{
    return QStringLiteral("%1/%2/%3")
        .arg((addr >> 11) & 0x1F)
        .arg((addr >>  8) & 0x07)
        .arg( addr        & 0xFF);
}
