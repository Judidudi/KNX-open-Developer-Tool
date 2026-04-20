#pragma once
#include <QDialog>
#include <QString>

class QLineEdit;
class QSpinBox;
class QComboBox;

class GroupAddressDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GroupAddressDialog(QWidget *parent = nullptr);

    QString name() const;
    int     mainGroup() const;
    int     middleGroup() const;
    int     subGroup() const;
    QString dpt() const;

private:
    QLineEdit *m_nameEdit    = nullptr;
    QSpinBox  *m_mainSpin    = nullptr;
    QSpinBox  *m_middleSpin  = nullptr;
    QSpinBox  *m_subSpin     = nullptr;
    QComboBox *m_dptCombo    = nullptr;
};
