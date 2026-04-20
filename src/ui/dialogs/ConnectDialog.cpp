#include "ConnectDialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QTabWidget>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QDialogButtonBox>

ConnectDialog::ConnectDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Bus-Interface verbinden"));
    setMinimumWidth(380);

    auto *layout = new QVBoxLayout(this);

    auto *tabs = new QTabWidget(this);

    // KNXnet/IP tab
    auto *ipWidget = new QWidget(tabs);
    auto *ipForm = new QFormLayout(ipWidget);
    auto *ipEdit = new QLineEdit(QStringLiteral("224.0.23.12"), ipWidget);
    auto *portSpin = new QSpinBox(ipWidget);
    portSpin->setRange(1, 65535);
    portSpin->setValue(3671);
    ipForm->addRow(tr("IP-Adresse / Multicast:"), ipEdit);
    ipForm->addRow(tr("Port:"), portSpin);
    tabs->addTab(ipWidget, tr("KNXnet/IP"));

    // USB tab
    auto *usbWidget = new QWidget(tabs);
    auto *usbForm = new QFormLayout(usbWidget);
    usbForm->addRow(new QLabel(tr("KNX USB-Interface wird beim Verbinden automatisch erkannt."), usbWidget));
    tabs->addTab(usbWidget, tr("USB-Interface"));

    layout->addWidget(tabs);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Verbinden"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}
