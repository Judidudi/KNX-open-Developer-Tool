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
    setMinimumWidth(420);

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

    // ── Stabilität ───────────────────────────────────────────────────────────
    auto *stabGroup = new QGroupBox(tr("Verbindungs-Stabilität"), this);
    auto *stabForm  = new QFormLayout(stabGroup);

    m_autoReconnectCb = new QCheckBox(tr("Bei Verbindungsverlust automatisch neu verbinden"), stabGroup);
    stabForm->addRow(QString(), m_autoReconnectCb);

    m_maxReconnectSpin = new QSpinBox(stabGroup);
    m_maxReconnectSpin->setRange(0, 20);
    m_maxReconnectSpin->setSpecialValueText(tr("Deaktiviert"));
    m_maxReconnectSpin->setSuffix(tr(" Versuche"));
    stabForm->addRow(tr("Max. Reconnect-Versuche:"), m_maxReconnectSpin);

    m_heartbeatTimeoutSpin = new QSpinBox(stabGroup);
    m_heartbeatTimeoutSpin->setRange(1000, 30000);
    m_heartbeatTimeoutSpin->setSingleStep(500);
    m_heartbeatTimeoutSpin->setSuffix(tr(" ms"));
    stabForm->addRow(tr("Heartbeat-Timeout:"), m_heartbeatTimeoutSpin);

    mainLayout->addWidget(stabGroup);

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

    // ── Programmierung ────────────────────────────────────────────────────────
    auto *progGroup = new QGroupBox(tr("Programmierung"), this);
    auto *progForm  = new QFormLayout(progGroup);

    m_ackTimeoutSpin = new QSpinBox(progGroup);
    m_ackTimeoutSpin->setRange(1000, 30000);
    m_ackTimeoutSpin->setSingleStep(500);
    m_ackTimeoutSpin->setSuffix(tr(" ms"));
    progForm->addRow(tr("Transport-ACK-Timeout:"), m_ackTimeoutSpin);

    m_verifyEnabledCb = new QCheckBox(tr("Parameter-Verifikation nach Schreiben"), progGroup);
    progForm->addRow(QString(), m_verifyEnabledCb);

    mainLayout->addWidget(progGroup);

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

    // Enable/disable reconnect options based on checkbox
    connect(m_autoReconnectCb, &QCheckBox::toggled, m_maxReconnectSpin, &QSpinBox::setEnabled);
    connect(m_autoReconnectCb, &QCheckBox::toggled, m_heartbeatTimeoutSpin, &QSpinBox::setEnabled);

    load();
}

void SettingsDialog::load()
{
    QSettings s;
    m_hostEdit->setText(s.value(QStringLiteral("connection/defaultHost")).toString());
    m_portSpin->setValue(s.value(QStringLiteral("connection/defaultPort"), 3671).toInt());
    m_connectOnStart->setChecked(s.value(QStringLiteral("connection/connectOnStart"), false).toBool());

    const bool autoRecon = s.value(QStringLiteral("connection/autoReconnect"), true).toBool();
    m_autoReconnectCb->setChecked(autoRecon);
    m_maxReconnectSpin->setValue(s.value(QStringLiteral("connection/maxReconnectAttempts"), 5).toInt());
    m_heartbeatTimeoutSpin->setValue(s.value(QStringLiteral("connection/heartbeatTimeoutMs"), 3000).toInt());
    m_maxReconnectSpin->setEnabled(autoRecon);
    m_heartbeatTimeoutSpin->setEnabled(autoRecon);

    m_openLastProjCb->setChecked(s.value(QStringLiteral("project/openLastOnStart"), false).toBool());
    m_maxEntriesSpin->setValue(s.value(QStringLiteral("busMonitor/maxEntries"), 2000).toInt());

    m_ackTimeoutSpin->setValue(s.value(QStringLiteral("programming/ackTimeoutMs"), 6000).toInt());
    m_verifyEnabledCb->setChecked(s.value(QStringLiteral("programming/verifyEnabled"), true).toBool());

    const QString lang = s.value(QStringLiteral("ui/language"), QStringLiteral("de")).toString();
    const int langIdx  = m_langCombo->findData(lang);
    m_langCombo->setCurrentIndex(langIdx >= 0 ? langIdx : 0);
}

void SettingsDialog::save()
{
    QSettings s;
    s.setValue(QStringLiteral("connection/defaultHost"),          m_hostEdit->text().trimmed());
    s.setValue(QStringLiteral("connection/defaultPort"),          m_portSpin->value());
    s.setValue(QStringLiteral("connection/connectOnStart"),       m_connectOnStart->isChecked());
    s.setValue(QStringLiteral("connection/autoReconnect"),        m_autoReconnectCb->isChecked());
    s.setValue(QStringLiteral("connection/maxReconnectAttempts"), m_maxReconnectSpin->value());
    s.setValue(QStringLiteral("connection/heartbeatTimeoutMs"),   m_heartbeatTimeoutSpin->value());
    s.setValue(QStringLiteral("project/openLastOnStart"),         m_openLastProjCb->isChecked());
    s.setValue(QStringLiteral("busMonitor/maxEntries"),           m_maxEntriesSpin->value());
    s.setValue(QStringLiteral("programming/ackTimeoutMs"),        m_ackTimeoutSpin->value());
    s.setValue(QStringLiteral("programming/verifyEnabled"),       m_verifyEnabledCb->isChecked());
    s.setValue(QStringLiteral("ui/language"),                     m_langCombo->currentData().toString());
    s.sync();
}

void SettingsDialog::onAccepted()
{
    save();
    accept();
}
