#include "ConnectDialog.h"
#include "KnxIpDiscovery.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QMessageBox>

ConnectDialog::ConnectDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Bus-Interface verbinden"));
    setMinimumSize(480, 380);

    auto *root = new QVBoxLayout(this);

    // --- Discovery group ---------------------------------------------------
    auto *discGroup  = new QGroupBox(tr("KNXnet/IP Discovery"), this);
    auto *discLayout = new QVBoxLayout(discGroup);

    auto *discTop = new QHBoxLayout;
    m_searchButton = new QPushButton(tr("Suchen"), discGroup);
    m_statusLabel  = new QLabel(tr("Klicken Sie „Suchen“, um verfügbare KNXnet/IP-Interfaces zu finden."), discGroup);
    m_statusLabel->setWordWrap(true);
    discTop->addWidget(m_searchButton);
    discTop->addWidget(m_statusLabel, 1);
    discLayout->addLayout(discTop);

    m_discoveryList = new QListWidget(discGroup);
    discLayout->addWidget(m_discoveryList);
    root->addWidget(discGroup);

    // --- Manual entry group -----------------------------------------------
    auto *manualGroup = new QGroupBox(tr("Manuell verbinden"), this);
    auto *manualForm  = new QFormLayout(manualGroup);
    m_manualIp   = new QLineEdit(QStringLiteral("192.168.1.100"), manualGroup);
    m_manualPort = new QSpinBox(manualGroup);
    m_manualPort->setRange(1, 65535);
    m_manualPort->setValue(3671);
    manualForm->addRow(tr("IP-Adresse:"), m_manualIp);
    manualForm->addRow(tr("Port:"), m_manualPort);
    root->addWidget(manualGroup);

    // --- Dialog buttons ---------------------------------------------------
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Verbinden"));
    root->addWidget(buttons);

    connect(m_searchButton,  &QPushButton::clicked, this, &ConnectDialog::startDiscovery);
    connect(m_discoveryList, &QListWidget::itemSelectionChanged,
            this, &ConnectDialog::onSelectionChanged);
    connect(buttons, &QDialogButtonBox::accepted, this, &ConnectDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ConnectDialog::startDiscovery()
{
    m_discoveryList->clear();
    m_searchButton->setEnabled(false);
    m_statusLabel->setText(tr("Suche läuft…"));

    if (!m_discovery) {
        m_discovery = new KnxIpDiscovery(this);
        connect(m_discovery, &KnxIpDiscovery::deviceFound,
                this, &ConnectDialog::onDeviceFound);
        connect(m_discovery, &KnxIpDiscovery::searchFinished,
                this, &ConnectDialog::onSearchFinished);
    }
    m_discovery->startSearch();
}

void ConnectDialog::onDeviceFound(KnxIpDevice dev)
{
    auto *item = new QListWidgetItem(
        tr("%1  (%2:%3)").arg(dev.name, dev.ipAddress).arg(dev.port));
    item->setData(Qt::UserRole,     dev.ipAddress);
    item->setData(Qt::UserRole + 1, dev.port);
    m_discoveryList->addItem(item);
}

void ConnectDialog::onSearchFinished()
{
    m_searchButton->setEnabled(true);
    if (m_discoveryList->count() == 0)
        m_statusLabel->setText(tr("Keine Interfaces gefunden – bitte IP manuell eingeben."));
    else
        m_statusLabel->setText(tr("%1 Interface(s) gefunden.").arg(m_discoveryList->count()));
}

void ConnectDialog::onSelectionChanged()
{
    const auto *item = m_discoveryList->currentItem();
    if (!item)
        return;
    m_manualIp->setText(item->data(Qt::UserRole).toString());
    m_manualPort->setValue(item->data(Qt::UserRole + 1).toInt());
}

void ConnectDialog::onAccept()
{
    QHostAddress addr(m_manualIp->text().trimmed());
    if (addr.isNull()) {
        QMessageBox::warning(this, tr("Ungültige Adresse"),
            tr("Die IP-Adresse „%1“ ist nicht gültig.").arg(m_manualIp->text()));
        return;
    }
    m_host = addr;
    m_port = static_cast<quint16>(m_manualPort->value());
    accept();
}
