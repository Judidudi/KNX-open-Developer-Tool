#pragma once

#include <QDialog>

class QCheckBox;
class QSpinBox;
class QLineEdit;
class QComboBox;

// Application-wide preferences stored in QSettings.
// Keys:
//   connection/defaultHost          QString
//   connection/defaultPort          int (default 3671)
//   connection/connectOnStart       bool
//   connection/autoReconnect        bool (default true)
//   connection/heartbeatTimeoutMs   int (default 3000)
//   connection/maxReconnectAttempts int (default 5)
//   project/openLastOnStart         bool
//   busMonitor/maxEntries           int (default 2000)
//   programming/ackTimeoutMs        int (default 6000)
//   programming/verifyEnabled       bool (default true)
//   ui/language                     QString ("de" | "en")
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
    void onAccepted();

private:
    void load();
    void save();

    // Connection
    QLineEdit  *m_hostEdit            = nullptr;
    QSpinBox   *m_portSpin            = nullptr;
    QCheckBox  *m_connectOnStart      = nullptr;
    QCheckBox  *m_autoReconnectCb     = nullptr;
    QSpinBox   *m_heartbeatTimeoutSpin = nullptr;
    QSpinBox   *m_maxReconnectSpin    = nullptr;

    // Project
    QCheckBox  *m_openLastProjCb      = nullptr;

    // Bus monitor
    QSpinBox   *m_maxEntriesSpin      = nullptr;

    // Programming
    QSpinBox   *m_ackTimeoutSpin      = nullptr;
    QCheckBox  *m_verifyEnabledCb     = nullptr;

    // UI
    QComboBox  *m_langCombo           = nullptr;
};
