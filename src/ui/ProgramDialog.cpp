#include "ProgramDialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QDialogButtonBox>

ProgramDialog::ProgramDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Gerät programmieren"));
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Bereit zum Programmieren…"), this));
    layout->addWidget(new QProgressBar(this));
    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(buttons);
}
