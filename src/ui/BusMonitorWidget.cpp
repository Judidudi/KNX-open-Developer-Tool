#include "BusMonitorWidget.h"
#include "BusMonitorModel.h"
#include "InterfaceManager.h"
#include "IKnxInterface.h"
#include "CemiFrame.h"
#include "Project.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QTableView>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QCheckBox>
#include <QMessageBox>
#include <QSettings>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QTimer>
#include <cmath>

// ─── Custom proxy: text filter + only-groups flag + source/dest/type regex ───
class BusMonitorProxy : public QSortFilterProxyModel
{
public:
    explicit BusMonitorProxy(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent) {}

    void setOnlyGroups(bool b)                    { m_onlyGroups = b; invalidateFilter(); }
    void setSourceRegex(const QRegularExpression &re) { m_srcRe = re; invalidateFilter(); }
    void setDestRegex(const QRegularExpression &re)   { m_dstRe = re; invalidateFilter(); }
    // typeKeyword: "", "Write", "Read", "Response"
    void setTypeKeyword(const QString &kw)        { m_typeKw = kw; invalidateFilter(); }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        const auto *src = sourceModel();

        if (m_onlyGroups) {
            const QModelIndex ti = src->index(sourceRow, BusMonitorModel::ColType, sourceParent);
            if (!src->data(ti).toString().startsWith(QLatin1String("GroupValue")))
                return false;
        }
        if (m_srcRe.isValid() && !m_srcRe.pattern().isEmpty()) {
            const QModelIndex si = src->index(sourceRow, BusMonitorModel::ColSource, sourceParent);
            if (!m_srcRe.match(src->data(si).toString()).hasMatch())
                return false;
        }
        if (m_dstRe.isValid() && !m_dstRe.pattern().isEmpty()) {
            const QModelIndex di = src->index(sourceRow, BusMonitorModel::ColDestination, sourceParent);
            if (!m_dstRe.match(src->data(di).toString()).hasMatch())
                return false;
        }
        if (!m_typeKw.isEmpty()) {
            const QModelIndex ti = src->index(sourceRow, BusMonitorModel::ColType, sourceParent);
            if (!src->data(ti).toString().contains(m_typeKw))
                return false;
        }
        return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
    }

private:
    bool              m_onlyGroups = false;
    QRegularExpression m_srcRe;
    QRegularExpression m_dstRe;
    QString           m_typeKw;
};

// ─── DPT encoding ─────────────────────────────────────────────────────────────

static const struct { const char *label; } kDpts[] = {
    { "DPT 1.x – Bool (0/1)"            },  // 0
    { "DPT 5.001 – Prozent (0–100 %)"   },  // 1
    { "DPT 5.010 – Zahl (0–255)"        },  // 2
    { "DPT 9.001 – Temperatur (°C)"     },  // 3
    { "DPT 9.002 – Temperaturdiff."     },  // 4
    { "DPT 9.007 – Luftfeuchte (%)"     },  // 5
    { "DPT 9.x – KNX-Float allgemein"   },  // 6
    { "Rohdaten (Hex)"                  },  // 7
};

static QByteArray encodeKnxFloat(double value)
{
    const bool negative = value < 0.0;
    double absVal = std::fabs(value);
    int32_t M = static_cast<int32_t>(std::round(absVal * 100.0));
    int E = 0;
    while (M > 2047 && E < 15) { M >>= 1; ++E; }
    M = qBound(0, M, 2047);
    const uint8_t b0 = static_cast<uint8_t>((negative ? 0x80 : 0x00) |
                                             ((E & 0x0F) << 3) |
                                             ((M >> 8) & 0x07));
    const uint8_t b1 = static_cast<uint8_t>(M & 0xFF);
    return QByteArray(1, static_cast<char>(b0)) + QByteArray(1, static_cast<char>(b1));
}

QByteArray BusMonitorWidget::encodeValue(int dptIndex, const QString &text)
{
    QString t = text.trimmed();
    bool ok = false;
    switch (dptIndex) {
    case 0: {
        const uint8_t v = (t == QLatin1String("1") || t.toLower() == QLatin1String("true")
                          || t.toLower() == QLatin1String("ein")) ? 1 : 0;
        return QByteArray(1, static_cast<char>(v));
    }
    case 1: {
        double pct = t.toDouble(&ok);
        if (!ok) return {};
        const uint8_t raw = static_cast<uint8_t>(qBound(0.0, pct * 255.0 / 100.0, 255.0));
        return QByteArray(1, static_cast<char>(raw));
    }
    case 2: {
        int val = t.toInt(&ok);
        if (!ok) return {};
        return QByteArray(1, static_cast<char>(qBound(0, val, 255)));
    }
    case 3: case 4: case 5: case 6: {
        double val = t.toDouble(&ok);
        if (!ok) return {};
        return encodeKnxFloat(val);
    }
    case 7: {
        const QByteArray hex = QByteArray::fromHex(t.remove(QStringLiteral("0x"))
                                                     .remove(QLatin1Char(' '))
                                                     .toLatin1());
        return hex.isEmpty() ? QByteArray(1, '\0') : hex;
    }
    default: return {};
    }
}

// ─── Widget construction ───────────────────────────────────────────────────────

BusMonitorWidget::BusMonitorWidget(QWidget *parent)
    : QWidget(parent)
    , m_model(new BusMonitorModel(this))
    , m_proxy(new BusMonitorProxy(this))
    , m_view(new QTableView(this))
    , m_startStop(new QPushButton(tr("Pause"), this))
    , m_clear(new QPushButton(tr("Leeren"), this))
    , m_filter(new QLineEdit(this))
    , m_onlyGroups(new QCheckBox(tr("Nur Gruppen"), this))
    , m_counter(new QLabel(tr("0 Telegramme"), this))
    , m_srcFilter(new QLineEdit(this))
    , m_dstFilter(new QLineEdit(this))
    , m_typeFilter(new QComboBox(this))
    , m_tsFormat(new QComboBox(this))
    , m_exportBtn(new QPushButton(tr("CSV↓"), this))
    , m_sendGa(new QLineEdit(this))
    , m_sendDpt(new QComboBox(this))
    , m_sendValue(new QLineEdit(this))
    , m_sendBtn(new QPushButton(tr("Senden"), this))
    , m_readBtn(new QPushButton(tr("Lesen"), this))
{
    const QSettings s;
    const int maxE = s.value(QStringLiteral("busMonitor/maxEntries"), 2000).toInt();
    m_model->setMaxEntries(qMax(100, maxE));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);

    auto *header = new QLabel(tr("Busmonitor"), this);
    header->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 11pt;"));
    layout->addWidget(header);

    // ── Toolbar row 1: start/stop, clear, text filter, only-groups, counter ──
    auto *toolbar1 = new QHBoxLayout;
    toolbar1->addWidget(m_startStop);
    toolbar1->addWidget(m_clear);
    m_filter->setPlaceholderText(tr("Freitext-Filter (alle Spalten)…"));
    m_filter->setClearButtonEnabled(true);
    toolbar1->addWidget(m_filter, 1);
    m_onlyGroups->setToolTip(tr("Nur Gruppenwert-Telegramme anzeigen"));
    toolbar1->addWidget(m_onlyGroups);
    toolbar1->addWidget(m_counter);
    layout->addLayout(toolbar1);

    // ── Toolbar row 2: source regex, dest regex, type, timestamp format, export ─
    auto *toolbar2 = new QHBoxLayout;
    m_srcFilter->setPlaceholderText(tr("Quelle (Regex)"));
    m_srcFilter->setClearButtonEnabled(true);
    m_srcFilter->setMaximumWidth(160);
    toolbar2->addWidget(new QLabel(tr("Quelle:"), this));
    toolbar2->addWidget(m_srcFilter);

    m_dstFilter->setPlaceholderText(tr("Ziel (Regex)"));
    m_dstFilter->setClearButtonEnabled(true);
    m_dstFilter->setMaximumWidth(160);
    toolbar2->addWidget(new QLabel(tr("Ziel:"), this));
    toolbar2->addWidget(m_dstFilter);

    m_typeFilter->addItem(tr("Alle Typen"),        QString());
    m_typeFilter->addItem(tr("GroupValueWrite"),   QStringLiteral("Write"));
    m_typeFilter->addItem(tr("GroupValueRead"),    QStringLiteral("Read"));
    m_typeFilter->addItem(tr("GroupValueResponse"),QStringLiteral("Response"));
    toolbar2->addWidget(new QLabel(tr("Typ:"), this));
    toolbar2->addWidget(m_typeFilter);

    m_tsFormat->addItem(tr("Absolut"), false);
    m_tsFormat->addItem(tr("Relativ"), true);
    toolbar2->addWidget(new QLabel(tr("Zeit:"), this));
    toolbar2->addWidget(m_tsFormat);

    toolbar2->addStretch(1);
    m_exportBtn->setToolTip(tr("Sichtbare Zeilen als CSV exportieren"));
    toolbar2->addWidget(m_exportBtn);
    layout->addLayout(toolbar2);

    // ── Telegram table ─────────────────────────────────────────────────────────
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1);

    m_view->setModel(m_proxy);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setAlternatingRowColors(true);
    m_view->verticalHeader()->setVisible(false);
    m_view->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(m_view);

    // ── Send panel ─────────────────────────────────────────────────────────────
    auto *sendBox = new QGroupBox(tr("Telegramm senden"), this);
    auto *sendLayout = new QHBoxLayout(sendBox);
    sendLayout->setContentsMargins(6, 4, 6, 4);

    m_sendGa->setPlaceholderText(tr("GA (z.B. 0/0/1)"));
    m_sendGa->setMaximumWidth(110);
    sendLayout->addWidget(new QLabel(tr("GA:"), this));
    sendLayout->addWidget(m_sendGa);

    for (const auto &dpt : kDpts)
        m_sendDpt->addItem(tr(dpt.label));
    m_sendDpt->setCurrentIndex(0);
    sendLayout->addWidget(new QLabel(tr("DPT:"), this));
    sendLayout->addWidget(m_sendDpt, 1);

    m_sendValue->setPlaceholderText(tr("Wert"));
    m_sendValue->setMaximumWidth(120);
    sendLayout->addWidget(new QLabel(tr("Wert:"), this));
    sendLayout->addWidget(m_sendValue);

    sendLayout->addWidget(m_sendBtn);
    sendLayout->addWidget(m_readBtn);

    layout->addWidget(sendBox);

    // ── Signals ────────────────────────────────────────────────────────────────
    connect(m_startStop,  &QPushButton::clicked,       this, &BusMonitorWidget::onStartStopClicked);
    connect(m_clear,      &QPushButton::clicked,       this, &BusMonitorWidget::onClearClicked);
    connect(m_filter,     &QLineEdit::textChanged,     this, &BusMonitorWidget::onFilterChanged);
    connect(m_onlyGroups, &QCheckBox::toggled,         this, &BusMonitorWidget::onOnlyGroupsToggled);
    connect(m_srcFilter,  &QLineEdit::textChanged,     this, &BusMonitorWidget::onSrcFilterChanged);
    connect(m_dstFilter,  &QLineEdit::textChanged,     this, &BusMonitorWidget::onDstFilterChanged);
    connect(m_typeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BusMonitorWidget::onTypeFilterChanged);
    connect(m_tsFormat,   QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BusMonitorWidget::onTsFormatChanged);
    connect(m_exportBtn,  &QPushButton::clicked,       this, &BusMonitorWidget::onExportCsv);
    connect(m_sendBtn,    &QPushButton::clicked,       this, &BusMonitorWidget::onSendClicked);
    connect(m_readBtn,    &QPushButton::clicked,       this, &BusMonitorWidget::onReadClicked);
    connect(m_sendValue,  &QLineEdit::returnPressed,   this, &BusMonitorWidget::onSendClicked);
}

// ─── Interface ────────────────────────────────────────────────────────────────

void BusMonitorWidget::setProject(Project *project)
{
    m_model->setProject(project);
}

void BusMonitorWidget::setInterfaceManager(InterfaceManager *mgr)
{
    if (m_iface)
        disconnect(m_iface, nullptr, this, nullptr);
    m_iface = mgr;
    if (m_iface) {
        connect(m_iface, &InterfaceManager::cemiFrameReceived,
                this, &BusMonitorWidget::onCemiReceived);
    }
}

void BusMonitorWidget::applySettings()
{
    QSettings s;
    const int maxE = s.value(QStringLiteral("busMonitor/maxEntries"), 2000).toInt();
    m_model->setMaxEntries(maxE);
}

// ─── Slots ────────────────────────────────────────────────────────────────────

void BusMonitorWidget::onCemiReceived(const QByteArray &cemi)
{
    if (!m_running)
        return;
    m_model->appendCemi(cemi);
    ++m_totalRows;
    m_counter->setText(tr("%1 Telegramme").arg(m_totalRows));
    m_view->scrollToBottom();
}

void BusMonitorWidget::onStartStopClicked()
{
    m_running = !m_running;
    m_startStop->setText(m_running ? tr("Pause") : tr("Fortsetzen"));
}

void BusMonitorWidget::onClearClicked()
{
    m_model->clear();
    m_totalRows = 0;
    m_counter->setText(tr("0 Telegramme"));
}

void BusMonitorWidget::onFilterChanged(const QString &text)
{
    m_proxy->setFilterFixedString(text);
}

void BusMonitorWidget::onOnlyGroupsToggled(bool checked)
{
    m_proxy->setOnlyGroups(checked);
}

void BusMonitorWidget::onSrcFilterChanged(const QString &text)
{
    m_proxy->setSourceRegex(QRegularExpression(text, QRegularExpression::CaseInsensitiveOption));
}

void BusMonitorWidget::onDstFilterChanged(const QString &text)
{
    m_proxy->setDestRegex(QRegularExpression(text, QRegularExpression::CaseInsensitiveOption));
}

void BusMonitorWidget::onTypeFilterChanged(int index)
{
    m_proxy->setTypeKeyword(m_typeFilter->itemData(index).toString());
}

void BusMonitorWidget::onTsFormatChanged(int index)
{
    m_model->setRelativeTimestamps(m_tsFormat->itemData(index).toBool());
}

void BusMonitorWidget::onExportCsv()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Busmonitor als CSV exportieren"), {},
        tr("CSV-Dateien (*.csv);;Alle Dateien (*)"));
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Fehler"),
            tr("Datei konnte nicht geöffnet werden:\n%1").arg(f.errorString()));
        return;
    }

    QTextStream out(&f);
    // CSV header
    const QStringList headers = {
        tr("Zeit"), tr("Quelle"), tr("Ziel"), tr("Typ"), tr("Wert / DPT"), tr("Rohdaten")
    };
    out << headers.join(QLatin1Char(';')) << QLatin1Char('\n');

    // Rows — only what the proxy currently shows
    const int rows = m_proxy->rowCount();
    const int cols = m_proxy->columnCount();
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (c > 0) out << QLatin1Char(';');
            QString cell = m_proxy->data(m_proxy->index(r, c)).toString();
            // Quote cells that contain delimiter, quote or newline
            if (cell.contains(QLatin1Char(';')) || cell.contains(QLatin1Char('"'))
                    || cell.contains(QLatin1Char('\n'))) {
                cell.replace(QLatin1Char('"'), QStringLiteral("\"\""));
                cell = QLatin1Char('"') + cell + QLatin1Char('"');
            }
            out << cell;
        }
        out << QLatin1Char('\n');
    }
    // status feedback via counter label
    m_counter->setText(tr("%1 Zeilen exportiert").arg(rows));
    QTimer::singleShot(3000, m_counter, [this](){
        m_counter->setText(tr("%1 Telegramme").arg(m_totalRows));
    });
}

void BusMonitorWidget::onSendClicked()
{
    if (!m_iface || !m_iface->activeInterface()) {
        QMessageBox::warning(this, tr("Nicht verbunden"),
            tr("Bitte zuerst mit einem Bus-Interface verbinden."));
        return;
    }
    const QString gaStr = m_sendGa->text().trimmed();
    if (gaStr.isEmpty()) {
        QMessageBox::warning(this, tr("Gruppenadresse fehlt"),
            tr("Bitte eine Gruppenadresse eingeben (z. B. 0/0/1)."));
        return;
    }
    const uint16_t ga = CemiFrame::groupAddrFromString(gaStr);
    const QByteArray payload = encodeValue(m_sendDpt->currentIndex(), m_sendValue->text());
    if (payload.isEmpty()) {
        QMessageBox::warning(this, tr("Ungültiger Wert"),
            tr("Der eingegebene Wert konnte nicht kodiert werden."));
        return;
    }
    m_iface->activeInterface()->sendCemiFrame(CemiFrame::buildGroupValueWrite(ga, payload));
}

void BusMonitorWidget::onReadClicked()
{
    if (!m_iface || !m_iface->activeInterface()) {
        QMessageBox::warning(this, tr("Nicht verbunden"),
            tr("Bitte zuerst mit einem Bus-Interface verbinden."));
        return;
    }
    const QString gaStr = m_sendGa->text().trimmed();
    if (gaStr.isEmpty()) {
        QMessageBox::warning(this, tr("Gruppenadresse fehlt"),
            tr("Bitte eine Gruppenadresse eingeben (z. B. 0/0/1)."));
        return;
    }
    const uint16_t ga = CemiFrame::groupAddrFromString(gaStr);
    m_iface->activeInterface()->sendCemiFrame(CemiFrame::buildGroupValueRead(ga));
}
