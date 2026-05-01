#include "SettingsDialog.h"

#include <QSettings>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Einstellungen"));
    setMinimumWidth(380);

    auto *mainLayout = new QVBoxLayout(this);

    // ── Verbindung ────────────────────────────────────────────────────────────
    auto *connGroup  = new QGroupBox(tr("KNXnet/IP Standard-Verbindung"), this);
    auto *connForm   = new QFormLayout(connGroup);

    m_hostEdit = new QLineEdit(connGroup);
    m_hostEdit->setPlaceholderText(QStringLiteral("192.168.1.100"));
    connForm->addRow(tr("Standard-Host:"), m_hostEdit);

    m_portSpin = new QSpinBox(connGroup);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(3671);
    connForm->addRow(tr("Port:"), m_portSpin);

    m_connectOnStart = new QCheckBox(tr("Beim Programmstart automatisch verbinden"), connGroup);
    connForm->addRow(QString(), m_connectOnStart);

    mainLayout->addWidget(connGroup);

    // ── Projekt ───────────────────────────────────────────────────────────────
    auto *projGroup = new QGroupBox(tr("Projekt"), this);
    auto *projForm  = new QFormLayout(projGroup);

    m_openLastProjCb = new QCheckBox(tr("Letztes Projekt beim Start öffnen"), projGroup);
    projForm->addRow(QString(), m_openLastProjCb);

    mainLayout->addWidget(projGroup);

    // ── Busmonitor ────────────────────────────────────────────────────────────
    auto *busGroup  = new QGroupBox(tr("Busmonitor"), this);
    auto *busForm   = new QFormLayout(busGroup);

    m_maxEntriesSpin = new QSpinBox(busGroup);
    m_maxEntriesSpin->setRange(100, 100000);
    m_maxEntriesSpin->setSingleStep(1000);
    m_maxEntriesSpin->setSuffix(tr(" Einträge"));
    busForm->addRow(tr("Puffergröße:"), m_maxEntriesSpin);

    mainLayout->addWidget(busGroup);

    // ── Sprache ───────────────────────────────────────────────────────────────
    auto *uiGroup = new QGroupBox(tr("Oberfläche"), this);
    auto *uiForm  = new QFormLayout(uiGroup);

    m_langCombo = new QComboBox(uiGroup);
    m_langCombo->addItem(tr("Deutsch"), QStringLiteral("de"));
    m_langCombo->addItem(tr("English"), QStringLiteral("en"));
    uiForm->addRow(tr("Sprache (nach Neustart):"), m_langCombo);

    mainLayout->addWidget(uiGroup);

    // ── Buttons ───────────────────────────────────────────────────────────────
    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    load();
}

void SettingsDialog::load()
{
    QSettings s;
    m_hostEdit->setText(s.value(QStringLiteral("connection/defaultHost")).toString());
    m_portSpin->setValue(s.value(QStringLiteral("connection/defaultPort"), 3671).toInt());
    m_connectOnStart->setChecked(s.value(QStringLiteral("connection/connectOnStart"), false).toBool());
    m_openLastProjCb->setChecked(s.value(QStringLiteral("project/openLastOnStart"), false).toBool());
    m_maxEntriesSpin->setValue(s.value(QStringLiteral("busMonitor/maxEntries"), 2000).toInt());

    const QString lang = s.value(QStringLiteral("ui/language"), QStringLiteral("de")).toString();
    const int langIdx  = m_langCombo->findData(lang);
    m_langCombo->setCurrentIndex(langIdx >= 0 ? langIdx : 0);
}

void SettingsDialog::save()
{
    QSettings s;
    s.setValue(QStringLiteral("connection/defaultHost"),    m_hostEdit->text().trimmed());
    s.setValue(QStringLiteral("connection/defaultPort"),    m_portSpin->value());
    s.setValue(QStringLiteral("connection/connectOnStart"), m_connectOnStart->isChecked());
    s.setValue(QStringLiteral("project/openLastOnStart"),   m_openLastProjCb->isChecked());
    s.setValue(QStringLiteral("busMonitor/maxEntries"),     m_maxEntriesSpin->value());
    s.setValue(QStringLiteral("ui/language"),               m_langCombo->currentData().toString());
    s.sync();
}

void SettingsDialog::onAccepted()
{
    save();
    accept();
}
