#include "GroupMonitorWidget.h"
#include "Project.h"
#include "GroupAddress.h"
#include "InterfaceManager.h"
#include "IKnxInterface.h"
#include "CemiFrame.h"
#include "DptRegistry.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QDateTime>
#include <cmath>
#include <cstdint>

// Column indices
enum Col { ColGa = 0, ColName, ColDpt, ColValue, ColTime, ColRead, ColSendValue, ColSend, ColCount };

// ─── DPT encoding ─────────────────────────────────────────────────────────────

QByteArray GroupMonitorWidget::encodeValue(const QString &dpt, const QString &text, bool *ok)
{
    if (ok) *ok = true;

    if (dpt.startsWith(QLatin1String("1."))) {
        // Boolean: accept "1", "ein", "true", "on" → 1; anything else → 0
        const QString lower = text.trimmed().toLower();
        const bool v = (lower == QLatin1String("1") || lower == QLatin1String("ein") ||
                        lower == QLatin1String("true") || lower == QLatin1String("on"));
        return QByteArray(1, static_cast<char>(v ? 0x01 : 0x00));
    }

    if (dpt.startsWith(QLatin1String("5."))) {
        bool conv = false;
        int val = text.trimmed().toInt(&conv);
        if (!conv) { if (ok) *ok = false; return {}; }
        if (dpt == QLatin1String("5.001")) {
            // percent → raw 0–255
            val = qBound(0, qRound(val * 255.0 / 100.0), 255);
        } else {
            val = qBound(0, val, 255);
        }
        return QByteArray(1, static_cast<char>(val));
    }

    if (dpt.startsWith(QLatin1String("9."))) {
        bool conv = false;
        double d = text.trimmed().toDouble(&conv);
        if (!conv) { if (ok) *ok = false; return {}; }
        // Encode KNX 2-byte float F16 (EIS 5 format)
        d = qBound(-671088.64, d, 670760.96);
        int exp = 0;
        double mant = d / 0.01;
        while (std::abs(mant) > 2047.0 && exp < 15) { mant /= 2.0; ++exp; }
        const int16_t m = static_cast<int16_t>(qRound(mant));
        const uint8_t b0 = static_cast<uint8_t>(((m < 0 ? 1 : 0) << 7) | (exp << 3) | ((m >> 8) & 0x07));
        const uint8_t b1 = static_cast<uint8_t>(m & 0xFF);
        QByteArray out(2, 0);
        out[0] = static_cast<char>(b0);
        out[1] = static_cast<char>(b1);
        return out;
    }

    // Fallback: hex string ("01 FF A3") or plain decimal
    const QString stripped = text.trimmed().remove(QLatin1Char(' '));
    if (stripped.isEmpty()) { if (ok) *ok = false; return {}; }
    const QByteArray raw = QByteArray::fromHex(stripped.toLatin1());
    if (raw.isEmpty()) { if (ok) *ok = false; return {}; }
    return raw;
}

// ─── DPT value decoding ───────────────────────────────────────────────────────

static QString decodeKnxFloat(const QByteArray &apdu)
{
    const QByteArray payload = apdu.mid(1);
    if (payload.size() < 2) return QStringLiteral("?");
    const uint8_t b0 = static_cast<uint8_t>(payload[0]);
    const uint8_t b1 = static_cast<uint8_t>(payload[1]);
    const bool neg   = (b0 & 0x80) != 0;
    const int  exp   = (b0 >> 3) & 0x0F;
    const int  mant  = ((b0 & 0x07) << 8) | b1;
    const double val = (neg ? -(2048 - mant) : mant) * 0.01 * std::pow(2.0, exp);
    return QString::number(val, 'f', 2);
}

QString GroupMonitorWidget::decodeValue(const QString &dpt, const QByteArray &apdu)
{
    if (apdu.isEmpty()) return {};
    const uint8_t a0 = static_cast<uint8_t>(apdu[0]);

    if (dpt.startsWith(QLatin1String("1."))) {
        return (a0 & 0x01) ? QStringLiteral("1 (EIN)") : QStringLiteral("0 (AUS)");
    }
    if (dpt.startsWith(QLatin1String("5."))) {
        if (apdu.size() < 2) return QString::number(a0 & 0x3F);
        const uint8_t raw = static_cast<uint8_t>(apdu[1]);
        if (dpt == QLatin1String("5.001"))
            return QString::number(qRound(raw * 100.0 / 255.0)) + QStringLiteral(" %");
        return QString::number(raw);
    }
    if (dpt.startsWith(QLatin1String("9."))) {
        return decodeKnxFloat(apdu);
    }

    const QByteArray payload = apdu.mid(1);
    return QString::fromLatin1(payload.toHex(' ').toUpper());
}

// ─── Widget ───────────────────────────────────────────────────────────────────

GroupMonitorWidget::GroupMonitorWidget(QWidget *parent)
    : QWidget(parent)
    , m_table(new QTableWidget(0, ColCount, this))
    , m_readAll(new QPushButton(tr("Alle lesen"), this))
    , m_status(new QLabel(this))
    , m_filter(new QLineEdit(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);

    auto *header = new QLabel(tr("Gruppenadress-Monitor"), this);
    header->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 11pt;"));
    layout->addWidget(header);

    auto *toolbar = new QHBoxLayout;
    toolbar->addWidget(m_readAll);
    m_filter->setPlaceholderText(tr("Suchen (Adresse oder Name)…"));
    m_filter->setClearButtonEnabled(true);
    toolbar->addWidget(m_filter, 1);
    toolbar->addWidget(m_status);
    layout->addLayout(toolbar);

    m_table->setHorizontalHeaderLabels({
        tr("GA"), tr("Name"), tr("DPT"), tr("Wert"), tr("Letzte Änderung"),
        tr("Lesen"), tr("Sendewert"), tr("Senden")
    });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(ColName,      QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ColValue,     QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColTime,      QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColRead,      QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColSendValue, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColSend,      QHeaderView::ResizeToContents);
    layout->addWidget(m_table);

    connect(m_readAll, &QPushButton::clicked, this, &GroupMonitorWidget::onReadAllClicked);
    connect(m_filter, &QLineEdit::textChanged, this, &GroupMonitorWidget::onFilterChanged);
}

void GroupMonitorWidget::setProject(Project *project)
{
    m_project = project;
    rebuild();
}

void GroupMonitorWidget::setInterfaceManager(InterfaceManager *mgr)
{
    if (m_iface)
        disconnect(m_iface, nullptr, this, nullptr);
    m_iface = mgr;
    if (m_iface)
        connect(m_iface, &InterfaceManager::cemiFrameReceived,
                this, &GroupMonitorWidget::onCemiReceived);
}

void GroupMonitorWidget::rebuild()
{
    m_table->setRowCount(0);
    m_gaRow.clear();

    if (!m_project) return;

    const QList<GroupAddress> &gas = m_project->groupAddresses();
    m_table->setRowCount(gas.size());

    for (int r = 0; r < gas.size(); ++r) {
        const GroupAddress &ga = gas[r];
        const uint16_t raw = ga.toRaw();
        m_gaRow.insert(raw, r);

        m_table->setItem(r, ColGa,   new QTableWidgetItem(ga.toString()));
        m_table->setItem(r, ColName, new QTableWidgetItem(ga.name()));

        auto *dptItem = new QTableWidgetItem(ga.dpt());
        if (const DptInfo *info = DptRegistry::instance().find(ga.dpt()))
            dptItem->setToolTip(info->nameDe + QStringLiteral(" (") + info->name + QLatin1Char(')'));
        m_table->setItem(r, ColDpt, dptItem);

        m_table->setItem(r, ColValue,new QTableWidgetItem(QStringLiteral("–")));
        m_table->setItem(r, ColTime, new QTableWidgetItem());

        auto *readBtn = new QPushButton(tr("Lesen"), this);
        readBtn->setMaximumHeight(22);
        const int row = r;
        connect(readBtn, &QPushButton::clicked, this, [this, row](){ onReadRowClicked(row); });
        m_table->setCellWidget(r, ColRead, readBtn);

        // Send value editor + button in a small inline widget
        auto *sendWidget = new QWidget(this);
        auto *sendLayout = new QHBoxLayout(sendWidget);
        sendLayout->setContentsMargins(2, 1, 2, 1);
        sendLayout->setSpacing(3);
        auto *valueEdit = new QLineEdit(sendWidget);
        valueEdit->setMaximumHeight(22);
        valueEdit->setMinimumWidth(60);
        // Pre-fill sensible placeholder per DPT
        const QString &dpt = ga.dpt();
        if (dpt.startsWith(QLatin1String("1.")))
            valueEdit->setPlaceholderText(QStringLiteral("0/1"));
        else if (dpt.startsWith(QLatin1String("5.001")))
            valueEdit->setPlaceholderText(QStringLiteral("0–100"));
        else if (dpt.startsWith(QLatin1String("9.")))
            valueEdit->setPlaceholderText(QStringLiteral("z.B. 21.5"));
        else
            valueEdit->setPlaceholderText(QStringLiteral("Hex"));
        sendLayout->addWidget(valueEdit);

        auto *sendBtn = new QPushButton(tr("Senden"), sendWidget);
        sendBtn->setMaximumHeight(22);
        connect(sendBtn, &QPushButton::clicked, this, [this, row](){ onSendRowClicked(row); });
        sendLayout->addWidget(sendBtn);
        m_table->setCellWidget(r, ColSendValue, sendWidget);
        // ColSend merged into sendWidget — leave ColSend empty
        m_table->setSpan(r, ColSendValue, 1, 2);
    }

    m_status->setText(tr("%1 Gruppenadressen").arg(gas.size()));
}

void GroupMonitorWidget::sendRead(uint16_t ga)
{
    if (m_iface && m_iface->activeInterface())
        m_iface->activeInterface()->sendCemiFrame(CemiFrame::buildGroupValueRead(ga));
}

void GroupMonitorWidget::sendWrite(uint16_t ga, const QString &dpt, const QString &text)
{
    if (!m_iface || !m_iface->activeInterface()) return;
    bool ok = false;
    const QByteArray payload = encodeValue(dpt, text, &ok);
    if (!ok) return;
    m_iface->activeInterface()->sendCemiFrame(CemiFrame::buildGroupValueWrite(ga, payload));
}

void GroupMonitorWidget::onReadAllClicked()
{
    if (!m_project) return;
    for (const GroupAddress &ga : m_project->groupAddresses())
        sendRead(ga.toRaw());
}

void GroupMonitorWidget::onReadRowClicked(int row)
{
    if (!m_project) return;
    const QList<GroupAddress> &gas = m_project->groupAddresses();
    if (row < 0 || row >= gas.size()) return;
    sendRead(gas[row].toRaw());
}

void GroupMonitorWidget::onSendRowClicked(int row)
{
    if (!m_project) return;
    const QList<GroupAddress> &gas = m_project->groupAddresses();
    if (row < 0 || row >= gas.size()) return;

    // Retrieve the QLineEdit inside the cell widget
    auto *cellWidget = m_table->cellWidget(row, ColSendValue);
    if (!cellWidget) return;
    const auto *edit = cellWidget->findChild<QLineEdit *>();
    if (!edit) return;

    const QString dpt  = m_table->item(row, ColDpt) ? m_table->item(row, ColDpt)->text() : QString();
    sendWrite(gas[row].toRaw(), dpt, edit->text());
}

void GroupMonitorWidget::onFilterChanged(const QString &text)
{
    const QString lower = text.trimmed().toLower();
    for (int r = 0; r < m_table->rowCount(); ++r) {
        bool match = lower.isEmpty();
        if (!match) {
            const QString ga   = m_table->item(r, ColGa)   ? m_table->item(r, ColGa)->text().toLower()   : QString();
            const QString name = m_table->item(r, ColName) ? m_table->item(r, ColName)->text().toLower() : QString();
            match = ga.contains(lower) || name.contains(lower);
        }
        m_table->setRowHidden(r, !match);
    }
}

void GroupMonitorWidget::onCemiReceived(const QByteArray &cemi)
{
    const CemiFrame frame = CemiFrame::fromBytes(cemi);
    if (!frame.isGroupValueResponse() || !frame.groupAddress) return;

    const uint16_t ga = frame.destAddress;
    auto it = m_gaRow.find(ga);
    if (it == m_gaRow.end()) return;

    const int row = it.value();
    const QString dpt = m_table->item(row, ColDpt) ? m_table->item(row, ColDpt)->text() : QString();
    const QString val = decodeValue(dpt, frame.apdu);
    const QString ts  = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));

    if (auto *item = m_table->item(row, ColValue)) item->setText(val);
    if (auto *item = m_table->item(row, ColTime))  item->setText(ts);
}
