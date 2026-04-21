#pragma once

#include <QAbstractTableModel>
#include <QDateTime>
#include <QByteArray>
#include <QString>
#include <QList>

// Table model for the bus monitor: each row is one observed CEMI telegram.
// Columns: Time, Source, Destination, Type, Value, Raw bytes.
class BusMonitorModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        ColTime,
        ColSource,
        ColDestination,
        ColType,
        ColValue,
        ColRaw,
        ColCount,
    };

    struct Entry {
        QDateTime  timestamp;
        QString    source;
        QString    destination;
        QString    type;
        QString    value;
        QString    raw;
    };

    explicit BusMonitorModel(QObject *parent = nullptr);

    void appendCemi(const QByteArray &cemi);
    void clear();

    const Entry &entryAt(int row) const { return m_entries.at(row); }

    // QAbstractTableModel
    int      rowCount(const QModelIndex &parent = {}) const override;
    int      columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    QList<Entry> m_entries;
};
