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
#  include <cerrno>
#endif

#include <QDir>
#include <QDebug>
#include <algorithm>
#include <cstring>

// ---- Serial framing ---------------------------------------------------------
// Start byte 0xAB is not a valid cEMI message code (valid: 0x11/0x2E/0x29).
static constexpr uint8_t SERIAL_START     = 0xAB;

// ---- KNX USB HID (spec 07_01_01 + 03_06_03 Ch. 4.1) ------------------------
//
// Wire format of a HID report sent/received over /dev/hidrawN:
//   [Numbered] Report ID (0x01)
//   ─── KNX HID Report Header (3 bytes) ───
//   Byte 1: SeqNo (bits 0-3) + PacketType (bits 4-7) — single frame = 0x05
//   Byte 2: PacketLength  (= 8-byte body header + body)
//   ─── KNX HID Report Body Header (8 bytes) ───
//   Byte 3: ProtocolVersion = 0x00
//   Byte 4: HeaderLength    = 0x08
//   Byte 5-6: BodyLength    (BE — bytes after this header)
//   Byte 7: ProtocolID      0x01 = KnxTunnel, 0x0F = BusAccessServerFeature
//   Byte 8: EMI ID          0x01 EMI1, 0x02 EMI2, 0x03 cEMI, 0x0F BusAccess
//   Byte 9-10: ManufacturerCode (BE, 0x0000)
//   ─── Body (EMI frame or BusAccess service request/response) ───

static constexpr int     HID_REPORT_SIZE  = 64;
static constexpr uint8_t HID_REPORT_ID    = 0x01;

// EMI ID / Protocol IDs that may appear in byte 7/8 of the HID body header
static constexpr uint8_t HID_PROTO_EMI1   = 0x01;
static constexpr uint8_t HID_PROTO_EMI2   = 0x02;
static constexpr uint8_t HID_PROTO_CEMI   = 0x03;

// Body header constants
static constexpr uint8_t HID_PROTO_VERSION  = 0x00;
static constexpr uint8_t HID_HEADER_LENGTH  = 0x08;
static constexpr uint8_t HID_PID_KNX_TUNNEL = 0x01;
static constexpr uint8_t HID_PID_BUS_ACCESS = 0x0F;
static constexpr uint8_t HID_BUS_ACCESS_SVC = 0x0F; // EMI-ID byte for Bus Access services

// Packet type for single-frame transfer (most KNX HID frames fit in one report)
static constexpr uint8_t HID_PKT_SINGLE     = 0x05;

// KNX USB management service types (Bus Access Server Feature service)
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
    uint8_t      hidEmiType         = HID_PROTO_CEMI; // updated by negotiation or auto-detect
    bool         hidNumberedReports = true;            // true = report ID byte present
    uint8_t      hidTxSeq           = 0;               // outgoing sequence number (1..15)
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

    // Build a KNX HID report (numbered or unnumbered) and write it to the device.
    // payload = body bytes that come AFTER the 8-byte KNX HID Report Body Header.
    bool hidWriteReport(uint8_t protoId, uint8_t emiId, const QByteArray &payload)
    {
        const int bodyLen = 4 + payload.size();   // ProtoID + EmiID + Mfg(2) + payload

        uint8_t r[HID_REPORT_SIZE] = {};
        int o = 0;
        if (hidNumberedReports) r[o++] = HID_REPORT_ID;
        // SeqNo (low nibble) + PacketType=Single (high nibble)
        const uint8_t seq = static_cast<uint8_t>(((++hidTxSeq) & 0x0F));
        r[o++] = static_cast<uint8_t>((HID_PKT_SINGLE << 4) | seq);
        r[o++] = static_cast<uint8_t>(8 + bodyLen);     // PacketLength
        r[o++] = HID_PROTO_VERSION;                     // 0x00
        r[o++] = HID_HEADER_LENGTH;                     // 0x08
        r[o++] = static_cast<uint8_t>((bodyLen >> 8) & 0xFF);
        r[o++] = static_cast<uint8_t>( bodyLen       & 0xFF);
        r[o++] = protoId;
        r[o++] = emiId;
        r[o++] = 0; r[o++] = 0;                         // ManufacturerCode = 0
        std::memcpy(r + o, payload.constData(),
                    static_cast<size_t>(std::min<int>(payload.size(), HID_REPORT_SIZE - o)));

        const ssize_t written = ::write(hidFd, r, HID_REPORT_SIZE);
        if (written != HID_REPORT_SIZE) {
            qWarning().nospace() << "[UsbKnx] HID-Write unvollständig: wrote=" << written;
            return false;
        }
        return true;
    }

    // Send a KNX USB Bus Access Server Feature management request and read back
    // the response.  body = bytes after the 2-byte service type prefix (0x05 XX).
    bool hidMgmtRequest(uint8_t svcByte, const QByteArray &body,
                        uint8_t expectedRespSvc, QByteArray &respBody)
    {
        QByteArray payload;
        payload.append(static_cast<char>(SVC_PREFIX));
        payload.append(static_cast<char>(svcByte));
        payload.append(body);
        if (!hidWriteReport(HID_PID_BUS_ACCESS, HID_BUS_ACCESS_SVC, payload))
            return false;

        // Read response — may need to skip unrelated bus traffic that arrives
        // while waiting for the management answer.
        const int kTries = 8;
        for (int i = 0; i < kTries; ++i) {
            uint8_t resp[HID_REPORT_SIZE] = {};
            if (!hidPollRead(resp, HID_INIT_TIMEOUT_MS))
                return false;

            const int hdrBase = hidNumberedReports ? 1 : 0;
            // Sanity-check unnumbered detection: if first byte is HID_REPORT_ID
            // the device uses numbered reports (overrides any prior assumption).
            if (resp[0] == HID_REPORT_ID && hdrBase == 0) {
                // Re-evaluate as numbered just for this response
                continue;
            }
            if (HID_REPORT_SIZE < hdrBase + 11)            continue;
            if (resp[hdrBase + 2] != HID_PROTO_VERSION)    continue;
            if (resp[hdrBase + 3] != HID_HEADER_LENGTH)    continue;

            const uint8_t protoId = resp[hdrBase + 6];
            if (protoId != HID_PID_BUS_ACCESS) continue;   // bus traffic, not for us

            const int bodyLen = (resp[hdrBase + 4] << 8) | resp[hdrBase + 5];
            if (bodyLen < 2 + 2) continue;                  // need 2 hdr + 2 svc bytes

            const uint8_t *body = resp + hdrBase + 11;
            if (body[0] != SVC_PREFIX || body[1] != expectedRespSvc) continue;

            // bodyLen counts: ProtoID(1) + EmiID(1) + Mfg(2) + svcPrefix(1) + svcByte(1) + payload
            //                = 6 + payloadLen, so payloadLen = bodyLen - 6.
            respBody = QByteArray(reinterpret_cast<const char *>(body + 2),
                                  std::max(0, bodyLen - 6));
            return true;
        }
        return false;
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
        if (n < 0) {
#ifdef Q_OS_LINUX
            const int err = errno;
            if (err == ENODEV || err == ENXIO || err == EIO) {
                qWarning().nospace() << "[UsbKnx] HID-Gerät nicht mehr vorhanden (errno="
                                     << err << "), trenne";
                ::close(hidFd);
                hidFd = -1;
                emit q->errorOccurred(UsbKnxInterface::tr("USB-Interface getrennt (Gerät entfernt)"));
                emit q->disconnected();
                return false;
            }
#endif
            return false;  // EAGAIN or transient error
        }
        if (n == 0) return false;
        if (n < 11) {
            qDebug().nospace() << "[UsbKnx] HID-Read zu kurz (" << n << " Byte)";
            return true;
        }

        // Detect numbered vs unnumbered reports from first byte.
        // Numbered:   [01][seq+type][len][00][08][bodyLen_hi][bodyLen_lo][protoId][emiId][mfg_hi][mfg_lo][...]
        // Unnumbered:    [seq+type][len][00][08][bodyLen_hi][bodyLen_lo][protoId][emiId][mfg_hi][mfg_lo][...]
        if (report[0] == HID_REPORT_ID
            && report[3] == HID_PROTO_VERSION && report[4] == HID_HEADER_LENGTH) {
            hidNumberedReports = true;
        } else if (report[0] != HID_REPORT_ID
                   && report[2] == HID_PROTO_VERSION && report[3] == HID_HEADER_LENGTH) {
            hidNumberedReports = false;
        }
        const int hdrBase = hidNumberedReports ? 1 : 0;

        if (n < hdrBase + 11) {
            qDebug() << "[UsbKnx] HID-Read: Header unvollständig";
            return true;
        }
        if (report[hdrBase + 2] != HID_PROTO_VERSION
            || report[hdrBase + 3] != HID_HEADER_LENGTH) {
            qDebug().nospace() << "[UsbKnx] HID-Read: ungültiger Body-Header (PV="
                               << static_cast<int>(report[hdrBase + 2]) << " HL="
                               << static_cast<int>(report[hdrBase + 3]) << ")";
            return true;
        }

        const uint8_t protoId = report[hdrBase + 6];
        const uint8_t emiId   = report[hdrBase + 7];
        const int     bodyLen = (static_cast<uint8_t>(report[hdrBase + 4]) << 8)
                                | static_cast<uint8_t>(report[hdrBase + 5]);
        // bodyLen counts: ProtoID(1) + EmiID(1) + Mfg(2) + payload  →  payload = bodyLen - 4
        const int emiLen = bodyLen - 4;
        if (emiLen <= 0 || hdrBase + 11 + emiLen > HID_REPORT_SIZE) {
            qDebug().nospace() << "[UsbKnx] HID-Read: ungültige bodyLen=" << bodyLen;
            return true;
        }

        if (protoId == HID_PID_BUS_ACCESS) {
            // Management response — handled inline in hidMgmtRequest(), ignore here.
            return true;
        }

        // Auto-update EMI type so sendHid() uses the same format
        if (emiId == HID_PROTO_EMI2 || emiId == HID_PROTO_CEMI)
            hidEmiType = emiId;

        const QByteArray frame(reinterpret_cast<const char *>(report + hdrBase + 11),
                               emiLen);
        const QByteArray cemi = (emiId == HID_PROTO_EMI2) ? emi2ToCemi(frame) : frame;
        qDebug().nospace() << "[UsbKnx] RX cemi=" << cemi.toHex(' ').left(60);
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
        qDebug().nospace() << "[UsbKnx] TX emi=" << static_cast<int>(hidEmiType)
                           << " payload=" << payload.toHex(' ').left(60);
        hidWriteReport(HID_PID_KNX_TUNNEL, hidEmiType, payload);
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
        const qint64 written = serial->write(frame);
        if (written != static_cast<qint64>(frame.size()))
            qWarning().nospace() << "[UsbKnx] Serial-Write unvollständig: wrote=" << written
                                 << " expected=" << frame.size()
                                 << " error=" << serial->errorString();
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
