#pragma once
#include <QDialog>
#include <QString>

class QLineEdit;

class NewProjectDialog : public QDialog
{
    Q_OBJECT
public:
    explicit NewProjectDialog(QWidget *parent = nullptr);
    QString projectName() const;

private:
    QLineEdit *m_nameEdit = nullptr;
};
