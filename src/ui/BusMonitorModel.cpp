#include "BusMonitorModel.h"
#include "CemiFrame.h"
#include "DptRegistry.h"
#include "Project.h"
#include "GroupAddress.h"
#include <QColor>

BusMonitorModel::BusMonitorModel(QObject *parent)
    : QAbstractTableModel(parent)
{}

void BusMonitorModel::setProject(Project *project)
{
    m_gaDptMap.clear();
    if (!project) return;
    for (const GroupAddress &ga : project->groupAddresses()) {
        if (!ga.dpt().isEmpty())
            m_gaDptMap.insert(ga.toRaw(), ga.dpt());
    }
}

QString BusMonitorModel::dptForGa(uint16_t raw) const
{
    return m_gaDptMap.value(raw);
}

void BusMonitorModel::appendCemi(const QByteArray &cemi)
{
    const CemiFrame frame = CemiFrame::fromBytes(cemi);

    Entry e;
    e.timestamp   = QDateTime::currentDateTime();
    e.source      = CemiFrame::physAddrToString(frame.sourceAddress);
    e.destination = frame.groupAddress
                        ? CemiFrame::groupAddrToString(frame.destAddress)
                        : CemiFrame::physAddrToString(frame.destAddress);
    e.isGroupTelegram = frame.groupAddress;

    const bool isWrite    = frame.isGroupValueWrite();
    const bool isResponse = frame.isGroupValueResponse();

    if (isWrite || isResponse) {
        e.type = isWrite ? tr("GroupValueWrite") : tr("GroupValueResponse");

        // Try DPT-aware decoding for known group addresses
        const QString dpt = frame.groupAddress ? dptForGa(frame.destAddress) : QString();
        if (!dpt.isEmpty()) {
            // Build a minimal apdu suitable for DptRegistry::decode:
            // GroupValue payload follows APCI; DptRegistry expects APDU with payload
            const QByteArray payload = frame.groupValuePayload();
            const QString decoded    = DptRegistry::decode(dpt, payload);
            e.value = decoded + QStringLiteral("  [") + dpt + QLatin1Char(']');
        } else {
            const QByteArray payload = frame.groupValuePayload();
            if (!payload.isEmpty())
                e.value = QString::fromLatin1(payload.toHex(' ').toUpper());
        }
    } else if (frame.groupAddress && (frame.apci() & 0x3FC) == 0x000) {
        e.type  = tr("GroupValueRead");
        e.value = QStringLiteral("?");
    } else {
        e.type  = tr("APCI 0x%1").arg(frame.apci(), 3, 16, QLatin1Char('0')).toUpper();
        e.value = QString::fromLatin1(frame.apdu.toHex(' ').toUpper());
    }
    e.raw = QString::fromLatin1(cemi.toHex(' ').toUpper());

    beginInsertRows({}, m_entries.size(), m_entries.size());
    m_entries.append(e);
    endInsertRows();
}

void BusMonitorModel::clear()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

int BusMonitorModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

int BusMonitorModel::columnCount(const QModelIndex &) const
{
    return ColCount;
}

QVariant BusMonitorModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return {};
    const Entry &e = m_entries.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColTime:        return e.timestamp.toString(QStringLiteral("HH:mm:ss.zzz"));
        case ColSource:      return e.source;
        case ColDestination: return e.destination;
        case ColType:        return e.type;
        case ColValue:       return e.value;
        case ColRaw:         return e.raw;
        }
    }
    // Highlight group telegrams in a slightly different background
    if (role == Qt::ForegroundRole && e.isGroupTelegram) {
        if (e.type.startsWith(QLatin1String("GroupValueWrite")))
            return QColor(0x2E, 0x7D, 0x32);  // dark green
        if (e.type.startsWith(QLatin1String("GroupValueResponse")))
            return QColor(0x01, 0x57, 0x9B);  // dark blue
    }
    return {};
}

QVariant BusMonitorModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case ColTime:        return tr("Zeit");
    case ColSource:      return tr("Quelle");
    case ColDestination: return tr("Ziel");
    case ColType:        return tr("Typ");
    case ColValue:       return tr("Wert / DPT");
    case ColRaw:         return tr("Rohdaten");
    }
    return {};
}
