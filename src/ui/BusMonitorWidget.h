#pragma once

#include <QWidget>
#include <QByteArray>

class BusMonitorModel;
class InterfaceManager;
class Project;

class QTableView;
class QPushButton;
class QLineEdit;
class QComboBox;
class QLabel;
class QCheckBox;

// Live telegram log with integrated group-telegram sender.
// Subscribes to InterfaceManager::cemiFrameReceived and appends rows to an
// internal model shown in a QTableView.
// The send panel at the bottom allows writing or reading group address values.
// Advanced filters (source regex, destination regex, type) are shown in a
// second filter bar. CSV export writes all currently visible rows.
class BusMonitorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BusMonitorWidget(QWidget *parent = nullptr);

    void setInterfaceManager(InterfaceManager *mgr);
    void setProject(Project *project);
    void applySettings();

private slots:
    void onCemiReceived(const QByteArray &cemi);
    void onStartStopClicked();
    void onClearClicked();
    void onFilterChanged(const QString &text);
    void onOnlyGroupsToggled(bool checked);
    void onSrcFilterChanged(const QString &text);
    void onDstFilterChanged(const QString &text);
    void onTypeFilterChanged(int index);
    void onTsFormatChanged(int index);
    void onExportCsv();
    void onSendClicked();
    void onReadClicked();

private:
    static QByteArray encodeValue(int dptIndex, const QString &text);

    BusMonitorModel *m_model     = nullptr;
    class BusMonitorProxy *m_proxy = nullptr;
    QTableView    *m_view        = nullptr;
    QPushButton   *m_startStop   = nullptr;
    QPushButton   *m_clear       = nullptr;
    QLineEdit     *m_filter      = nullptr;
    QCheckBox     *m_onlyGroups  = nullptr;
    QLabel        *m_counter     = nullptr;

    // Advanced filter bar
    QLineEdit     *m_srcFilter   = nullptr;
    QLineEdit     *m_dstFilter   = nullptr;
    QComboBox     *m_typeFilter  = nullptr;
    QComboBox     *m_tsFormat    = nullptr;
    QPushButton   *m_exportBtn   = nullptr;

    // Send panel
    QLineEdit   *m_sendGa    = nullptr;
    QComboBox   *m_sendDpt   = nullptr;
    QLineEdit   *m_sendValue = nullptr;
    QPushButton *m_sendBtn   = nullptr;
    QPushButton *m_readBtn   = nullptr;

    InterfaceManager *m_iface     = nullptr;
    bool              m_running   = true;
    int               m_totalRows = 0;
};
