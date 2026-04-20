#pragma once

#include <QObject>
#include <QList>

struct KnxIpDevice {
    QString name;
    QString ipAddress;
    quint16 port = 3671;
};

class QUdpSocket;

class KnxIpDiscovery : public QObject
{
    Q_OBJECT
public:
    explicit KnxIpDiscovery(QObject *parent = nullptr);
    ~KnxIpDiscovery() override;

    void startSearch();

signals:
    void deviceFound(KnxIpDevice device);
    void searchFinished();

private slots:
    void onReadyRead();
    void onTimeout();

private:
    QUdpSocket *m_socket  = nullptr;
};
