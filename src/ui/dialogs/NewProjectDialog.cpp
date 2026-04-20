#include "NewProjectDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>

NewProjectDialog::NewProjectDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Neues Projekt"));
    setMinimumWidth(320);

    auto *layout = new QVBoxLayout(this);

    auto *form = new QFormLayout;
    m_nameEdit = new QLineEdit(tr("Mein KNX-Projekt"), this);
    form->addRow(tr("Projektname:"), m_nameEdit);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Anlegen"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    m_nameEdit->selectAll();
    m_nameEdit->setFocus();
}

QString NewProjectDialog::projectName() const
{
    return m_nameEdit->text().trimmed();
}
