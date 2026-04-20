#include "GroupAddress.h"
#include <QStringList>

GroupAddress::GroupAddress(int main, int middle, int sub,
                           const QString &name, const QString &dpt)
    : m_main(main), m_middle(middle), m_sub(sub), m_name(name), m_dpt(dpt)
{}

GroupAddress GroupAddress::fromString(const QString &s)
{
    const QStringList parts = s.split(QLatin1Char('/'));
    if (parts.size() != 3)
        return {};
    bool ok1, ok2, ok3;
    int  main   = parts[0].toInt(&ok1);
    int  middle = parts[1].toInt(&ok2);
    int  sub    = parts[2].toInt(&ok3);
    if (!ok1 || !ok2 || !ok3)
        return {};
    return {main, middle, sub};
}

QString GroupAddress::toString() const
{
    return QStringLiteral("%1/%2/%3").arg(m_main).arg(m_middle).arg(m_sub);
}

uint16_t GroupAddress::toRaw() const
{
    return static_cast<uint16_t>(
        ((m_main & 0x1F) << 11) | ((m_middle & 0x07) << 8) | (m_sub & 0xFF));
}

GroupAddress GroupAddress::fromRaw(uint16_t raw)
{
    return {(raw >> 11) & 0x1F, (raw >> 8) & 0x07, raw & 0xFF};
}

bool GroupAddress::isValid() const
{
    return m_main   >= 0 && m_main   <= 31
        && m_middle >= 0 && m_middle <= 7
        && m_sub    >= 0 && m_sub    <= 255;
}

bool GroupAddress::operator==(const GroupAddress &o) const
{
    return m_main == o.m_main && m_middle == o.m_middle && m_sub == o.m_sub;
}
