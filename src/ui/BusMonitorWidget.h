#pragma once

#include <QWidget>
#include <QByteArray>

class BusMonitorModel;
class InterfaceManager;

class QTableView;
class QPushButton;
class QLineEdit;
class QComboBox;
class QLabel;
class QSortFilterProxyModel;

// Live telegram log with integrated group-telegram sender.
// Subscribes to InterfaceManager::cemiFrameReceived and appends rows to an
// internal model shown in a QTableView.
// The send panel at the bottom allows writing or reading group address values.
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
    void onSendClicked();
    void onReadClicked();

private:
    // Encode the user-supplied value string according to the selected DPT.
    // Returns the raw payload bytes to pass to CemiFrame::buildGroupValueWrite.
    static QByteArray encodeValue(int dptIndex, const QString &text);

    BusMonitorModel       *m_model     = nullptr;
    QSortFilterProxyModel *m_proxy     = nullptr;
    QTableView            *m_view      = nullptr;
    QPushButton           *m_startStop = nullptr;
    QPushButton           *m_clear     = nullptr;
    QLineEdit             *m_filter    = nullptr;
    QLabel                *m_counter   = nullptr;

    // Send panel
    QLineEdit   *m_sendGa    = nullptr;
    QComboBox   *m_sendDpt   = nullptr;
    QLineEdit   *m_sendValue = nullptr;
    QPushButton *m_sendBtn   = nullptr;
    QPushButton *m_readBtn   = nullptr;

    InterfaceManager      *m_iface     = nullptr;
    bool                   m_running   = true;
    int                    m_totalRows = 0;
};
