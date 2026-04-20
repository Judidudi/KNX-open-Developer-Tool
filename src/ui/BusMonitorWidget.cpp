#include "BusMonitorWidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QTableView>
#include <QPushButton>
#include <QHBoxLayout>

BusMonitorWidget::BusMonitorWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *btnBar = new QHBoxLayout;
    btnBar->addWidget(new QPushButton(tr("Start"), this));
    btnBar->addWidget(new QPushButton(tr("Stop"), this));
    btnBar->addWidget(new QPushButton(tr("Löschen"), this));
    btnBar->addStretch();
    layout->addLayout(btnBar);
    layout->addWidget(new QTableView(this));
}
