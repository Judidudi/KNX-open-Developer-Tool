#include "UsbKnxInterface.h"

#ifndef KNXODT_NO_SERIAL
#  include <QSerialPort>
#  include <QSerialPortInfo>
#endif

#ifdef Q_OS_LINUX
#  include <QSocketNotifier>
#  include <fcntl.h>
#  include <unistd.h>
#  include <dirent.h>
#  include <linux/hidraw.h>
#  include <sys/ioctl.h>
#endif

#include <QDir>
#include <QFileInfo>

// ---- Serial framing ---------------------------------------------------------
// Start byte 0xAB is not a valid cEMI message code (0x11/0x2E/0x29), allowing
// byte-level resynchronization without state machine complexity.
static constexpr uint8_t SERIAL_START = 0xAB;

// ---- HID report format (KNX spec 07_01_01) ----------------------------------
static constexpr int    HID_REPORT_SIZE  = 64;
static constexpr uint8_t HID_REPORT_ID   = 0x01;
static constexpr uint8_t HID_PACKET_INFO = 0x13; // single packet, cEMI protocol

// ---- Priv implementation ----------------------------------------------------

struct UsbKnxInterface::Priv
{
    UsbKnxInterface *q;
    Transport    transport;
    QString      devicePath;
    bool         connected = false;
    QByteArray   recvBuf;

#ifndef KNXODT_NO_SERIAL
    QSerialPort *serial = nullptr;
#endif

#ifdef Q_OS_LINUX
    int              hidFd       = -1;
    QSocketNotifier *hidNotifier = nullptr;
#endif

    explicit Priv(UsbKnxInterface *owner, Transport t, const QString &path)
        : q(owner), transport(t), devicePath(path)
    {}

    bool openSerial()
    {
#ifndef KNXODT_NO_SERIAL
        if (devicePath.isEmpty()) {
            emit q->errorOccurred(QObject::tr("USB-Seriell: Kein Gerätepfad angegeben."));
            return false;
        }
        if (!serial) {
            serial = new QSerialPort(q);
            QObject::connect(serial, &QSerialPort::readyRead, q, [this]() { onSerialRead(); });
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
        hidNotifier = new QSocketNotifier(hidFd, QSocketNotifier::Read, q);
        QObject::connect(hidNotifier, &QSocketNotifier::activated, q,
            [this](QSocketDescriptor, QSocketNotifier::Type) { onHidRead(); });
        connected = true;
        recvBuf.clear();
        emit q->connected();
        return true;
#else
        emit q->errorOccurred(QObject::tr("USB-HID-Interface ist nur unter Linux verfügbar."));
        return false;
#endif
    }

    void closeHid()
    {
#ifdef Q_OS_LINUX
        if (hidNotifier) {
            hidNotifier->setEnabled(false);
            delete hidNotifier;
            hidNotifier = nullptr;
        }
        if (hidFd >= 0) {
            ::close(hidFd);
            hidFd = -1;
        }
#endif
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

    void sendHid(const QByteArray &cemi)
    {
#ifdef Q_OS_LINUX
        if (hidFd < 0) return;
        uint8_t report[HID_REPORT_SIZE] = {};
        report[0] = HID_REPORT_ID;
        report[1] = HID_PACKET_INFO;
        const int len = std::min(static_cast<int>(cemi.size()), HID_REPORT_SIZE - 3);
        report[2] = static_cast<uint8_t>(len);
        std::memcpy(report + 3, cemi.constData(), static_cast<size_t>(len));
        ::write(hidFd, report, HID_REPORT_SIZE);
#else
        Q_UNUSED(cemi)
#endif
    }

    void onSerialRead()
    {
#ifndef KNXODT_NO_SERIAL
        if (!serial) return;
        recvBuf.append(serial->readAll());
        parseSerialBuffer();
#endif
    }

    void onHidRead()
    {
#ifdef Q_OS_LINUX
        uint8_t report[HID_REPORT_SIZE] = {};
        const ssize_t n = ::read(hidFd, report, HID_REPORT_SIZE);
        if (n < 3) return;
        if (report[0] != HID_REPORT_ID) return;
        const int dataLen = report[2];
        if (n < 3 + dataLen) return;
        const QByteArray cemi(reinterpret_cast<const char *>(report + 3), dataLen);
        emit q->cemiFrameReceived(cemi);
#endif
    }

    void parseSerialBuffer()
    {
        // Consume bytes until we have a complete frame.
        // Format: [0xAB][len_hi][len_lo][cemi...]
        while (recvBuf.size() >= 3) {
            // Resync: skip bytes until start marker
            int start = -1;
            for (int i = 0; i < recvBuf.size(); ++i) {
                if (static_cast<uint8_t>(recvBuf[i]) == SERIAL_START) {
                    start = i;
                    break;
                }
            }
            if (start < 0) {
                recvBuf.clear();
                return;
            }
            if (start > 0)
                recvBuf.remove(0, start);

            if (recvBuf.size() < 3) return; // need length bytes

            const int payloadLen =
                (static_cast<uint8_t>(recvBuf[1]) << 8) |
                 static_cast<uint8_t>(recvBuf[2]);

            if (recvBuf.size() < 3 + payloadLen) return; // incomplete

            const QByteArray cemi = recvBuf.mid(3, payloadLen);
            recvBuf.remove(0, 3 + payloadLen);
            emit q->cemiFrameReceived(cemi);
        }
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
    if (d->connected)
        return true;
    if (d->transport == Transport::Serial)
        return d->openSerial();
    return d->openHid();
}

void UsbKnxInterface::disconnectFromInterface()
{
    if (!d->connected)
        return;
    d->connected = false;
    if (d->transport == Transport::Serial)
        d->closeSerial();
    else
        d->closeHid();
    emit disconnected();
}

void UsbKnxInterface::sendCemiFrame(const QByteArray &cemi)
{
    if (!d->connected) {
        emit errorOccurred(tr("USB-Interface: Nicht verbunden."));
        return;
    }
    if (d->transport == Transport::Serial)
        d->sendSerial(cemi);
    else
        d->sendHid(cemi);
}

QStringList UsbKnxInterface::availableSerialPorts()
{
    QStringList result;
#ifndef KNXODT_NO_SERIAL
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        if (!info.isNull())
            result << info.systemLocation();
    }
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
