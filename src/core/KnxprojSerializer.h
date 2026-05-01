#pragma once

#include <QString>
#include <memory>

class Project;

// Reads and writes .knxproj files (KNX standard ZIP+XML format, ETS 6).
// The ZIP contains two XML entries:
//   0.xml             – project metadata / ID
//   P-XXXXXXXX/0.xml  – full topology, group addresses, device instances
//
// The project ID (P-XXXXXXXX) is generated on first save and preserved
// via Project::knxprojId() across subsequent saves.
class KnxprojSerializer
{
public:
    // Assigns a project ID if none is set yet, then writes the .knxproj ZIP.
    static bool save(Project &project, const QString &filePath);

    // Returns nullptr on failure. If errorOut is given, fills it with a
    // human-readable description of why the load failed.
    static std::unique_ptr<Project> load(const QString &filePath,
                                          QString *errorOut = nullptr);
};
