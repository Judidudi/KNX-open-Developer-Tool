#pragma once

#include <QString>
#include <QByteArray>
#include <QVariant>
#include <QList>

struct DptInfo {
    QString id;          // e.g. "1.001"
    QString name;        // e.g. "Switch"
    QString nameDe;      // e.g. "Schalten"
    int     sizeBytes;   // payload bytes (0 = 1-bit packed in APCI)
};

// Static registry of known DPTs. Covers the most common types needed for
// the MVP. Extended later.
class DptRegistry
{
public:
    static const DptRegistry &instance();

    const DptInfo *find(const QString &id) const;
    QList<DptInfo> all() const { return m_dpts; }

    // Decode APDU payload to human-readable string
    static QString decode(const QString &dptId, const QByteArray &apdu);

private:
    DptRegistry();
    QList<DptInfo> m_dpts;
};
