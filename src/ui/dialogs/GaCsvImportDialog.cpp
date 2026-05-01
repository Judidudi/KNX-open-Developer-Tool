#include "GaCsvImportDialog.h"
#include "GroupAddress.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

// Split a single CSV line respecting quoted fields (semicolon-separated).
static QStringList splitCsvLine(const QString &line)
{
    QStringList fields;
    QString cur;
    bool inQuote = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line[i];
        if (c == QLatin1Char('"')) {
            if (inQuote && i + 1 < line.size() && line[i + 1] == QLatin1Char('"')) {
                cur += QLatin1Char('"');
                ++i;
            } else {
                inQuote = !inQuote;
            }
        } else if (c == QLatin1Char(';') && !inQuote) {
            fields.append(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    fields.append(cur);
    return fields;
}

// Strip surrounding quotes and trim whitespace.
static QString clean(const QString &s)
{
    QString t = s.trimmed();
    if (t.size() >= 2 && t.front() == QLatin1Char('"') && t.back() == QLatin1Char('"'))
        t = t.mid(1, t.size() - 2);
    return t.trimmed();
}

// Normalise DPT: "DPT-1" → "1", "DPT-5-1" → "5.001", "5.001" unchanged, "" → "".
static QString normalizeDpt(const QString &raw)
{
    if (raw.isEmpty()) return {};
    // ETS exports "DPT-main" or "DPT-main-sub" (sub zero-padded to 3 digits)
    static const QRegularExpression reDpt(QStringLiteral("^DPT-(\\d+)(?:-(\\d+))?$"),
                                          QRegularExpression::CaseInsensitiveOption);
    const auto m = reDpt.match(raw);
    if (m.hasMatch()) {
        const QString main = m.captured(1);
        const QString sub  = m.captured(2);
        if (sub.isEmpty()) return main;
        return QStringLiteral("%1.%2").arg(main).arg(sub.toInt(), 3, 10, QLatin1Char('0'));
    }
    return raw;
}

GaCsvImportDialog::GaCsvImportDialog(const QString &filePath, QWidget *parent)
    : QDialog(parent)
    , m_preview(new QTableWidget(0, 3, this))
    , m_summary(new QLabel(this))
{
    setWindowTitle(tr("Gruppenadressen importieren"));
    setMinimumSize(640, 420);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    m_preview->setHorizontalHeaderLabels({tr("Adresse"), tr("Name"), tr("DPT")});
    m_preview->setSelectionMode(QAbstractItemView::NoSelection);
    m_preview->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_preview->setAlternatingRowColors(true);
    m_preview->verticalHeader()->setVisible(false);
    m_preview->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    layout->addWidget(m_preview);
    layout->addWidget(m_summary);

    auto *bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(bbox);

    parse(filePath);
}

void GaCsvImportDialog::parse(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        m_summary->setText(tr("Datei konnte nicht geöffnet werden."));
        return;
    }

    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);

    // Read all lines; skip empty lines and the header row.
    QStringList lines;
    while (!ts.atEnd()) {
        const QString line = ts.readLine();
        if (!line.trimmed().isEmpty())
            lines.append(line);
    }
    f.close();

    // Detect header: first line contains "Address" or "Adresse" in column 1 (0-based)
    int startRow = 0;
    if (!lines.isEmpty()) {
        const QStringList hdr = splitCsvLine(lines[0]);
        if (hdr.size() > 1) {
            const QString c1 = clean(hdr[1]).toLower();
            if (c1.contains(QLatin1String("address")) || c1.contains(QLatin1String("adresse")))
                startRow = 1;
        }
    }

    // Column index detection: scan header for known column names
    // ETS exports vary; common order: Name;Address;...;DPT;DatapointSubtype
    int colName = 0, colAddr = 1, colDpt = -1;
    if (startRow == 1) {
        const QStringList hdr = splitCsvLine(lines[0]);
        for (int i = 0; i < hdr.size(); ++i) {
            const QString h = clean(hdr[i]).toLower();
            if (h.contains(QLatin1String("address")) || h.contains(QLatin1String("adresse")))
                colAddr = i;
            else if (h == QLatin1String("dpt") || h == QLatin1String("datenpunkttyp"))
                colDpt = i;
            else if ((h.contains(QLatin1String("name")) || h.contains(QLatin1String("gruppe"))) && i == 0)
                colName = i;
        }
    }

    m_addresses.clear();

    for (int i = startRow; i < lines.size(); ++i) {
        const QStringList cols = splitCsvLine(lines[i]);
        if (cols.size() <= colAddr) continue;

        const QString addrStr = clean(cols[colAddr]);
        const QString name    = (colName < cols.size()) ? clean(cols[colName]) : QString();
        QString dpt;
        if (colDpt >= 0 && colDpt < cols.size())
            dpt = normalizeDpt(clean(cols[colDpt]));

        // Accept only 3-level addresses (main/middle/sub) — skip group headings (e.g. "0", "0/0")
        const QStringList parts = addrStr.split(QLatin1Char('/'));
        if (parts.size() != 3) continue;
        bool ok1, ok2, ok3;
        const int main = parts[0].toInt(&ok1);
        const int mid  = parts[1].toInt(&ok2);
        const int sub  = parts[2].toInt(&ok3);
        if (!ok1 || !ok2 || !ok3) continue;

        m_addresses.append(GroupAddress(main, mid, sub, name, dpt));
    }

    // Populate preview table
    m_preview->setRowCount(m_addresses.size());
    for (int r = 0; r < m_addresses.size(); ++r) {
        const GroupAddress &ga = m_addresses[r];
        m_preview->setItem(r, 0, new QTableWidgetItem(ga.toString()));
        m_preview->setItem(r, 1, new QTableWidgetItem(ga.name()));
        m_preview->setItem(r, 2, new QTableWidgetItem(ga.dpt()));
    }

    m_summary->setText(m_addresses.isEmpty()
        ? tr("Keine gültigen Gruppenadressen gefunden.")
        : tr("%1 Gruppenadressen erkannt.").arg(m_addresses.size()));
}
