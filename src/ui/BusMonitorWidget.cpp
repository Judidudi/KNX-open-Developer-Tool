#include "BusMonitorWidget.h"
#include "BusMonitorModel.h"
#include "InterfaceManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QHeaderView>
#include <QSortFilterProxyModel>

BusMonitorWidget::BusMonitorWidget(QWidget *parent)
    : QWidget(parent)
    , m_model(new BusMonitorModel(this))
    , m_proxy(new QSortFilterProxyModel(this))
    , m_view(new QTableView(this))
    , m_startStop(new QPushButton(tr("Pause"), this))
    , m_clear(new QPushButton(tr("Leeren"), this))
    , m_filter(new QLineEdit(this))
    , m_counter(new QLabel(tr("0 Telegramme"), this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);

    auto *header = new QLabel(tr("Busmonitor"), this);
    header->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 11pt;"));
    layout->addWidget(header);

    auto *toolbar = new QHBoxLayout;
    toolbar->addWidget(m_startStop);
    toolbar->addWidget(m_clear);
    m_filter->setPlaceholderText(tr("Filter (Quelle/Ziel/Typ)…"));
    toolbar->addWidget(m_filter, 1);
    toolbar->addWidget(m_counter);
    layout->addLayout(toolbar);

    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1);  // match against all columns

    m_view->setModel(m_proxy);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setAlternatingRowColors(true);
    m_view->verticalHeader()->setVisible(false);
    m_view->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(m_view);

    connect(m_startStop, &QPushButton::clicked, this, &BusMonitorWidget::onStartStopClicked);
    connect(m_clear,     &QPushButton::clicked, this, &BusMonitorWidget::onClearClicked);
    connect(m_filter,    &QLineEdit::textChanged, this, &BusMonitorWidget::onFilterChanged);
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
