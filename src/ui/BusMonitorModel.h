#pragma once

#include <QAbstractTableModel>
#include <QDateTime>
#include <QByteArray>
#include <QString>
#include <QList>
#include <QMap>

class Project;

// Table model for the bus monitor: each row is one observed CEMI telegram.
// Columns: Time, Source, Destination, Type, Value, Raw bytes.
// When a Project is set, GroupValue telegrams targeting a known GA are decoded
// using the GA's DPT (DPT-aware value display).
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
        bool       isGroupTelegram = false;
    };

    explicit BusMonitorModel(QObject *parent = nullptr);

    void setProject(Project *project);
    void appendCemi(const QByteArray &cemi);
    void clear();

    void setMaxEntries(int max) { m_maxEntries = qMax(100, max); }
    int  maxEntries() const     { return m_maxEntries; }

    const Entry &entryAt(int row) const { return m_entries.at(row); }

    // QAbstractTableModel
    int      rowCount(const QModelIndex &parent = {}) const override;
    int      columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    QString dptForGa(uint16_t raw) const;

    QList<Entry>           m_entries;
    QMap<uint16_t, QString> m_gaDptMap;   // raw GA address → DPT string
    int                    m_maxEntries = 2000;
};
