#include "LineScanRunner.h"
#include "IKnxInterface.h"
#include "CemiFrame.h"

#include <QTimer>

LineScanRunner::LineScanRunner(IKnxInterface *iface, QObject *parent)
    : QObject(parent)
    , m_iface(iface)
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &LineScanRunner::onProbeTimeout);
}

void LineScanRunner::startScan(int area, int line, int maxDevice, int timeoutMs)
{
    if (m_running)
        return;

    m_area       = area;
    m_line       = line;
    m_current    = 1;
    m_maxDevice  = maxDevice;
    m_timeoutMs  = timeoutMs;
    m_running    = true;

    connect(m_iface, &IKnxInterface::cemiFrameReceived,
            this, &LineScanRunner::onCemiReceived, Qt::UniqueConnection);

    probeNext();
}

void LineScanRunner::cancel()
{
    m_running = false;
    m_timer->stop();
    if (m_iface)
        disconnect(m_iface, &IKnxInterface::cemiFrameReceived, this, nullptr);
    emit finished();
}

void LineScanRunner::probeNext()
{
    if (!m_running || m_current > m_maxDevice) {
        m_running = false;
        if (m_iface)
            disconnect(m_iface, &IKnxInterface::cemiFrameReceived, this, nullptr);
        emit finished();
        return;
    }

    emit progress(m_current, m_maxDevice);

    // Build the physical address: area.line.device
    const uint16_t physAddr = static_cast<uint16_t>(
        ((m_area  & 0x0F) << 12) |
        ((m_line  & 0x0F) <<  8) |
        ( m_current & 0xFF));

    m_gotResponse = false;
    m_iface->sendCemiFrame(CemiFrame::buildDeviceDescriptorRead(physAddr));
    m_timer->start(m_timeoutMs);
}

void LineScanRunner::onCemiReceived(const QByteArray &cemi)
{
    if (!m_running || m_gotResponse)
        return;

    const CemiFrame frame = CemiFrame::fromBytes(cemi);
    if (!frame.isDeviceDescriptorResponse())
        return;

    // Check that the response came from the address we're currently probing.
    const uint16_t expected = static_cast<uint16_t>(
        ((m_area    & 0x0F) << 12) |
        ((m_line    & 0x0F) <<  8) |
        ( m_current & 0xFF));

    if (frame.sourceAddress != expected)
        return;

    m_gotResponse = true;
    emit deviceFound(CemiFrame::physAddrToString(frame.sourceAddress));

    // Advance immediately without waiting for the timeout
    m_timer->stop();
    ++m_current;
    probeNext();
}

void LineScanRunner::onProbeTimeout()
{
    // No response within timeout → device not present at this address.
    if (!m_running)
        return;
    ++m_current;
    probeNext();
}
