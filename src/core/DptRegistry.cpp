#include "DptRegistry.h"

const DptRegistry &DptRegistry::instance()
{
    static DptRegistry reg;
    return reg;
}

DptRegistry::DptRegistry()
{
    m_dpts = {
        {"1.001", "Switch",         "Schalten",           0},
        {"1.002", "Boolean",        "Boolean",            0},
        {"1.008", "Up/Down",        "Hoch/Runter",        0},
        {"1.009", "Open/Close",     "Auf/Zu",             0},
        {"3.007", "Dimming",        "Dimmen",             0},
        {"5.001", "Percentage",     "Prozentwert",        1},
        {"5.010", "Counter",        "Zählerwert",         1},
        {"9.001", "Temperature",    "Temperatur",         2},
        {"9.004", "Illuminance",    "Beleuchtungsstärke", 2},
        {"9.007", "Humidity",       "Feuchte",            2},
        {"9.020", "Voltage",        "Spannung",           2},
        {"14.019","Current",        "Strom",              4},
    };
}

const DptInfo *DptRegistry::find(const QString &id) const
{
    for (const auto &d : m_dpts)
        if (d.id == id) return &d;
    return nullptr;
}

QString DptRegistry::decode(const QString &dptId, const QByteArray &apdu)
{
    if (apdu.isEmpty())
        return QStringLiteral("–");

    // DPT 1.x: 1-bit value packed in lower bit of last APCI byte
    if (dptId.startsWith(QLatin1String("1."))) {
        uint8_t val = static_cast<uint8_t>(apdu.back()) & 0x01;
        if (dptId == QStringLiteral("1.001"))
            return val ? QStringLiteral("EIN") : QStringLiteral("AUS");
        if (dptId == QStringLiteral("1.008"))
            return val ? QStringLiteral("RUNTER") : QStringLiteral("HOCH");
        return val ? QStringLiteral("1") : QStringLiteral("0");
    }

    // DPT 5.x: 1 byte unsigned
    if (dptId.startsWith(QLatin1String("5.")) && apdu.size() >= 1) {
        uint8_t raw = static_cast<uint8_t>(apdu.back());
        if (dptId == QStringLiteral("5.001"))
            return QStringLiteral("%1 %").arg(qRound(raw * 100.0 / 255.0));
        return QString::number(raw);
    }

    // DPT 9.x: 2-byte float (KNX encoding)
    if (dptId.startsWith(QLatin1String("9.")) && apdu.size() >= 2) {
        int offset = apdu.size() - 2;
        uint8_t b0 = static_cast<uint8_t>(apdu[offset]);
        uint8_t b1 = static_cast<uint8_t>(apdu[offset + 1]);
        int sign  = (b0 & 0x80) ? -1 : 1;
        int exp   = (b0 >> 3) & 0x0F;
        int mant  = ((b0 & 0x07) << 8) | b1;
        double v  = 0.01 * sign * mant * (1 << exp);
        const DptInfo *info = instance().find(dptId);
        QString unit;
        if (dptId == QStringLiteral("9.001")) unit = QStringLiteral(" °C");
        else if (dptId == QStringLiteral("9.004")) unit = QStringLiteral(" lx");
        else if (dptId == QStringLiteral("9.007")) unit = QStringLiteral(" %RH");
        return QStringLiteral("%1%2").arg(v, 0, 'f', 1).arg(unit);
    }

    // Fallback: hex dump
    return QLatin1String("0x") + apdu.toHex().toUpper();
}
