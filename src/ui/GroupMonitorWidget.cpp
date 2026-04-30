#include "GroupMonitorWidget.h"
#include "Project.h"
#include "GroupAddress.h"
#include "InterfaceManager.h"
#include "IKnxInterface.h"
#include "CemiFrame.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QDateTime>
#include <cmath>
#include <cstdint>

// Column indices
enum Col { ColGa = 0, ColName, ColDpt, ColValue, ColTime, ColRead, ColCount };

// ─── DPT value decoding ───────────────────────────────────────────────────────

static QString decodeKnxFloat(const QByteArray &apdu)
{
    // GroupValue_Response: APDU[0]=0x04, APDU[1..2] = data bytes
    // For small values (≤6 bit), data is in APDU[0] bits 0-5 + APDU[1]
    // For 2-byte float, the full apdu payload is ≥2 bytes after APCI
    const QByteArray payload = apdu.mid(1); // strip APCI byte
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

    // APCI is in APDU[0] bits 7..2 combined with APDU[1] bits 7..6 = 10 bits total
    // For GroupValue_Response, bits 5..0 of APDU[0] carry small values (≤6 bit).
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

    // Fallback: hex
    const QByteArray payload = apdu.mid(1);
    return QString::fromLatin1(payload.toHex(' ').toUpper());
}

// ─── Widget ───────────────────────────────────────────────────────────────────

GroupMonitorWidget::GroupMonitorWidget(QWidget *parent)
    : QWidget(parent)
    , m_table(new QTableWidget(0, ColCount, this))
    , m_readAll(new QPushButton(tr("Alle lesen"), this))
    , m_status(new QLabel(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);

    auto *header = new QLabel(tr("Gruppenadress-Monitor"), this);
    header->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 11pt;"));
    layout->addWidget(header);

    auto *toolbar = new QHBoxLayout;
    toolbar->addWidget(m_readAll);
    toolbar->addStretch();
    toolbar->addWidget(m_status);
    layout->addLayout(toolbar);

    m_table->setHorizontalHeaderLabels({
        tr("GA"), tr("Name"), tr("DPT"), tr("Wert"), tr("Letzte Änderung"), tr("Lesen")
    });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(ColName,  QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ColValue, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColTime,  QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColRead,  QHeaderView::ResizeToContents);
    layout->addWidget(m_table);

    connect(m_readAll, &QPushButton::clicked, this, &GroupMonitorWidget::onReadAllClicked);
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

        auto *itGa   = new QTableWidgetItem(ga.toString());
        auto *itName = new QTableWidgetItem(ga.name());
        auto *itDpt  = new QTableWidgetItem(ga.dpt());
        auto *itVal  = new QTableWidgetItem(QStringLiteral("–"));
        auto *itTime = new QTableWidgetItem();

        m_table->setItem(r, ColGa,   itGa);
        m_table->setItem(r, ColName, itName);
        m_table->setItem(r, ColDpt,  itDpt);
        m_table->setItem(r, ColValue,itVal);
        m_table->setItem(r, ColTime, itTime);

        auto *btn = new QPushButton(tr("Lesen"), this);
        btn->setMaximumHeight(22);
        const int row = r;
        connect(btn, &QPushButton::clicked, this, [this, row](){ onReadRowClicked(row); });
        m_table->setCellWidget(r, ColRead, btn);
    }

    m_status->setText(tr("%1 Gruppenadressen").arg(gas.size()));
}

void GroupMonitorWidget::sendRead(uint16_t ga)
{
    if (m_iface && m_iface->activeInterface())
        m_iface->activeInterface()->sendCemiFrame(CemiFrame::buildGroupValueRead(ga));
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
