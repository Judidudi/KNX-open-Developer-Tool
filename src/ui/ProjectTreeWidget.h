#pragma once
#include <QWidget>

class Project;

class ProjectTreeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ProjectTreeWidget(QWidget *parent = nullptr);
    void setProject(Project *project);
};
