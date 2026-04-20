#include "CatalogWidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QListView>
#include <QLineEdit>

CatalogWidget::CatalogWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel(tr("Geräte-Katalog"), this));
    layout->addWidget(new QLineEdit(this));
    layout->addWidget(new QListView(this));
}
