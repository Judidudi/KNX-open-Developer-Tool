#pragma once

#include <QDialog>
#include <QStringList>

class LineScanRunner;
class InterfaceManager;

class QSpinBox;
class QPushButton;
class QProgressBar;
class QListWidget;
class QLabel;

// Dialog that scans a KNX line for connected devices by probing individual
// addresses and listening for A_DeviceDescriptor_Response telegrams.
class LineScanDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LineScanDialog(InterfaceManager *mgr, QWidget *parent = nullptr);
    ~LineScanDialog() override;

    // Physical addresses found during the last scan.
    QStringList foundAddresses() const { return m_found; }

private slots:
    void onStartClicked();
    void onDeviceFound(const QString &physAddr);
    void onProgress(int current, int total);
    void onScanFinished();

private:
    InterfaceManager *m_mgr    = nullptr;
    LineScanRunner   *m_runner = nullptr;

    QSpinBox     *m_areaSpin   = nullptr;
    QSpinBox     *m_lineSpin   = nullptr;
    QSpinBox     *m_maxSpin    = nullptr;
    QSpinBox     *m_timeoutSpin = nullptr;
    QPushButton  *m_startBtn   = nullptr;
    QPushButton  *m_stopBtn    = nullptr;
    QProgressBar *m_progress   = nullptr;
    QListWidget  *m_results    = nullptr;
    QLabel       *m_statusLbl  = nullptr;

    QStringList m_found;
};
