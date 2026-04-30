#include "BusMonitorWidget.h"
#include "BusMonitorModel.h"
#include "InterfaceManager.h"
#include "IKnxInterface.h"
#include "CemiFrame.h"

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
#include <QMessageBox>
#include <QtMath>
#include <cmath>

// ─── DPT encoding ─────────────────────────────────────────────────────────────

// DPT combo index → (label, encoding logic in encodeValue)
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
    // Byte 1: S(1) | E(4) | M[10..8](3)
    // Byte 2: M[7..0](8)
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
    case 0: {   // DPT 1.x  — inline 1-bit value
        const uint8_t v = (t == QLatin1String("1") || t.toLower() == QLatin1String("true")
                          || t.toLower() == QLatin1String("ein")) ? 1 : 0;
        return QByteArray(1, static_cast<char>(v));
    }
    case 1: {   // DPT 5.001 — percentage 0-100 → 0-255
        double pct = t.toDouble(&ok);
        if (!ok) return {};
        const uint8_t raw = static_cast<uint8_t>(qBound(0.0, pct * 255.0 / 100.0, 255.0));
        return QByteArray(1, static_cast<char>(raw));
    }
    case 2: {   // DPT 5.010 — uint8 0-255
        int val = t.toInt(&ok);
        if (!ok) return {};
        return QByteArray(1, static_cast<char>(qBound(0, val, 255)));
    }
    case 3:
    case 4:
    case 5:
    case 6: {   // DPT 9.x — KNX 2-byte float
        double val = t.toDouble(&ok);
        if (!ok) return {};
        return encodeKnxFloat(val);
    }
    case 7: {   // Raw hex
        const QByteArray hex = QByteArray::fromHex(t.remove(QStringLiteral("0x"))
                                                     .remove(QLatin1Char(' '))
                                                     .toLatin1());
        return hex.isEmpty() ? QByteArray(1, '\0') : hex;
    }
    default:
        return {};
    }
}

// ─── Widget construction ───────────────────────────────────────────────────────

BusMonitorWidget::BusMonitorWidget(QWidget *parent)
    : QWidget(parent)
    , m_model(new BusMonitorModel(this))
    , m_proxy(new QSortFilterProxyModel(this))
    , m_view(new QTableView(this))
    , m_startStop(new QPushButton(tr("Pause"), this))
    , m_clear(new QPushButton(tr("Leeren"), this))
    , m_filter(new QLineEdit(this))
    , m_counter(new QLabel(tr("0 Telegramme"), this))
    , m_sendGa(new QLineEdit(this))
    , m_sendDpt(new QComboBox(this))
    , m_sendValue(new QLineEdit(this))
    , m_sendBtn(new QPushButton(tr("Senden"), this))
    , m_readBtn(new QPushButton(tr("Lesen"), this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);

    auto *header = new QLabel(tr("Busmonitor"), this);
    header->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 11pt;"));
    layout->addWidget(header);

    // ── Toolbar ────────────────────────────────────────────────────────────────
    auto *toolbar = new QHBoxLayout;
    toolbar->addWidget(m_startStop);
    toolbar->addWidget(m_clear);
    m_filter->setPlaceholderText(tr("Filter (Quelle/Ziel/Typ)…"));
    toolbar->addWidget(m_filter, 1);
    toolbar->addWidget(m_counter);
    layout->addLayout(toolbar);

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
    connect(m_startStop, &QPushButton::clicked, this, &BusMonitorWidget::onStartStopClicked);
    connect(m_clear,     &QPushButton::clicked, this, &BusMonitorWidget::onClearClicked);
    connect(m_filter,    &QLineEdit::textChanged, this, &BusMonitorWidget::onFilterChanged);
    connect(m_sendBtn,   &QPushButton::clicked, this, &BusMonitorWidget::onSendClicked);
    connect(m_readBtn,   &QPushButton::clicked, this, &BusMonitorWidget::onReadClicked);
    connect(m_sendValue, &QLineEdit::returnPressed, this, &BusMonitorWidget::onSendClicked);
}

// ─── Interface ────────────────────────────────────────────────────────────────

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
