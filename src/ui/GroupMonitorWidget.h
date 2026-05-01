#pragma once

#include <QWidget>
#include <QMap>

class Project;
class InterfaceManager;
class GroupAddress;

class QTableWidget;
class QPushButton;
class QLabel;

// Displays the current value of every group address in the project.
// Listens for GroupValue_Response telegrams on the bus and refreshes
// the "Wert" column. The user can trigger a GroupValue_Read or a
// GroupValue_Write for any individual GA or read all GAs at once.
class GroupMonitorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GroupMonitorWidget(QWidget *parent = nullptr);

    void setProject(Project *project);
    void setInterfaceManager(InterfaceManager *mgr);

private slots:
    void onCemiReceived(const QByteArray &cemi);
    void onReadAllClicked();
    void onReadRowClicked(int row);
    void onSendRowClicked(int row);

private:
    void rebuild();
    void sendRead(uint16_t ga);
    void sendWrite(uint16_t ga, const QString &dpt, const QString &text);
    static QByteArray encodeValue(const QString &dpt, const QString &text, bool *ok = nullptr);
    static QString    decodeValue(const QString &dpt, const QByteArray &apdu);

    Project          *m_project = nullptr;
    InterfaceManager *m_iface   = nullptr;

    QTableWidget *m_table     = nullptr;
    QPushButton  *m_readAll   = nullptr;
    QLabel       *m_status    = nullptr;

    // ga raw → row index
    QMap<uint16_t, int> m_gaRow;
};
