#include "DeviceDiagnosticWidget.h"

#include "DeviceInstance.h"
#include "InterfaceManager.h"
#include "IKnxInterface.h"
#include "TransportConnection.h"
#include "CemiFrame.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

// ── Decoder helpers ───────────────────────────────────────────────────────────

QString DeviceDiagnosticWidget::decodeHex(const QByteArray &d)
{
    return d.toHex(':'). toUpper();
}

QString DeviceDiagnosticWidget::decodeUInt8(const QByteArray &d)
{
    if (d.isEmpty()) return QStringLiteral("–");
    return QString::number(static_cast<uint8_t>(d[0]));
}

QString DeviceDiagnosticWidget::decodeUInt16(const QByteArray &d)
{
    if (d.size() < 2) return QStringLiteral("–");
    const uint16_t v = (static_cast<uint8_t>(d[0]) << 8) | static_cast<uint8_t>(d[1]);
    return QStringLiteral("0x%1").arg(v, 4, 16, QLatin1Char('0')).toUpper();
}

QString DeviceDiagnosticWidget::decodeUInt16Bytes(const QByteArray &d)
{
    if (d.size() < 2) return QStringLiteral("–");
    const uint16_t v = (static_cast<uint8_t>(d[0]) << 8) | static_cast<uint8_t>(d[1]);
    return QStringLiteral("%1 Bytes").arg(v);
}

QString DeviceDiagnosticWidget::decodeSerial(const QByteArray &d)
{
    if (d.size() < 6) return decodeHex(d);
    QString s;
    for (int i = 0; i < 6; ++i) {
        if (i) s += QLatin1Char(':');
        s += QStringLiteral("%1").arg(static_cast<uint8_t>(d[i]), 2, 16, QLatin1Char('0')).toUpper();
    }
    return s;
}

QString DeviceDiagnosticWidget::decodeManufId(const QByteArray &d)
{
    if (d.size() < 2) return QStringLiteral("–");
    const uint16_t v = (static_cast<uint8_t>(d[0]) << 8) | static_cast<uint8_t>(d[1]);
    return QStringLiteral("M-%1").arg(v, 4, 16, QLatin1Char('0')).toUpper();
}

QString DeviceDiagnosticWidget::decodeBool(const QByteArray &d)
{
    if (d.isEmpty()) return QStringLiteral("–");
    return d[0] ? QObject::tr("An") : QObject::tr("Aus");
}

QString DeviceDiagnosticWidget::decodeLoadState(const QByteArray &d)
{
    if (d.isEmpty()) return QStringLiteral("–");
    switch (static_cast<uint8_t>(d[0])) {
        case 0: return QObject::tr("Nicht geladen");
        case 1: return QObject::tr("Wird geladen");
        case 2: return QObject::tr("Geladen");
        case 3: return QObject::tr("Fehler");
        default: return QStringLiteral("0x%1").arg(static_cast<uint8_t>(d[0]), 2, 16, QLatin1Char('0'));
    }
}

QString DeviceDiagnosticWidget::decodeRunState(const QByteArray &d)
{
    if (d.isEmpty()) return QStringLiteral("–");
    switch (static_cast<uint8_t>(d[0])) {
        case 0: return QObject::tr("Gestoppt");
        case 1: return QObject::tr("Läuft");
        case 2: return QObject::tr("Bereit");
        default: return QStringLiteral("0x%1").arg(static_cast<uint8_t>(d[0]), 2, 16, QLatin1Char('0'));
    }
}

// ── Property catalogue ────────────────────────────────────────────────────────

const QList<DeviceDiagnosticWidget::PropertyDef> DeviceDiagnosticWidget::kProperties = {
    {0,  1,  QT_TR_NOOP("Objekttyp (Device)"),    decodeUInt16},
    {0, 11,  QT_TR_NOOP("Seriennummer"),           decodeSerial},
    {0, 12,  QT_TR_NOOP("Hersteller-ID"),          decodeManufId},
    {0, 13,  QT_TR_NOOP("Firmware-Revision"),      decodeUInt8},
    {0, 54,  QT_TR_NOOP("Programmiermodus"),       decodeBool},
    {0, 56,  QT_TR_NOOP("Max. APDU-Länge"),        decodeUInt16Bytes},
    {1,  5,  QT_TR_NOOP("Ladezustand"),            decodeLoadState},
    {1,  6,  QT_TR_NOOP("Ausführungszustand"),     decodeRunState},
    {1,  7,  QT_TR_NOOP("Download-Zähler"),        decodeUInt16},
};

// ── Widget ────────────────────────────────────────────────────────────────────

DeviceDiagnosticWidget::DeviceDiagnosticWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);

    auto *topRow = new QHBoxLayout;
    m_startBtn = new QPushButton(tr("Diagnose starten"), this);
    m_startBtn->setEnabled(false);
    m_statusLabel = new QLabel(tr("Bereit"), this);
    topRow->addWidget(m_startBtn);
    topRow->addWidget(m_statusLabel);
    topRow->addStretch();
    layout->addLayout(topRow);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({tr("Obj"), tr("Prop"), tr("Name"), tr("Wert")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setColumnWidth(0, 40);
    m_table->setColumnWidth(1, 40);
    m_table->setColumnWidth(2, 200);
    layout->addWidget(m_table);

    m_propTimer = new QTimer(this);
    m_propTimer->setSingleShot(true);
    connect(m_propTimer, &QTimer::timeout, this, &DeviceDiagnosticWidget::onPropertyTimeout);

    connect(m_startBtn, &QPushButton::clicked, this, &DeviceDiagnosticWidget::onStartClicked);

    // Pre-populate table with property names
    m_table->setRowCount(kProperties.size());
    for (int i = 0; i < kProperties.size(); ++i) {
        m_table->setItem(i, 0, new QTableWidgetItem(QString::number(kProperties[i].objIdx)));
        m_table->setItem(i, 1, new QTableWidgetItem(QString::number(kProperties[i].propId)));
        m_table->setItem(i, 2, new QTableWidgetItem(tr(kProperties[i].name.toUtf8())));
        m_table->setItem(i, 3, new QTableWidgetItem(QString()));
    }
}

void DeviceDiagnosticWidget::setDevice(DeviceInstance *dev)
{
    m_device = dev;
    const bool ok = m_device && !m_device->physicalAddress().isEmpty()
                    && m_mgr && m_mgr->isConnected();
    m_startBtn->setEnabled(ok && !m_running);
}

void DeviceDiagnosticWidget::setInterfaceManager(InterfaceManager *mgr)
{
    if (m_mgr == mgr) return;
    if (m_mgr) {
        disconnect(m_mgr, nullptr, this, nullptr);
    }
    m_mgr = mgr;
    if (m_mgr) {
        connect(m_mgr, &InterfaceManager::connected,
                this, &DeviceDiagnosticWidget::onInterfaceConnected);
        connect(m_mgr, &InterfaceManager::disconnected,
                this, &DeviceDiagnosticWidget::onInterfaceDisconnected);
    }
    const bool ok = m_device && !m_device->physicalAddress().isEmpty()
                    && m_mgr && m_mgr->isConnected();
    m_startBtn->setEnabled(ok && !m_running);
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void DeviceDiagnosticWidget::onInterfaceConnected()
{
    const bool ok = m_device && !m_device->physicalAddress().isEmpty();
    m_startBtn->setEnabled(ok && !m_running);
}

void DeviceDiagnosticWidget::onInterfaceDisconnected()
{
    m_startBtn->setEnabled(false);
    if (m_running) {
        cleanup();
        setStatus(tr("Verbindung getrennt"));
    }
}

void DeviceDiagnosticWidget::onStartClicked()
{
    if (!m_mgr || !m_mgr->isConnected() || !m_device) return;

    // Clear previous results
    for (int i = 0; i < kProperties.size(); ++i) {
        auto *item = m_table->item(i, 3);
        if (item) item->setText(tr("⏳"));
    }

    m_propIdx = 0;
    m_running = true;
    m_startBtn->setEnabled(false);
    setStatus(tr("Verbinde…"));

    m_transport = new TransportConnection(m_mgr->activeInterface(), this);
    connect(m_transport, &TransportConnection::opened,
            this, &DeviceDiagnosticWidget::onTransportOpened);
    connect(m_transport, &TransportConnection::apduReceived,
            this, &DeviceDiagnosticWidget::onApduReceived);
    connect(m_transport, &TransportConnection::error,
            this, &DeviceDiagnosticWidget::onTransportError);
    connect(m_transport, &TransportConnection::closed,
            this, &DeviceDiagnosticWidget::onTransportClosed);

    // Forward all incoming CEMI frames so the transport can handle T_ACK / responses
    connect(m_mgr->activeInterface(), &IKnxInterface::cemiFrameReceived,
            this, &DeviceDiagnosticWidget::onCemiReceived);

    const uint16_t pa = CemiFrame::physAddrFromString(m_device->physicalAddress());
    m_transport->open(pa);
}

void DeviceDiagnosticWidget::onTransportOpened()
{
    setStatus(tr("Lese Properties…"));
    readNextProperty();
}

void DeviceDiagnosticWidget::readNextProperty()
{
    if (m_propIdx >= kProperties.size()) {
        m_transport->close();
        return;
    }
    const auto &def = kProperties[m_propIdx];
    setStatus(tr("Lese %1 …").arg(tr(def.name.toUtf8())));
    m_transport->sendPropertyRead(def.objIdx, def.propId, 1, 1);
    m_propTimer->start(3000);
}

void DeviceDiagnosticWidget::onApduReceived(const QByteArray &apdu)
{
    if (apdu.size() < 2) return;

    // Check APCI: PropertyValue_Response = 0x3D6
    const uint16_t apci = ((static_cast<uint8_t>(apdu[0]) & 0x03) << 8)
                          | static_cast<uint8_t>(apdu[1]);
    if (apci != 0x3D6) return;

    m_propTimer->stop();

    uint8_t objIdx = 0, propId = 0, count = 0;
    uint16_t startIdx = 0;
    QByteArray data;

    // Build a minimal CemiFrame to reuse propertyValueResponseData()
    CemiFrame f;
    f.apdu = apdu;
    if (!f.propertyValueResponseData(objIdx, propId, count, startIdx, data)) {
        finishCurrentProperty(QStringLiteral("?"), false);
        return;
    }

    const auto &def = kProperties[m_propIdx];
    if (objIdx != def.objIdx || propId != def.propId) {
        // Response is for a different property — ignore and wait
        return;
    }

    const QString value = (count == 0 || data.isEmpty())
        ? tr("Nicht verfügbar")
        : (def.decode ? def.decode(data) : decodeHex(data));

    finishCurrentProperty(value, count > 0);
}

void DeviceDiagnosticWidget::onPropertyTimeout()
{
    finishCurrentProperty(tr("Nicht verfügbar"), false);
}

void DeviceDiagnosticWidget::finishCurrentProperty(const QString &value, bool ok)
{
    if (m_propIdx < m_table->rowCount()) {
        auto *item = m_table->item(m_propIdx, 3);
        if (item) {
            item->setText(value);
            item->setForeground(ok ? Qt::darkGreen : Qt::darkGray);
        }
    }
    ++m_propIdx;
    readNextProperty();
}

void DeviceDiagnosticWidget::onTransportError(const QString &msg)
{
    m_propTimer->stop();
    cleanup();
    setStatus(tr("Fehler: %1").arg(msg));
}

void DeviceDiagnosticWidget::onTransportClosed()
{
    m_propTimer->stop();
    const bool allDone = (m_propIdx >= kProperties.size());
    cleanup();
    setStatus(allDone ? tr("Abgeschlossen") : tr("Getrennt"));
}

void DeviceDiagnosticWidget::setStatus(const QString &msg)
{
    m_statusLabel->setText(msg);
}

void DeviceDiagnosticWidget::onCemiReceived(const QByteArray &cemi)
{
    if (m_transport)
        m_transport->handleIncoming(cemi);
}

void DeviceDiagnosticWidget::cleanup()
{
    m_running = false;
    if (m_mgr && m_mgr->activeInterface())
        disconnect(m_mgr->activeInterface(), &IKnxInterface::cemiFrameReceived,
                   this, &DeviceDiagnosticWidget::onCemiReceived);
    if (m_transport) {
        m_transport->deleteLater();
        m_transport = nullptr;
    }
    const bool ok = m_device && !m_device->physicalAddress().isEmpty()
                    && m_mgr && m_mgr->isConnected();
    m_startBtn->setEnabled(ok);
}
