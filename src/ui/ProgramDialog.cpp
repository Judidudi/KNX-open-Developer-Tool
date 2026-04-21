#include "ProgramDialog.h"
#include "DeviceProgrammer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QListWidget>
#include <QPushButton>
#include <QDialogButtonBox>

ProgramDialog::ProgramDialog(DeviceProgrammer *programmer, QWidget *parent)
    : QDialog(parent)
    , m_programmer(programmer)
{
    setWindowTitle(tr("Gerät programmieren"));
    setMinimumSize(480, 360);

    auto *layout = new QVBoxLayout(this);

    m_header = new QLabel(tr("Gerät wird programmiert. Bitte Programmiertaster drücken."), this);
    m_header->setWordWrap(true);
    m_header->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(m_header);

    m_steps = new QListWidget(this);
    // Pre-populate step list; status icons are updated as we progress
    for (int s = DeviceProgrammer::StepWaitProgMode; s <= DeviceProgrammer::StepDone; ++s)
        m_steps->addItem(QStringLiteral("◯  ") + DeviceProgrammer::stepLabel(
            static_cast<DeviceProgrammer::Step>(s)));
    layout->addWidget(m_steps);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    layout->addWidget(m_progress);

    m_status = new QLabel(tr("Bereit"), this);
    layout->addWidget(m_status);

    auto *btns = new QHBoxLayout;
    m_startButton = new QPushButton(tr("Programmieren starten"), this);
    m_closeButton = new QPushButton(tr("Schließen"), this);
    m_closeButton->setEnabled(false);
    btns->addStretch();
    btns->addWidget(m_startButton);
    btns->addWidget(m_closeButton);
    layout->addLayout(btns);

    connect(m_startButton, &QPushButton::clicked, this, &ProgramDialog::onStartClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &ProgramDialog::onCancelClicked);

    if (m_programmer) {
        connect(m_programmer, &DeviceProgrammer::stepStarted,
                this, &ProgramDialog::onStepStarted);
        connect(m_programmer, &DeviceProgrammer::stepCompleted,
                this, &ProgramDialog::onStepCompleted);
        connect(m_programmer, &DeviceProgrammer::progressUpdated,
                this, &ProgramDialog::onProgressUpdated);
        connect(m_programmer, &DeviceProgrammer::finished,
                this, &ProgramDialog::onFinished);
    }
}

void ProgramDialog::onStartClicked()
{
    if (!m_programmer)
        return;
    m_startButton->setEnabled(false);
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
    if (step >= 0 && step < m_steps->count()) {
        m_steps->item(step)->setText(QStringLiteral("▶  ") + DeviceProgrammer::stepLabel(
            static_cast<DeviceProgrammer::Step>(step)));
    }
    m_status->setText(description);
}

void ProgramDialog::onStepCompleted(int step)
{
    if (step >= 0 && step < m_steps->count()) {
        m_steps->item(step)->setText(QStringLiteral("✓  ") + DeviceProgrammer::stepLabel(
            static_cast<DeviceProgrammer::Step>(step)));
    }
}

void ProgramDialog::onProgressUpdated(int percent)
{
    m_progress->setValue(percent);
}

void ProgramDialog::onFinished(bool success, const QString &message)
{
    m_status->setText(message);
    m_closeButton->setEnabled(true);
    m_closeButton->setText(tr("Schließen"));
    if (!success) {
        m_header->setStyleSheet(QStringLiteral("font-weight: bold; color: #c0392b;"));
        m_header->setText(tr("Programmierung fehlgeschlagen"));
    } else {
        m_header->setStyleSheet(QStringLiteral("font-weight: bold; color: #27ae60;"));
        m_header->setText(tr("Programmierung abgeschlossen"));
    }
}
