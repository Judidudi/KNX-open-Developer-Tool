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
//   project/openLastOnStart         bool
//   busMonitor/maxEntries           int (default 2000)
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

    QLineEdit  *m_hostEdit       = nullptr;
    QSpinBox   *m_portSpin       = nullptr;
    QCheckBox  *m_connectOnStart = nullptr;
    QCheckBox  *m_openLastProjCb = nullptr;
    QSpinBox   *m_maxEntriesSpin = nullptr;
    QComboBox  *m_langCombo      = nullptr;
};
