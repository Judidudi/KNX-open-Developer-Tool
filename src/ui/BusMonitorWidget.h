#pragma once

#include <QWidget>
#include <QByteArray>

class BusMonitorModel;
class InterfaceManager;

class QTableView;
class QPushButton;
class QLineEdit;
class QLabel;
class QSortFilterProxyModel;

// Live telegram log. Subscribes to InterfaceManager::cemiFrameReceived and
// appends rows to an internal model, which is shown in a QTableView.
// Supports start/stop (pause incoming updates) and a text filter that matches
// against source, destination and type columns.
class BusMonitorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BusMonitorWidget(QWidget *parent = nullptr);

    void setInterfaceManager(InterfaceManager *mgr);

private slots:
    void onCemiReceived(const QByteArray &cemi);
    void onStartStopClicked();
    void onClearClicked();
    void onFilterChanged(const QString &text);

private:
    BusMonitorModel       *m_model     = nullptr;
    QSortFilterProxyModel *m_proxy     = nullptr;
    QTableView            *m_view      = nullptr;
    QPushButton           *m_startStop = nullptr;
    QPushButton           *m_clear     = nullptr;
    QLineEdit             *m_filter    = nullptr;
    QLabel                *m_counter   = nullptr;

    InterfaceManager      *m_iface     = nullptr;
    bool                   m_running   = true;
    int                    m_totalRows = 0;
};
