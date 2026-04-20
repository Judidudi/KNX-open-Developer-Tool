#pragma once

#include <QString>
#include <memory>

class Project;

class ProjectXmlSerializer
{
public:
    static bool save(const Project &project, const QString &filePath);
    static std::unique_ptr<Project> load(const QString &filePath);
};
