#include "LineScanDialog.h"
#include "LineScanRunner.h"
#include "InterfaceManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QPushButton>
#include <QProgressBar>
#include <QListWidget>
#include <QLabel>
#include <QDialogButtonBox>

LineScanDialog::LineScanDialog(InterfaceManager *mgr, QWidget *parent)
    : QDialog(parent)
    , m_mgr(mgr)
    , m_areaSpin(new QSpinBox(this))
    , m_lineSpin(new QSpinBox(this))
    , m_maxSpin(new QSpinBox(this))
    , m_timeoutSpin(new QSpinBox(this))
    , m_startBtn(new QPushButton(tr("Scan starten"), this))
    , m_stopBtn(new QPushButton(tr("Abbrechen"), this))
    , m_progress(new QProgressBar(this))
    , m_results(new QListWidget(this))
    , m_statusLbl(new QLabel(tr("Bereit."), this))
{
    setWindowTitle(tr("Leitungsscan"));
    setMinimumSize(420, 480);

    auto *layout = new QVBoxLayout(this);

    // ── Settings ───────────────────────────────────────────────────────────────
    auto *settingsBox = new QGroupBox(tr("Scanbereich"), this);
    auto *form = new QFormLayout(settingsBox);

    m_areaSpin->setRange(1, 15);
    m_areaSpin->setValue(1);
    form->addRow(tr("Bereich:"), m_areaSpin);

    m_lineSpin->setRange(1, 15);
    m_lineSpin->setValue(1);
    form->addRow(tr("Linie:"), m_lineSpin);

    m_maxSpin->setRange(1, 255);
    m_maxSpin->setValue(64);
    form->addRow(tr("Max. Teilnehmer:"), m_maxSpin);

    m_timeoutSpin->setRange(50, 2000);
    m_timeoutSpin->setValue(200);
    m_timeoutSpin->setSuffix(tr(" ms"));
    form->addRow(tr("Timeout pro Adresse:"), m_timeoutSpin);

    layout->addWidget(settingsBox);

    // ── Buttons ────────────────────────────────────────────────────────────────
    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(m_startBtn);
    m_stopBtn->setEnabled(false);
    btnRow->addWidget(m_stopBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    // ── Progress ───────────────────────────────────────────────────────────────
    m_progress->setValue(0);
    m_progress->setTextVisible(true);
    layout->addWidget(m_progress);

    layout->addWidget(m_statusLbl);

    // ── Results ────────────────────────────────────────────────────────────────
    auto *resultsBox = new QGroupBox(tr("Gefundene Teilnehmer"), this);
    auto *resLayout  = new QVBoxLayout(resultsBox);
    resLayout->addWidget(m_results);
    layout->addWidget(resultsBox, 1);

    // ── Close button ───────────────────────────────────────────────────────────
    auto *bbox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(bbox);

    connect(m_startBtn, &QPushButton::clicked, this, &LineScanDialog::onStartClicked);
    connect(m_stopBtn,  &QPushButton::clicked, this, [this](){
        if (m_runner) m_runner->cancel();
    });
}

LineScanDialog::~LineScanDialog()
{
    if (m_runner && m_runner->isRunning())
        m_runner->cancel();
}

void LineScanDialog::onStartClicked()
{
    if (!m_mgr || !m_mgr->activeInterface())
        return;

    m_found.clear();
    m_results->clear();
    m_progress->setValue(0);
    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_statusLbl->setText(tr("Scan läuft…"));

    // Delete any previous runner
    delete m_runner;
    m_runner = new LineScanRunner(m_mgr->activeInterface(), this);
    connect(m_runner, &LineScanRunner::deviceFound, this, &LineScanDialog::onDeviceFound);
    connect(m_runner, &LineScanRunner::progress,    this, &LineScanDialog::onProgress);
    connect(m_runner, &LineScanRunner::finished,    this, &LineScanDialog::onScanFinished);

    m_runner->startScan(m_areaSpin->value(), m_lineSpin->value(),
                        m_maxSpin->value(), m_timeoutSpin->value());
}

void LineScanDialog::onDeviceFound(const QString &physAddr)
{
    m_found.append(physAddr);
    m_results->addItem(physAddr);
    m_statusLbl->setText(tr("%1 Teilnehmer gefunden…").arg(m_found.size()));
}

void LineScanDialog::onProgress(int current, int total)
{
    m_progress->setMaximum(total);
    m_progress->setValue(current);
}

void LineScanDialog::onScanFinished()
{
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_progress->setValue(m_progress->maximum());
    m_statusLbl->setText(tr("Scan abgeschlossen — %1 Teilnehmer gefunden.")
                            .arg(m_found.size()));
}
