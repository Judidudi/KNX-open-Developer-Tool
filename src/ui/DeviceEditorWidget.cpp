#include "DeviceEditorWidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QTabWidget>

DeviceEditorWidget::DeviceEditorWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(new QWidget(tabs), tr("Parameter"));
    tabs->addTab(new QWidget(tabs), tr("Kommunikationsobjekte"));
    layout->addWidget(new QLabel(tr("Kein Gerät ausgewählt"), this));
    layout->addWidget(tabs);
}
