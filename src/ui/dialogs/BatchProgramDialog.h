#pragma once

#include <QDialog>
#include <QList>

class DeviceInstance;
class KnxprodCatalog;
class IKnxInterface;
class DeviceProgrammer;

class QListWidget;
class QLabel;
class QProgressBar;
class QPushButton;

// Programs multiple KNX devices sequentially.
// Each device row shows a status indicator that updates in real time.
class BatchProgramDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BatchProgramDialog(QList<DeviceInstance *> devices,
                                IKnxInterface           *iface,
                                KnxprodCatalog          *catalog,
                                QWidget                 *parent = nullptr);

private slots:
    void onStartClicked();
    void onCancelClicked();
    void onCurrentDeviceFinished(bool success, const QString &message);

private:
    void programNext();

    QList<DeviceInstance *> m_devices;
    IKnxInterface          *m_iface   = nullptr;
    KnxprodCatalog         *m_catalog = nullptr;
    int                     m_current = -1;
    DeviceProgrammer       *m_programmer = nullptr;

    QListWidget  *m_list       = nullptr;
    QLabel       *m_statusLbl  = nullptr;
    QProgressBar *m_overallBar = nullptr;
    QPushButton  *m_startBtn   = nullptr;
    QPushButton  *m_closeBtn   = nullptr;
};
