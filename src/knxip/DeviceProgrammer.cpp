#include "DeviceProgrammer.h"

#include "IKnxInterface.h"
#include "CemiFrame.h"
#include "TableBuilder.h"
#include "../core/DeviceInstance.h"
#include "../core/Manifest.h"

#include <QTimer>

DeviceProgrammer::DeviceProgrammer(IKnxInterface *iface,
                                   DeviceInstance *device,
                                   const Manifest *manifest,
                                   QObject *parent)
    : QObject(parent)
    , m_iface(iface)
    , m_device(device)
    , m_manifest(manifest)
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &DeviceProgrammer::runNextStep);
}

QString DeviceProgrammer::stepLabel(Step s)
{
    switch (s) {
    case StepWaitProgMode:          return tr("Auf Programmiermodus warten");
    case StepWritePhysAddress:      return tr("Physikalische Adresse schreiben");
    case StepWriteAddressTable:     return tr("Adresstabelle schreiben");
    case StepWriteAssociationTable: return tr("Assoziationstabelle schreiben");
    case StepWriteParameters:       return tr("Parameter schreiben");
    case StepRestart:               return tr("Gerät neu starten");
    case StepDone:                  return tr("Fertig");
    }
    return {};
}

void DeviceProgrammer::start()
{
    if (m_running)
        return;
    if (!m_iface || !m_device || !m_manifest) {
        emit finished(false, tr("Ungültiger Programmer-Zustand"));
        return;
    }
    if (!m_iface->isConnected()) {
        emit finished(false, tr("Kein aktives KNX-Interface verbunden"));
        return;
    }
    m_running = true;
    m_step    = StepWaitProgMode;
    emit stepStarted(m_step,
                     tr("Bitte Programmiertaster am Gerät drücken (Prog-LED muss leuchten)."));
    // Give user 5 s to press the prog button before we start writing
    m_timer->start(5000);
}

void DeviceProgrammer::cancel()
{
    m_running = false;
    m_timer->stop();
    emit finished(false, tr("Programmierung abgebrochen."));
}

void DeviceProgrammer::sendAndAdvance(const QByteArray &frame, int delayMs)
{
    m_iface->sendCemiFrame(frame);
    emit stepCompleted(m_step);
    ++m_step;
    const int totalSteps = StepDone;
    emit progressUpdated(m_step * 100 / totalSteps);
    m_timer->start(delayMs);
}

void DeviceProgrammer::runNextStep()
{
    if (!m_running)
        return;

    const uint16_t newPa = CemiFrame::physAddrFromString(m_device->physicalAddress());
    const DeviceMemoryImage img = TableBuilder::build(*m_device, *m_manifest);

    switch (m_step) {
    case StepWaitProgMode: {
        m_step = StepWritePhysAddress;
        emit stepStarted(m_step, stepLabel(static_cast<Step>(m_step)));
        sendAndAdvance(CemiFrame::buildIndividualAddressWrite(newPa));
        break;
    }
    case StepWriteAddressTable: {
        emit stepStarted(m_step, stepLabel(static_cast<Step>(m_step)));
        sendAndAdvance(CemiFrame::buildMemoryWrite(
            newPa, m_manifest->memoryLayout.addressTable, img.addressTable));
        break;
    }
    case StepWriteAssociationTable: {
        emit stepStarted(m_step, stepLabel(static_cast<Step>(m_step)));
        sendAndAdvance(CemiFrame::buildMemoryWrite(
            newPa, m_manifest->memoryLayout.associationTable, img.associationTable));
        break;
    }
    case StepWriteParameters: {
        emit stepStarted(m_step, stepLabel(static_cast<Step>(m_step)));
        sendAndAdvance(CemiFrame::buildMemoryWrite(
            newPa, m_manifest->memoryLayout.parameterBase, img.parameterBlock));
        break;
    }
    case StepRestart: {
        emit stepStarted(m_step, stepLabel(static_cast<Step>(m_step)));
        sendAndAdvance(CemiFrame::buildRestart(newPa), 1000);
        break;
    }
    case StepDone: {
        m_running = false;
        emit progressUpdated(100);
        emit finished(true, tr("Programmierung erfolgreich abgeschlossen."));
        break;
    }
    default:
        m_running = false;
        emit finished(false, tr("Unbekannter Schritt: %1").arg(m_step));
        break;
    }
}
