#include "GroupAddressDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>

GroupAddressDialog::GroupAddressDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Gruppenadresse"));
    setMinimumWidth(340);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;

    m_nameEdit = new QLineEdit(this);
    form->addRow(tr("Name:"), m_nameEdit);

    auto *addrRow = new QHBoxLayout;
    m_mainSpin   = new QSpinBox(this); m_mainSpin->setRange(0, 31);
    m_middleSpin = new QSpinBox(this); m_middleSpin->setRange(0, 7);
    m_subSpin    = new QSpinBox(this); m_subSpin->setRange(0, 255);
    addrRow->addWidget(m_mainSpin);
    addrRow->addWidget(new QLabel(QStringLiteral("/"), this));
    addrRow->addWidget(m_middleSpin);
    addrRow->addWidget(new QLabel(QStringLiteral("/"), this));
    addrRow->addWidget(m_subSpin);
    form->addRow(tr("Adresse:"), addrRow);

    m_dptCombo = new QComboBox(this);
    const QStringList dpts = {
        QStringLiteral("1.001 – Schalten"),
        QStringLiteral("1.008 – Hoch/Runter"),
        QStringLiteral("5.001 – Prozentwert"),
        QStringLiteral("5.010 – Zählerwert"),
        QStringLiteral("9.001 – Temperatur"),
        QStringLiteral("9.004 – Beleuchtungsstärke"),
    };
    m_dptCombo->addItems(dpts);
    form->addRow(tr("DPT:"), m_dptCombo);

    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    m_nameEdit->setFocus();
}

QString GroupAddressDialog::name()        const { return m_nameEdit->text().trimmed(); }
int     GroupAddressDialog::mainGroup()   const { return m_mainSpin->value(); }
int     GroupAddressDialog::middleGroup() const { return m_middleSpin->value(); }
int     GroupAddressDialog::subGroup()    const { return m_subSpin->value(); }
QString GroupAddressDialog::dpt()         const {
    const QString t = m_dptCombo->currentText();
    return t.left(t.indexOf(QLatin1Char(' ')));
}
