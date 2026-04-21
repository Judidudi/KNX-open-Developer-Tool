#pragma once

#include <QDialog>

class DeviceProgrammer;
class QLabel;
class QProgressBar;
class QListWidget;
class QDialogButtonBox;
class QPushButton;

// Drives a DeviceProgrammer and visualizes its progress: a step list with
// icons (pending/running/done/failed), an overall progress bar and a status
// line. Stays open after completion so the user can read the log; close
// button is only enabled once programming has finished.
class ProgramDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProgramDialog(DeviceProgrammer *programmer, QWidget *parent = nullptr);

private slots:
    void onStepStarted(int step, const QString &description);
    void onStepCompleted(int step);
    void onProgressUpdated(int percent);
    void onFinished(bool success, const QString &message);
    void onStartClicked();
    void onCancelClicked();

private:
    DeviceProgrammer *m_programmer  = nullptr;
    QLabel           *m_header      = nullptr;
    QLabel           *m_status      = nullptr;
    QProgressBar     *m_progress    = nullptr;
    QListWidget      *m_steps       = nullptr;
    QPushButton      *m_startButton = nullptr;
    QPushButton      *m_closeButton = nullptr;
};
