#include "ConnectDialog.h"
#include "KnxIpDiscovery.h"
#include "UsbKnxInterface.h"

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
#include <QTabWidget>
#include <QComboBox>
#include <QMessageBox>

static QWidget *buildKnxIpTab(ConnectDialog *dlg,
                               QListWidget    *&discoveryList,
                               QPushButton    *&searchButton,
                               QLabel         *&statusLabel,
                               QLineEdit      *&manualIp,
                               QSpinBox       *&manualPort)
{
    auto *tab    = new QWidget(dlg);
    auto *layout = new QVBoxLayout(tab);

    // Discovery group
    auto *discGroup  = new QGroupBox(ConnectDialog::tr("Automatische Suche"), tab);
    auto *discLayout = new QVBoxLayout(discGroup);

    auto *discTop = new QHBoxLayout;
    searchButton  = new QPushButton(ConnectDialog::tr("Suchen"), discGroup);
    statusLabel   = new QLabel(
        ConnectDialog::tr("Klicken Sie \"Suchen\", um KNXnet/IP-Interfaces zu finden."), discGroup);
    statusLabel->setWordWrap(true);
    discTop->addWidget(searchButton);
    discTop->addWidget(statusLabel, 1);
    discLayout->addLayout(discTop);

    discoveryList = new QListWidget(discGroup);
    discLayout->addWidget(discoveryList);
    layout->addWidget(discGroup);

    // Manual entry group
    auto *manualGroup = new QGroupBox(ConnectDialog::tr("Manuell verbinden"), tab);
    auto *manualForm  = new QFormLayout(manualGroup);
    manualIp   = new QLineEdit(QStringLiteral("192.168.1.100"), manualGroup);
    manualPort = new QSpinBox(manualGroup);
    manualPort->setRange(1, 65535);
    manualPort->setValue(3671);
    manualForm->addRow(ConnectDialog::tr("IP-Adresse:"), manualIp);
    manualForm->addRow(ConnectDialog::tr("Port:"),       manualPort);
    layout->addWidget(manualGroup);

    return tab;
}

static QWidget *buildUsbTab(ConnectDialog *dlg,
                             QComboBox     *&transportCombo,
                             QComboBox     *&deviceCombo,
                             QPushButton   *&refreshButton,
                             QLabel        *&hintLabel)
{
    auto *tab    = new QWidget(dlg);
    auto *layout = new QVBoxLayout(tab);

    auto *form = new QFormLayout;

    transportCombo = new QComboBox(tab);
    transportCombo->addItem(ConnectDialog::tr("Seriell (USB CDC-ACM, z. B. /dev/ttyACM0)"),
                            static_cast<int>(UsbKnxInterface::Transport::Serial));
    transportCombo->addItem(ConnectDialog::tr("USB HID (/dev/hidrawN, KNX spec 07_01_01)"),
                            static_cast<int>(UsbKnxInterface::Transport::HID));
    form->addRow(ConnectDialog::tr("Transport:"), transportCombo);

    auto *deviceRow = new QHBoxLayout;
    deviceCombo     = new QComboBox(tab);
    deviceCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    refreshButton = new QPushButton(ConnectDialog::tr("Aktualisieren"), tab);
    deviceRow->addWidget(deviceCombo, 1);
    deviceRow->addWidget(refreshButton);
    form->addRow(ConnectDialog::tr("Gerät:"), deviceRow);

    layout->addLayout(form);

    hintLabel = new QLabel(
        ConnectDialog::tr(
            "<i>Seriell:</i> Das Interface benötigt Lese-/Schreibzugriff auf den Port.<br>"
            "Auf Linux: <tt>sudo usermod -aG dialout $USER</tt> und neu anmelden.<br>"
            "<i>HID:</i> udev-Regel für das KNX-Gerät notwendig (Vendor-/Product-ID)."),
        tab);
    hintLabel->setWordWrap(true);
    hintLabel->setTextFormat(Qt::RichText);
    layout->addWidget(hintLabel);
    layout->addStretch();

    return tab;
}

// ---- ConnectDialog ----------------------------------------------------------

ConnectDialog::ConnectDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Bus-Interface verbinden"));
    setMinimumSize(520, 420);

    auto *root = new QVBoxLayout(this);

    m_tabs = new QTabWidget(this);

    auto *knxIpTab = buildKnxIpTab(this,
        m_discoveryList, m_searchButton, m_statusLabel,
        m_manualIp, m_manualPort);
    m_tabs->addTab(knxIpTab, tr("KNXnet/IP"));

    auto *usbTab = buildUsbTab(this,
        m_usbTransport, m_usbDevice, m_usbRefresh, m_usbHint);
    m_tabs->addTab(usbTab, tr("USB-Interface"));
    root->addWidget(m_tabs);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Verbinden"));
    root->addWidget(buttons);

    connect(m_searchButton,    &QPushButton::clicked, this, &ConnectDialog::startDiscovery);
    connect(m_discoveryList,   &QListWidget::itemSelectionChanged,
            this, &ConnectDialog::onKnxIpSelectionChanged);
    connect(m_usbRefresh,      &QPushButton::clicked, this, &ConnectDialog::refreshUsbDevices);
    connect(m_usbTransport,    qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) { refreshUsbDevices(); });
    connect(buttons, &QDialogButtonBox::accepted, this, &ConnectDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    refreshUsbDevices();
}

ConnectDialog::ConnectionType ConnectDialog::connectionType() const
{
    return (m_tabs->currentIndex() == 1) ? ConnectionType::Usb : ConnectionType::KnxIp;
}

UsbKnxInterface::Transport ConnectDialog::usbTransport() const
{
    return static_cast<UsbKnxInterface::Transport>(m_usbTransport->currentData().toInt());
}

QString ConnectDialog::usbDevicePath() const
{
    return m_usbDevice->currentText();
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

void ConnectDialog::onKnxIpSelectionChanged()
{
    const auto *item = m_discoveryList->currentItem();
    if (!item) return;
    m_manualIp->setText(item->data(Qt::UserRole).toString());
    m_manualPort->setValue(item->data(Qt::UserRole + 1).toInt());
}

void ConnectDialog::refreshUsbDevices()
{
    m_usbDevice->clear();
    const auto t = usbTransport();
    const QStringList devices =
        (t == UsbKnxInterface::Transport::Serial)
            ? UsbKnxInterface::availableSerialPorts()
            : UsbKnxInterface::availableHidDevices();

    if (devices.isEmpty()) {
        m_usbDevice->addItem(tr("(Keine Geräte gefunden)"));
        m_usbDevice->setEnabled(false);
    } else {
        for (const QString &d : devices)
            m_usbDevice->addItem(d);
        m_usbDevice->setEnabled(true);
    }
}

void ConnectDialog::onAccept()
{
    if (connectionType() == ConnectionType::Usb) {
        if (!m_usbDevice->isEnabled() || m_usbDevice->currentText().isEmpty()) {
            QMessageBox::warning(this, tr("Kein Gerät"),
                tr("Bitte wählen Sie ein USB-Gerät aus oder schließen Sie es zuerst an."));
            return;
        }
        accept();
        return;
    }

    // KNXnet/IP path
    QHostAddress addr(m_manualIp->text().trimmed());
    if (addr.isNull()) {
        QMessageBox::warning(this, tr("Ungültige Adresse"),
            tr("Die IP-Adresse \"%1\" ist nicht gültig.").arg(m_manualIp->text()));
        return;
    }
    m_host = addr;
    m_port = static_cast<quint16>(m_manualPort->value());
    accept();
}
