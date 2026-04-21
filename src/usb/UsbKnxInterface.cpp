#include "UsbKnxInterface.h"

#ifndef KNXODT_NO_SERIAL
#  include <QSerialPort>
#  include <QSerialPortInfo>
#endif

#ifdef Q_OS_LINUX
#  include <QTimer>
#  include <fcntl.h>
#  include <unistd.h>
#  include <poll.h>
#endif

#include <QDir>
#include <algorithm>
#include <cstring>

// ---- Serial framing ---------------------------------------------------------
// Start byte 0xAB is not a valid cEMI message code (valid: 0x11/0x2E/0x29).
static constexpr uint8_t SERIAL_START     = 0xAB;

// ---- KNX USB HID (spec 07_01_01) -------------------------------------------
static constexpr int    HID_REPORT_SIZE   = 64;
static constexpr uint8_t HID_REPORT_ID    = 0x01;

// HID Packet Info – lower nibble is the protocol/transport type:
//   0x00 = KNX USB management (Device Feature Get/Set)
//   0x01 = EMI1
//   0x02 = EMI2
//   0x03 = cEMI
static constexpr uint8_t HID_PROTO_MGMT  = 0x00;
static constexpr uint8_t HID_PROTO_EMI2  = 0x02;
static constexpr uint8_t HID_PROTO_CEMI  = 0x03;

// KNX USB management service types (big-endian 16-bit in body bytes 0-1)
static constexpr uint8_t SVC_FEAT_GET_REQ  = 0x30; // 0x0530
static constexpr uint8_t SVC_FEAT_GET_RESP = 0x31; // 0x0531
static constexpr uint8_t SVC_FEAT_SET_REQ  = 0x32; // 0x0532
static constexpr uint8_t SVC_FEAT_SET_RESP = 0x33; // 0x0533
static constexpr uint8_t SVC_PREFIX        = 0x05;

// Feature IDs
static constexpr uint8_t FEAT_SUPPORTED_EMI = 0x01; // bitmap: bit0=EMI1 bit1=EMI2 bit2=cEMI
static constexpr uint8_t FEAT_CURRENT_EMI   = 0x02; // value: 0x01/0x02/0x03

// Timeout for HID feature negotiation reads
static constexpr int HID_INIT_TIMEOUT_MS = 1000;

// ---- Frame conversion helpers -----------------------------------------------
// cEMI: [msgCode][addInfoLen][addInfo...][ctrl1][ctrl2][src][dst][apduLen][apdu...]
// EMI2: [msgCode][ctrl1][ctrl2][src][dst][apduLen][apdu...]  (no addInfo section)

static QByteArray emi2ToCemi(const QByteArray &emi2)
{
    if (emi2.isEmpty()) return emi2;
    QByteArray cemi;
    cemi.reserve(emi2.size() + 1);
    cemi.append(emi2[0]);       // message code
    cemi.append(char(0x00));    // addInfoLen = 0  (inserted)
    cemi.append(emi2.mid(1));   // ctrl1, ctrl2, addresses, APDU
    return cemi;
}

static QByteArray cemiToEmi2(const QByteArray &cemi)
{
    if (cemi.size() < 3) return cemi;
    const int addLen = static_cast<uint8_t>(cemi[1]);
    QByteArray emi2;
    emi2.reserve(cemi.size() - 1 - addLen);
    emi2.append(cemi[0]);                  // message code
    emi2.append(cemi.mid(2 + addLen));     // skip addInfoLen + addInfo bytes
    return emi2;
}

// ---- Priv -------------------------------------------------------------------

struct UsbKnxInterface::Priv
{
    UsbKnxInterface *q;
    Transport    transport;
    QString      devicePath;
    bool         connected          = false;
    uint8_t      hidEmiType         = HID_PROTO_EMI2; // updated by negotiation or auto-detect
    bool         hidNumberedReports = true;            // true = report ID byte present
    QByteArray   recvBuf;

#ifndef KNXODT_NO_SERIAL
    QSerialPort *serial = nullptr;
#endif

#ifdef Q_OS_LINUX
    int     hidFd        = -1;
    QTimer *hidPollTimer = nullptr;  // replaces QSocketNotifier for reliability
#endif

    explicit Priv(UsbKnxInterface *owner, Transport t, const QString &path)
        : q(owner), transport(t), devicePath(path)
    {}

    // ------------------------------------------------------------------
    // Serial
    // ------------------------------------------------------------------

    bool openSerial()
    {
#ifndef KNXODT_NO_SERIAL
        if (devicePath.isEmpty()) {
            emit q->errorOccurred(QObject::tr("USB-Seriell: Kein Gerätepfad angegeben."));
            return false;
        }
        if (!serial) {
            serial = new QSerialPort(q);
            QObject::connect(serial, &QSerialPort::readyRead,
                             q, [this]() { onSerialRead(); });
            QObject::connect(serial, &QSerialPort::errorOccurred, q,
                [this](QSerialPort::SerialPortError e) {
                    if (e != QSerialPort::NoError) {
                        emit q->errorOccurred(
                            QObject::tr("Serieller Fehler: %1").arg(serial->errorString()));
                        if (connected) {
                            connected = false;
                            emit q->disconnected();
                        }
                    }
                });
        }
        serial->setPortName(devicePath);
        serial->setBaudRate(QSerialPort::Baud115200);
        serial->setDataBits(QSerialPort::Data8);
        serial->setParity(QSerialPort::NoParity);
        serial->setStopBits(QSerialPort::OneStop);
        serial->setFlowControl(QSerialPort::NoFlowControl);
        if (!serial->open(QIODevice::ReadWrite)) {
            emit q->errorOccurred(
                QObject::tr("Konnte %1 nicht öffnen: %2").arg(devicePath, serial->errorString()));
            return false;
        }
        connected = true;
        recvBuf.clear();
        emit q->connected();
        return true;
#else
        emit q->errorOccurred(
            QObject::tr("USB-Seriell-Interface wurde ohne Qt6::SerialPort gebaut."));
        return false;
#endif
    }

    void closeSerial()
    {
#ifndef KNXODT_NO_SERIAL
        if (serial && serial->isOpen())
            serial->close();
#endif
    }

    // ------------------------------------------------------------------
    // HID
    // ------------------------------------------------------------------

    bool openHid()
    {
#ifdef Q_OS_LINUX
        if (devicePath.isEmpty()) {
            emit q->errorOccurred(QObject::tr("USB-HID: Kein Gerätepfad angegeben."));
            return false;
        }
        hidFd = ::open(devicePath.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK);
        if (hidFd < 0) {
            emit q->errorOccurred(
                QObject::tr("Konnte %1 nicht öffnen (kein Zugriff? udev-Regel prüfen).")
                    .arg(devicePath));
            return false;
        }

        // Negotiate EMI protocol type via Device Feature Get/Set (spec 07_01_01).
        // Falls back to cEMI if the device does not respond to management frames
        // (some devices work without explicit negotiation).
        if (!negotiateHidProtocol()) {
            // Many older devices (e.g. Hager, Siemens) don't implement the
            // Device Feature management protocol but work fine with EMI2.
            // We keep hidEmiType = HID_PROTO_EMI2 and auto-detect the actual
            // protocol from the first received frame.
            emit q->errorOccurred(
                QObject::tr("USB-HID %1: Protokollaushandlung fehlgeschlagen – "
                            "EMI2 wird als Fallback verwendet, "
                            "Protokoll wird automatisch erkannt.").arg(devicePath));
        }

        // Poll every 5 ms instead of QSocketNotifier: hidraw fds are not
        // reliably selectable on all kernel/distro combinations.
        // Each poll attempt costs one ::read() syscall (returns EAGAIN
        // immediately when no data is available – negligible CPU overhead).
        hidPollTimer = new QTimer(q);
        hidPollTimer->setInterval(5);
        QObject::connect(hidPollTimer, &QTimer::timeout, q, [this]() {
            // Drain all buffered reports in one timer tick
            for (int i = 0; i < 16; ++i) {
                if (!onHidRead()) break;
            }
        });
        hidPollTimer->start();

        connected = true;
        emit q->connected();
        return true;
#else
        emit q->errorOccurred(QObject::tr("USB-HID-Interface ist nur unter Linux verfügbar."));
        return false;
#endif
    }

#ifdef Q_OS_LINUX
    // Wait up to timeoutMs for a complete HID report. Returns false on timeout/error.
    bool hidPollRead(uint8_t *buf, int timeoutMs)
    {
        struct pollfd pfd { hidFd, POLLIN, 0 };
        if (::poll(&pfd, 1, timeoutMs) <= 0) return false;
        return ::read(hidFd, buf, HID_REPORT_SIZE) == HID_REPORT_SIZE;
    }

    // Send a KNX USB management request and read back the response.
    // body: bytes after the 2-byte service type prefix (0x05 XX).
    bool hidMgmtRequest(uint8_t svcByte, const QByteArray &body,
                        uint8_t expectedRespSvc, QByteArray &respBody)
    {
        uint8_t req[HID_REPORT_SIZE] = {};
        req[0] = HID_REPORT_ID;
        req[1] = HID_PROTO_MGMT;
        req[2] = static_cast<uint8_t>(2 + body.size()); // service type (2) + body
        req[3] = SVC_PREFIX;
        req[4] = svcByte;
        std::memcpy(req + 5, body.constData(), static_cast<size_t>(body.size()));

        if (::write(hidFd, req, HID_REPORT_SIZE) != HID_REPORT_SIZE)
            return false;

        uint8_t resp[HID_REPORT_SIZE] = {};
        if (!hidPollRead(resp, HID_INIT_TIMEOUT_MS))
            return false;
        if (resp[0] != HID_REPORT_ID || (resp[1] & 0x0F) != HID_PROTO_MGMT)
            return false;
        if (resp[3] != SVC_PREFIX || resp[4] != expectedRespSvc)
            return false;

        const int dataLen = resp[2];
        if (dataLen < 2) return false;
        respBody = QByteArray(reinterpret_cast<const char *>(resp + 5), dataLen - 2);
        return true;
    }

    bool negotiateHidProtocol()
    {
        // Step 1: query supported EMI types
        QByteArray resp;
        const QByteArray getReq(1, static_cast<char>(FEAT_SUPPORTED_EMI));
        if (!hidMgmtRequest(SVC_FEAT_GET_REQ, getReq, SVC_FEAT_GET_RESP, resp))
            return false;

        // resp[0] = feature id, resp[1] = supported bitmap
        if (resp.size() < 2 || static_cast<uint8_t>(resp[0]) != FEAT_SUPPORTED_EMI)
            return false;

        const uint8_t supported = static_cast<uint8_t>(resp[1]);
        // bit2 = cEMI, bit1 = EMI2, bit0 = EMI1
        uint8_t chosen;
        if      (supported & 0x04) chosen = HID_PROTO_CEMI;
        else if (supported & 0x02) chosen = HID_PROTO_EMI2;
        else return false; // only EMI1 – not supported by this tool

        // Step 2: set chosen EMI type
        QByteArray setReq;
        setReq.append(static_cast<char>(FEAT_CURRENT_EMI));
        setReq.append(static_cast<char>(chosen));
        QByteArray setResp;
        if (!hidMgmtRequest(SVC_FEAT_SET_REQ, setReq, SVC_FEAT_SET_RESP, setResp))
            return false;

        hidEmiType = chosen;
        return true;
    }

    // Returns true if a report was read, false if no data was available (EAGAIN).
    bool onHidRead()
    {
        uint8_t report[HID_REPORT_SIZE] = {};
        const ssize_t n = ::read(hidFd, report, HID_REPORT_SIZE);
        if (n <= 0) return false;  // EAGAIN or error
        if (n < 2)  return true;   // too short to be useful, but consumed

        // KNX USB HID spec 07_01_01 defines numbered reports (ID = 0x01).
        // Some devices (Hager, older Siemens) use unnumbered reports where
        // the report ID byte is absent from the hidraw stream.
        // Detect format once from the first received frame and remember it.
        if (report[0] == HID_REPORT_ID && n >= 3)
            hidNumberedReports = true;
        else if (report[0] != HID_REPORT_ID)
            hidNumberedReports = false;

        // Locate Packet Info and data length based on format:
        //   Numbered:   [0x01][PacketInfo][DataLen][Data...]
        //   Unnumbered: [PacketInfo][DataLen][Data...]
        const int infoOffset = hidNumberedReports ? 1 : 0;
        const int dataOffset = infoOffset + 2;
        if (n < dataOffset) return true;

        const uint8_t packetInfo = report[infoOffset];
        const uint8_t protoType  = packetInfo & 0x0F;
        const int     dataLen    = static_cast<int>(static_cast<uint8_t>(report[infoOffset + 1]));

        if (dataLen <= 0 || n < dataOffset + dataLen) return true;
        if (protoType == HID_PROTO_MGMT) return true;

        // Auto-update EMI type so sendHid() uses the correct format
        if (protoType == HID_PROTO_EMI2 || protoType == HID_PROTO_CEMI)
            hidEmiType = protoType;

        const QByteArray frame(reinterpret_cast<const char *>(report + dataOffset), dataLen);
        const QByteArray cemi = (protoType == HID_PROTO_EMI2) ? emi2ToCemi(frame) : frame;
        if (!cemi.isEmpty())
            emit q->cemiFrameReceived(cemi);
        return true;
    }
#endif

    void closeHid()
    {
#ifdef Q_OS_LINUX
        if (hidPollTimer) {
            hidPollTimer->stop();
            delete hidPollTimer;
            hidPollTimer = nullptr;
        }
        if (hidFd >= 0) {
            ::close(hidFd);
            hidFd = -1;
        }
#endif
    }

    void sendHid(const QByteArray &cemi)
    {
#ifdef Q_OS_LINUX
        if (hidFd < 0) return;

        const QByteArray payload = (hidEmiType == HID_PROTO_EMI2)
                                   ? cemiToEmi2(cemi) : cemi;

        uint8_t report[HID_REPORT_SIZE] = {};
        // Use the same report format (numbered/unnumbered) as the device sends
        const int infoOffset = hidNumberedReports ? 1 : 0;
        if (hidNumberedReports)
            report[0] = HID_REPORT_ID;
        report[infoOffset]     = hidEmiType;
        const int maxPayload   = HID_REPORT_SIZE - infoOffset - 2;
        const int len          = std::min<int>(static_cast<int>(payload.size()), maxPayload);
        report[infoOffset + 1] = static_cast<uint8_t>(len);
        std::memcpy(report + infoOffset + 2, payload.constData(), static_cast<size_t>(len));
        ::write(hidFd, report, HID_REPORT_SIZE);
#else
        Q_UNUSED(cemi)
#endif
    }

    // ------------------------------------------------------------------
    // Serial receive
    // ------------------------------------------------------------------

    void onSerialRead()
    {
#ifndef KNXODT_NO_SERIAL
        if (!serial) return;
        recvBuf.append(serial->readAll());
        parseSerialBuffer();
#endif
    }

    void parseSerialBuffer()
    {
        // Format: [0xAB][len_hi][len_lo][cemi_bytes…]
        // Resync by scanning for the 0xAB start byte.
        while (recvBuf.size() >= 3) {
            int start = -1;
            for (int i = 0; i < recvBuf.size(); ++i) {
                if (static_cast<uint8_t>(recvBuf[i]) == SERIAL_START) {
                    start = i;
                    break;
                }
            }
            if (start < 0) { recvBuf.clear(); return; }
            if (start > 0)  recvBuf.remove(0, start);
            if (recvBuf.size() < 3) return;

            const int payloadLen =
                (static_cast<uint8_t>(recvBuf[1]) << 8) |
                 static_cast<uint8_t>(recvBuf[2]);
            if (recvBuf.size() < 3 + payloadLen) return;

            const QByteArray cemi = recvBuf.mid(3, payloadLen);
            recvBuf.remove(0, 3 + payloadLen);
            emit q->cemiFrameReceived(cemi);
        }
    }

    void sendSerial(const QByteArray &cemi)
    {
#ifndef KNXODT_NO_SERIAL
        if (!serial || !serial->isOpen()) return;
        QByteArray frame;
        frame.reserve(3 + cemi.size());
        frame.append(static_cast<char>(SERIAL_START));
        frame.append(static_cast<char>((cemi.size() >> 8) & 0xFF));
        frame.append(static_cast<char>( cemi.size()       & 0xFF));
        frame.append(cemi);
        serial->write(frame);
#endif
    }
};

// ---- UsbKnxInterface --------------------------------------------------------

UsbKnxInterface::UsbKnxInterface(Transport transport, const QString &devicePath, QObject *parent)
    : IKnxInterface(parent)
    , d(std::make_unique<Priv>(this, transport, devicePath))
{}

UsbKnxInterface::~UsbKnxInterface()
{
    disconnectFromInterface();
}

void UsbKnxInterface::setDevicePath(const QString &path) { d->devicePath = path; }
void UsbKnxInterface::setTransport(Transport t)           { d->transport  = t;    }
UsbKnxInterface::Transport UsbKnxInterface::transport() const { return d->transport; }
QString   UsbKnxInterface::devicePath()  const { return d->devicePath;  }
bool      UsbKnxInterface::isConnected() const { return d->connected;   }

bool UsbKnxInterface::connectToInterface()
{
    if (d->connected) return true;
    return (d->transport == Transport::Serial) ? d->openSerial() : d->openHid();
}

void UsbKnxInterface::disconnectFromInterface()
{
    if (!d->connected) return;
    d->connected = false;
    if (d->transport == Transport::Serial) d->closeSerial();
    else                                   d->closeHid();
    emit disconnected();
}

void UsbKnxInterface::sendCemiFrame(const QByteArray &cemi)
{
    if (!d->connected) {
        emit errorOccurred(tr("USB-Interface: Nicht verbunden."));
        return;
    }
    if (d->transport == Transport::Serial) d->sendSerial(cemi);
    else                                   d->sendHid(cemi);
}

QStringList UsbKnxInterface::availableSerialPorts()
{
    QStringList result;
#ifndef KNXODT_NO_SERIAL
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        if (!info.isNull())
            result << info.systemLocation();
#endif
    return result;
}

QStringList UsbKnxInterface::availableHidDevices()
{
    QStringList result;
#ifdef Q_OS_LINUX
    const QDir dev(QStringLiteral("/dev"));
    const QStringList entries = dev.entryList({QStringLiteral("hidraw*")},
                                               QDir::System | QDir::CaseSensitive);
    for (const QString &e : entries)
        result << dev.absoluteFilePath(e);
#endif
    return result;
}
