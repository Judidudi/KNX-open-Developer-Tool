#pragma once

#include "DeviceProgrammer.h"
#include <QDialog>

class QLabel;
class QProgressBar;
class QListWidget;
class QPushButton;
class QComboBox;

// Drives a DeviceProgrammer and shows its progress: step list with status icons,
// progress bar, mode selector (disabled once started), and a warning area for
// non-fatal issues (mask mismatch, restart timeout, etc.).
class ProgramDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProgramDialog(DeviceProgrammer *programmer, QWidget *parent = nullptr);

private slots:
    void onStepStarted(int seqIdx, const QString &description);
    void onStepCompleted(int seqIdx);
    void onProgressUpdated(int percent);
    void onWarning(const QString &message);
    void onFinished(bool success, const QString &message);
    void onStartClicked();
    void onCancelClicked();
    void onModeChanged(int index);

private:
    void rebuildStepList();

    DeviceProgrammer *m_programmer  = nullptr;
    QComboBox        *m_modeCombo   = nullptr;
    QLabel           *m_header      = nullptr;
    QLabel           *m_warning     = nullptr;
    QLabel           *m_status      = nullptr;
    QProgressBar     *m_progress    = nullptr;
    QListWidget      *m_steps       = nullptr;
    QPushButton      *m_startButton = nullptr;
    QPushButton      *m_closeButton = nullptr;
};
