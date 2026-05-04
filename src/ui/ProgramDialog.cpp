#include "ProgramDialog.h"
#include "DeviceProgrammer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QProgressBar>
#include <QListWidget>
#include <QPushButton>
#include <QComboBox>
#include <QGroupBox>

ProgramDialog::ProgramDialog(DeviceProgrammer *programmer, QWidget *parent)
    : QDialog(parent)
    , m_programmer(programmer)
{
    setWindowTitle(tr("Gerät programmieren"));
    setMinimumSize(520, 480);

    auto *layout = new QVBoxLayout(this);

    // Mode selector
    auto *modeBox    = new QGroupBox(tr("Programmiermodus"), this);
    auto *modeLayout = new QHBoxLayout(modeBox);
    m_modeCombo = new QComboBox(modeBox);
    using M = DeviceProgrammer::Mode;
    m_modeCombo->addItem(DeviceProgrammer::modeLabel(M::Full),
                         QVariant::fromValue(M::Full));
    m_modeCombo->addItem(DeviceProgrammer::modeLabel(M::ApplicationOnly),
                         QVariant::fromValue(M::ApplicationOnly));
    m_modeCombo->addItem(DeviceProgrammer::modeLabel(M::PhysicalAddressOnly),
                         QVariant::fromValue(M::PhysicalAddressOnly));
    m_modeCombo->addItem(DeviceProgrammer::modeLabel(M::VerifyOnly),
                         QVariant::fromValue(M::VerifyOnly));
    modeLayout->addWidget(m_modeCombo);
    layout->addWidget(modeBox);

    // Status header
    m_header = new QLabel(tr("Modus wählen und Programmierung starten."), this);
    m_header->setWordWrap(true);
    m_header->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(m_header);

    // Step list
    m_steps = new QListWidget(this);
    layout->addWidget(m_steps);

    // Progress
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    layout->addWidget(m_progress);

    // Warning area (hidden until a warning occurs)
    m_warning = new QLabel(this);
    m_warning->setWordWrap(true);
    m_warning->setStyleSheet(
        QStringLiteral("color: #8B6914; background: #FFF8DC; "
                       "padding: 4px; border: 1px solid #CCA300; border-radius: 3px;"));
    m_warning->setVisible(false);
    layout->addWidget(m_warning);

    // Status line
    m_status = new QLabel(tr("Bereit"), this);
    layout->addWidget(m_status);

    // Buttons
    auto *btns = new QHBoxLayout;
    m_startButton = new QPushButton(tr("Programmieren starten"), this);
    m_closeButton = new QPushButton(tr("Schließen"), this);
    m_closeButton->setEnabled(false);
    btns->addStretch();
    btns->addWidget(m_startButton);
    btns->addWidget(m_closeButton);
    layout->addLayout(btns);

    connect(m_modeCombo,  qOverload<int>(&QComboBox::currentIndexChanged),
            this, &ProgramDialog::onModeChanged);
    connect(m_startButton, &QPushButton::clicked, this, &ProgramDialog::onStartClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &ProgramDialog::onCancelClicked);

    if (m_programmer) {
        connect(m_programmer, &DeviceProgrammer::stepStarted,
                this, &ProgramDialog::onStepStarted);
        connect(m_programmer, &DeviceProgrammer::stepCompleted,
                this, &ProgramDialog::onStepCompleted);
        connect(m_programmer, &DeviceProgrammer::progressUpdated,
                this, &ProgramDialog::onProgressUpdated);
        connect(m_programmer, &DeviceProgrammer::warningOccurred,
                this, &ProgramDialog::onWarning);
        connect(m_programmer, &DeviceProgrammer::finished,
                this, &ProgramDialog::onFinished);
    }

    rebuildStepList();
}

void ProgramDialog::rebuildStepList()
{
    m_steps->clear();
    if (!m_programmer) return;
    for (const DeviceProgrammer::Step s : m_programmer->stepSequence()) {
        if (s == DeviceProgrammer::StepDone) continue;
        m_steps->addItem(QStringLiteral("○  ") + DeviceProgrammer::stepLabel(s));
    }
}

void ProgramDialog::onModeChanged(int index)
{
    if (!m_programmer) return;
    const auto mode = m_modeCombo->itemData(index).value<DeviceProgrammer::Mode>();
    m_programmer->setMode(mode);
    rebuildStepList();
    m_progress->setValue(0);
    m_warning->setVisible(false);
    m_status->setText(tr("Bereit"));
    m_header->setStyleSheet(QStringLiteral("font-weight: bold;"));
    m_header->setText(tr("Modus wählen und Programmierung starten."));
}

void ProgramDialog::onStartClicked()
{
    if (!m_programmer) return;
    m_startButton->setEnabled(false);
    m_modeCombo->setEnabled(false);
    m_warning->setVisible(false);
    // Rebuild step list now to reflect mode (in case mode was changed)
    rebuildStepList();
    m_programmer->start();
}

void ProgramDialog::onCancelClicked()
{
    if (m_programmer && !m_closeButton->isEnabled())
        m_programmer->cancel();
    close();
}

void ProgramDialog::onStepStarted(int step, const QString &description)
{
    if (!m_programmer) return;
    const auto s = static_cast<DeviceProgrammer::Step>(step);
    const int row = m_programmer->stepSequence().indexOf(s);
    if (row >= 0 && row < m_steps->count()) {
        m_steps->item(row)->setText(QStringLiteral("▶  ") + description);
        m_steps->scrollToItem(m_steps->item(row));
    }
    m_status->setText(description);
}

void ProgramDialog::onStepCompleted(int step)
{
    if (!m_programmer) return;
    const auto s = static_cast<DeviceProgrammer::Step>(step);
    const int row = m_programmer->stepSequence().indexOf(s);
    if (row >= 0 && row < m_steps->count())
        m_steps->item(row)->setText(QStringLiteral("✓  ") + DeviceProgrammer::stepLabel(s));
}

void ProgramDialog::onProgressUpdated(int percent)
{
    m_progress->setValue(percent);
}

void ProgramDialog::onWarning(const QString &message)
{
    const QString existing = m_warning->text();
    m_warning->setText(existing.isEmpty() ? message : existing + QStringLiteral("\n\n") + message);
    m_warning->setVisible(true);
}

void ProgramDialog::onFinished(bool success, const QString &message)
{
    m_status->setText(message);
    m_closeButton->setEnabled(true);
    m_closeButton->setText(tr("Schließen"));
    if (!success) {
        for (int i = 0; i < m_steps->count(); ++i) {
            if (m_steps->item(i)->text().startsWith(QLatin1Char('▶'))) {
                m_steps->item(i)->setText(
                    QStringLiteral("✗  ") + m_steps->item(i)->text().mid(3));
            }
        }
        m_header->setStyleSheet(QStringLiteral("font-weight: bold; color: #c0392b;"));
        m_header->setText(tr("Programmierung fehlgeschlagen"));
    } else {
        m_header->setStyleSheet(QStringLiteral("font-weight: bold; color: #27ae60;"));
        m_header->setText(tr("Programmierung abgeschlossen"));
    }
}
