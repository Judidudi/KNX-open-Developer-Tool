#include "BatchProgramDialog.h"
#include "DeviceInstance.h"
#include "KnxprodCatalog.h"
#include "DeviceProgrammer.h"
#include "IKnxInterface.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QMessageBox>

BatchProgramDialog::BatchProgramDialog(QList<DeviceInstance *> devices,
                                       IKnxInterface           *iface,
                                       KnxprodCatalog          *catalog,
                                       QWidget                 *parent)
    : QDialog(parent)
    , m_devices(std::move(devices))
    , m_iface(iface)
    , m_catalog(catalog)
{
    setWindowTitle(tr("Mehrere Geräte programmieren"));
    setMinimumWidth(480);

    auto *layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(tr("%n Gerät(e) ausgewählt:", nullptr, m_devices.size()), this));

    m_list = new QListWidget(this);
    for (const DeviceInstance *dev : m_devices) {
        const QString label = tr("○  %1  –  %2").arg(dev->physicalAddress(), dev->productRefId());
        m_list->addItem(label);
    }
    layout->addWidget(m_list);

    m_overallBar = new QProgressBar(this);
    m_overallBar->setRange(0, m_devices.size());
    m_overallBar->setValue(0);
    layout->addWidget(m_overallBar);

    m_statusLbl = new QLabel(tr("Bereit."), this);
    layout->addWidget(m_statusLbl);

    auto *btnRow = new QHBoxLayout;
    m_startBtn = new QPushButton(tr("Starten"), this);
    m_closeBtn = new QPushButton(tr("Schließen"), this);
    m_closeBtn->setEnabled(false);
    btnRow->addStretch();
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_closeBtn);
    layout->addLayout(btnRow);

    connect(m_startBtn, &QPushButton::clicked, this, &BatchProgramDialog::onStartClicked);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void BatchProgramDialog::onStartClicked()
{
    m_startBtn->setEnabled(false);
    m_current = -1;
    m_overallBar->setValue(0);
    programNext();
}

void BatchProgramDialog::onCancelClicked()
{
    if (m_programmer)
        m_programmer->cancel();
}

void BatchProgramDialog::programNext()
{
    ++m_current;
    if (m_current >= m_devices.size()) {
        m_statusLbl->setText(tr("Alle Geräte wurden programmiert."));
        m_closeBtn->setEnabled(true);
        return;
    }

    DeviceInstance *dev = m_devices[m_current];

    // Resolve app program if not already loaded
    if (!dev->appProgram())
        dev->setAppProgram(m_catalog->sharedByProductRef(dev->productRefId()));
    if (!dev->appProgram()) {
        m_list->item(m_current)->setText(
            tr("✗  %1  –  Anwendungsprogramm nicht gefunden").arg(dev->physicalAddress()));
        m_list->item(m_current)->setForeground(Qt::red);
        m_overallBar->setValue(m_current + 1);
        programNext();
        return;
    }

    m_list->item(m_current)->setText(
        tr("⟳  %1  –  %2  (läuft…)").arg(dev->physicalAddress(), dev->productRefId()));
    m_list->scrollToItem(m_list->item(m_current));
    m_statusLbl->setText(tr("Programmiere %1 (%2/%3)…")
        .arg(dev->physicalAddress())
        .arg(m_current + 1)
        .arg(m_devices.size()));

    delete m_programmer;
    m_programmer = new DeviceProgrammer(m_iface, dev, dev->appProgram(), this);
    connect(m_programmer, &DeviceProgrammer::finished,
            this, &BatchProgramDialog::onCurrentDeviceFinished);
    m_programmer->start();
}

void BatchProgramDialog::onCurrentDeviceFinished(bool success, const QString &message)
{
    const DeviceInstance *dev = m_devices[m_current];
    QListWidgetItem *item = m_list->item(m_current);
    if (success) {
        item->setText(tr("✓  %1  –  %2").arg(dev->physicalAddress(), dev->productRefId()));
        item->setForeground(QColor(0x2E, 0x7D, 0x32));
    } else {
        item->setText(tr("✗  %1  –  %2  (%3)")
                      .arg(dev->physicalAddress(), dev->productRefId(), message));
        item->setForeground(Qt::red);
    }
    m_overallBar->setValue(m_current + 1);
    programNext();
}
