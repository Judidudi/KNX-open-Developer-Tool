#include "BusMonitorModel.h"
#include "CemiFrame.h"

BusMonitorModel::BusMonitorModel(QObject *parent)
    : QAbstractTableModel(parent)
{}

void BusMonitorModel::appendCemi(const QByteArray &cemi)
{
    const CemiFrame frame = CemiFrame::fromBytes(cemi);

    Entry e;
    e.timestamp   = QDateTime::currentDateTime();
    e.source      = CemiFrame::physAddrToString(frame.sourceAddress);
    e.destination = frame.groupAddress
                        ? CemiFrame::groupAddrToString(frame.destAddress)
                        : CemiFrame::physAddrToString(frame.destAddress);

    if (frame.isGroupValueWrite()) {
        e.type  = tr("GroupValueWrite");
        const QByteArray payload = frame.groupValuePayload();
        if (payload.size() == 1)
            e.value = QString::number(static_cast<uint8_t>(payload[0]));
        else
            e.value = QString::fromLatin1(payload.toHex(' ').toUpper());
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
    if (!index.isValid() || role != Qt::DisplayRole)
        return {};

    const Entry &e = m_entries.at(index.row());
    switch (index.column()) {
    case ColTime:        return e.timestamp.toString(QStringLiteral("HH:mm:ss.zzz"));
    case ColSource:      return e.source;
    case ColDestination: return e.destination;
    case ColType:        return e.type;
    case ColValue:       return e.value;
    case ColRaw:         return e.raw;
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
    case ColValue:       return tr("Wert");
    case ColRaw:         return tr("Rohdaten");
    }
    return {};
}
