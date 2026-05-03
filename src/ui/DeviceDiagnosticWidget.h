#pragma once

#include <QWidget>
#include <QList>
#include <cstdint>

class DeviceInstance;
class InterfaceManager;
class TransportConnection;
class QTableWidget;
class QLabel;
class QPushButton;
class QTimer;

class DeviceDiagnosticWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceDiagnosticWidget(QWidget *parent = nullptr);

    void setDevice(DeviceInstance *dev);
    void setInterfaceManager(InterfaceManager *mgr);

private slots:
    void onStartClicked();
    void onTransportOpened();
    void onApduReceived(const QByteArray &apdu);
    void onTransportError(const QString &msg);
    void onTransportClosed();
    void onPropertyTimeout();
    void onInterfaceConnected();
    void onInterfaceDisconnected();
    void onCemiReceived(const QByteArray &cemi);

private:
    struct PropertyDef {
        uint8_t objIdx;
        uint8_t propId;
        QString name;
        QString (*decode)(const QByteArray &);
    };

    void readNextProperty();
    void finishCurrentProperty(const QString &value, bool ok);
    void setStatus(const QString &msg);
    void cleanup();

    static QString decodeHex(const QByteArray &d);
    static QString decodeUInt8(const QByteArray &d);
    static QString decodeUInt16(const QByteArray &d);
    static QString decodeUInt16Bytes(const QByteArray &d);
    static QString decodeSerial(const QByteArray &d);
    static QString decodeManufId(const QByteArray &d);
    static QString decodeBool(const QByteArray &d);
    static QString decodeLoadState(const QByteArray &d);
    static QString decodeRunState(const QByteArray &d);

    static const QList<PropertyDef> kProperties;

    DeviceInstance    *m_device    = nullptr;
    InterfaceManager  *m_mgr       = nullptr;
    TransportConnection *m_transport = nullptr;
    QTimer            *m_propTimer = nullptr;
    int                m_propIdx   = 0;
    bool               m_running   = false;

    QTableWidget *m_table      = nullptr;
    QLabel       *m_statusLabel = nullptr;
    QPushButton  *m_startBtn   = nullptr;
};
