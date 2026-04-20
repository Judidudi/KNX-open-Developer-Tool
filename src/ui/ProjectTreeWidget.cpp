#include "ProjectTreeWidget.h"
#include "Project.h"
#include <QVBoxLayout>
#include <QTreeView>
#include <QLabel>

ProjectTreeWidget::ProjectTreeWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel(tr("Projekt-Browser"), this));
    layout->addWidget(new QTreeView(this));
}

void ProjectTreeWidget::setProject(Project *) {}
