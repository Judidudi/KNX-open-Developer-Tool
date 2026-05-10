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
// HID_BUS_ACCESS_SVC removed: BAS service type is encoded in EMI-ID slot (BAS_GET_REQ etc.)

// Packet type for single-frame transfer (most KNX HID frames fit in one report)
static constexpr uint8_t HID_PKT_SINGLE     = 0x05;

// Bus Access Server Feature service types — encoded in EMI-ID slot (Byte 8) of BAS reports
static constexpr uint8_t BAS_GET_REQ  = 0x01;  // Feature Get Request
static constexpr uint8_t BAS_GET_RESP = 0x02;  // Feature Get Response
static constexpr uint8_t BAS_SET_REQ  = 0x03;  // Feature Set Request
static constexpr uint8_t BAS_SET_RESP = 0x04;  // Feature Set Response

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

// EMI1 L_Data frame:  [code][ctrl1][srcHi][srcLo][dstHi][dstLo][NPCI][apdu...]
//   NPCI = AT(bit7) | HopCount(bits6-4) | DataLen(bits3-0)
// cEMI L_Data frame:  [code][0x00][ctrl1][ctrl2][srcHi][srcLo][dstHi][dstLo][dataLen][apdu...]
//   ctrl2 = NPCI & 0xF0  (AT + HopCount bits at identical positions)

static QByteArray emi1ToCemi(const QByteArray &emi1)
{
    if (emi1.size() < 7) return {};
    uint8_t cemiCode;
    switch (static_cast<uint8_t>(emi1[0])) {
        case 0x49: cemiCode = 0x29; break;   // L_Data.ind
        case 0x4E: cemiCode = 0x2E; break;   // L_Data.con
        case 0x11: cemiCode = 0x11; break;   // L_Data.req (same in both)
        default: return {};
    }
    const uint8_t npci = static_cast<uint8_t>(emi1[6]);
    QByteArray cemi;
    cemi.reserve(emi1.size() + 1);
    cemi.append(static_cast<char>(cemiCode));
    cemi.append('\x00');                        // addInfoLen = 0
    cemi.append(emi1[1]);                       // ctrl1
    cemi.append(static_cast<char>(npci & 0xF0)); // ctrl2 = AT(bit7) + HopCount(bits6-4)
    cemi.append(emi1.mid(2, 4));                // src(2) + dst(2)
    cemi.append(static_cast<char>(npci & 0x0F)); // dataLen (bits3-0 of NPCI)
    cemi.append(emi1.mid(7));                   // APCI + data
    return cemi;
}

static QByteArray cemiToEmi1(const QByteArray &cemi)
{
    if (cemi.size() < 2) return {};
    const int addLen = static_cast<uint8_t>(cemi[1]);
    const int base   = 2 + addLen;
    if (cemi.size() < base + 7) return {};
    uint8_t emi1Code;
    switch (static_cast<uint8_t>(cemi[0])) {
        case 0x29: emi1Code = 0x49; break;
        case 0x2E: emi1Code = 0x4E; break;
        case 0x11: emi1Code = 0x11; break;
        default: return {};
    }
    const uint8_t ctrl2 = static_cast<uint8_t>(cemi[base + 1]);
    const uint8_t dLen  = static_cast<uint8_t>(cemi[base + 6]);
    QByteArray emi1;
    emi1.reserve(cemi.size() - addLen - 1);
    emi1.append(static_cast<char>(emi1Code));
    emi1.append(cemi[base]);                    // ctrl1
    emi1.append(cemi.mid(base + 2, 4));         // src(2) + dst(2)
    emi1.append(static_cast<char>((ctrl2 & 0xF0) | (dLen & 0x0F))); // NPCI
    emi1.append(cemi.mid(base + 7));            // APCI + data
    return emi1;
}

// ---- Priv -------------------------------------------------------------------

struct UsbKnxInterface::Priv
{
    UsbKnxInterface *q;
    Transport    transport;
    QString      devicePath;
    bool         connected          = false;
    uint8_t      hidEmiType         = HID_PROTO_EMI1; // updated by negotiation or auto-detect
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

        // Negotiate EMI protocol type via Bus Access Server Feature Get/Set (spec 07_01_01).
        // Falls back to EMI1 if device does not respond to management frames.
        // BCU activation is sent once after negotiation, regardless of method.
        const bool negotiated = negotiateHidProtocol();
        if (!negotiated) {
            hidEmiType = HID_PROTO_EMI1;
            qWarning() << "[UsbKnx] BAS-Aushandlung fehlgeschlagen – EMI1 als Fallback";
        }
        if (hidEmiType == HID_PROTO_EMI1)
            activateBcuEmi1();

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
        const int bodyLen = payload.size();   // BodyLen = payload only; ProtoID/EmiID/Mfg are in header

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

    // Send a KNX USB Bus Access Server Feature management request and wait for response.
    // svcType  = service type encoded in the EMI-ID slot (BAS_GET_REQ / BAS_SET_REQ).
    // reqBody  = [featureId, optionalValue...]
    // respBody = full response body [featureId, value...] on success
    bool hidMgmtRequest(uint8_t svcType, const QByteArray &reqBody,
                        uint8_t expectedRespSvcType, QByteArray &respBody)
    {
        if (!hidWriteReport(HID_PID_BUS_ACCESS, svcType, reqBody))
            return false;

        // Read response — skip unrelated bus traffic frames
        for (int i = 0; i < 8; ++i) {
            uint8_t resp[HID_REPORT_SIZE] = {};
            if (!hidPollRead(resp, HID_INIT_TIMEOUT_MS))
                return false;

            const int hdrBase = hidNumberedReports ? 1 : 0;
            if (HID_REPORT_SIZE < hdrBase + 11)          continue;
            if (resp[hdrBase + 2] != HID_PROTO_VERSION)  continue;
            if (resp[hdrBase + 3] != HID_HEADER_LENGTH)  continue;
            if (resp[hdrBase + 6] != HID_PID_BUS_ACCESS) continue;  // bus traffic
            if (resp[hdrBase + 7] != expectedRespSvcType) continue;

            const int bodyLen = (static_cast<uint8_t>(resp[hdrBase + 4]) << 8)
                               |  static_cast<uint8_t>(resp[hdrBase + 5]);
            if (bodyLen < 1) continue;

            // Verify feature ID in response body matches our request
            if (!reqBody.isEmpty()
                && static_cast<uint8_t>(resp[hdrBase + 10]) != static_cast<uint8_t>(reqBody[0]))
                continue;

            respBody = QByteArray(reinterpret_cast<const char *>(resp + hdrBase + 10), bodyLen);
            return true;
        }
        return false;
    }

    // BCU activation for EMI1 interfaces.
    // Sends the two-command init sequence from knxd EMI1Driver::cmdOpen() +
    // cmdEnterMonitor().  Without cmdEnterMonitor the interface stays closed
    // and does NOT forward any received KNX bus frames to the host.
    void activateBcuEmi1()
    {
        // Step 1 – cmdOpen: clear address table (knxd: EMI1Driver::cmdOpen)
        static const uint8_t kCmdOpen[] = { 0x46, 0x01, 0x01, 0x16, 0x00 };
        hidWriteReport(HID_PID_KNX_TUNNEL, HID_PROTO_EMI1,
                       QByteArray(reinterpret_cast<const char *>(kCmdOpen), sizeof(kCmdOpen)));
        qDebug() << "[UsbKnx] EMI1 cmdOpen (0x46 0x01 0x01 0x16 0x00) gesendet";

        // Drain ack (0x47-prefixed response) before sending next command
        uint8_t ack[HID_REPORT_SIZE] = {};
        hidPollRead(ack, 300);

        // Step 2 – cmdEnterMonitor: put interface into receive mode so it forwards
        // all incoming KNX bus frames to the host (knxd: EMI1Driver::cmdEnterMonitor)
        static const uint8_t kCmdEnterMonitor[] = { 0x46, 0x01, 0x00, 0x60, 0x90 };
        hidWriteReport(HID_PID_KNX_TUNNEL, HID_PROTO_EMI1,
                       QByteArray(reinterpret_cast<const char *>(kCmdEnterMonitor),
                                  sizeof(kCmdEnterMonitor)));
        qDebug() << "[UsbKnx] EMI1 cmdEnterMonitor (0x46 0x01 0x00 0x60 0x90) gesendet";

        // Drain final ack
        hidPollRead(ack, 300);
    }

    bool negotiateHidProtocol()
    {
        // Step 1: query supported EMI types (Feature 0x01)
        const QByteArray getReq(1, static_cast<char>(FEAT_SUPPORTED_EMI));
        QByteArray resp;
        if (!hidMgmtRequest(BAS_GET_REQ, getReq, BAS_GET_RESP, resp))
            return false;

        if (resp.size() < 2 || static_cast<uint8_t>(resp[0]) != FEAT_SUPPORTED_EMI)
            return false;

        const uint8_t supported = static_cast<uint8_t>(resp[1]);
        // bit2=cEMI, bit1=EMI2, bit0=EMI1 — choose best available
        uint8_t chosen;
        if      (supported & 0x04) chosen = HID_PROTO_CEMI;
        else if (supported & 0x02) chosen = HID_PROTO_EMI2;
        else if (supported & 0x01) chosen = HID_PROTO_EMI1;  // Hager/Insta
        else return false;

        // Step 2: set chosen EMI type (Feature 0x02)
        QByteArray setReq;
        setReq.append(static_cast<char>(FEAT_CURRENT_EMI));
        setReq.append(static_cast<char>(chosen));
        QByteArray setResp;
        if (!hidMgmtRequest(BAS_SET_REQ, setReq, BAS_SET_RESP, setResp))
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

        // Raw dump — shows every byte the device sends before any parsing
        qDebug().nospace() << "[UsbKnx] RAW " << n << "B: "
                           << QByteArray(reinterpret_cast<const char *>(report),
                                         static_cast<int>(n)).toHex(' ').left(96);

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
        // bodyLen = length of EMI frame only (ProtoID/EmiID/Mfg are in the 8-byte header)
        const int emiLen = bodyLen;
        if (emiLen <= 0 || hdrBase + 10 + emiLen > HID_REPORT_SIZE) {
            qDebug().nospace() << "[UsbKnx] HID-Read: ungültige bodyLen=" << bodyLen;
            return true;
        }

        if (protoId == HID_PID_BUS_ACCESS) {
            // Management response — handled inline in hidMgmtRequest(), ignore here.
            return true;
        }

        // Auto-update EMI type so sendHid() uses the same format
        if (emiId == HID_PROTO_EMI1 || emiId == HID_PROTO_EMI2 || emiId == HID_PROTO_CEMI)
            hidEmiType = emiId;

        // EMI frame starts at hdrBase+10 (after the 8-byte body header: PV+HL+BodyLen+ProtoID+EmiID+Mfg)
        const QByteArray frame(reinterpret_cast<const char *>(report + hdrBase + 10), emiLen);
        QByteArray cemi;
        if      (emiId == HID_PROTO_EMI1) cemi = emi1ToCemi(frame);
        else if (emiId == HID_PROTO_EMI2) cemi = emi2ToCemi(frame);
        else                               cemi = frame;
        qDebug().nospace() << "[UsbKnx] RX emi=" << static_cast<int>(emiId)
                           << " raw=" << frame.toHex(' ').left(40)
                           << " cemi=" << cemi.toHex(' ').left(40);
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

        QByteArray payload;
        if      (hidEmiType == HID_PROTO_EMI1) payload = cemiToEmi1(cemi);
        else if (hidEmiType == HID_PROTO_EMI2) payload = cemiToEmi2(cemi);
        else                                   payload = cemi;
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
