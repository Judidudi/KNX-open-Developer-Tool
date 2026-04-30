#include "UndoCommands.h"

#include "Project.h"
#include "TopologyNode.h"
#include "DeviceInstance.h"

// ─── DeleteDeviceCommand ──────────────────────────────────────────────────────

DeleteDeviceCommand::DeleteDeviceCommand(TopologyNode *line, int index, QUndoCommand *parent)
    : QUndoCommand(parent), m_line(line), m_index(index)
{
    const DeviceInstance *dev = line->deviceAt(index);
    setText(QObject::tr("Gerät löschen: %1")
                .arg(dev ? dev->description() : QStringLiteral("?")));
}

void DeleteDeviceCommand::undo()
{
    m_line->insertDeviceAt(m_index, std::move(m_device));
}

void DeleteDeviceCommand::redo()
{
    m_device = m_line->takeDeviceAt(m_index);
}

// ─── DeleteLineCommand ────────────────────────────────────────────────────────

DeleteLineCommand::DeleteLineCommand(TopologyNode *area, int index, QUndoCommand *parent)
    : QUndoCommand(parent), m_area(area), m_index(index)
{
    const TopologyNode *line = area->childAt(index);
    setText(QObject::tr("Linie löschen: %1")
                .arg(line ? line->name() : QStringLiteral("?")));
}

void DeleteLineCommand::undo()
{
    m_area->insertChildAt(m_index, std::move(m_line));
}

void DeleteLineCommand::redo()
{
    m_line = m_area->takeChildAt(m_index);
}

// ─── DeleteAreaCommand ────────────────────────────────────────────────────────

DeleteAreaCommand::DeleteAreaCommand(Project *project, int index, QUndoCommand *parent)
    : QUndoCommand(parent), m_project(project), m_index(index)
{
    const TopologyNode *area = project->areaAt(index);
    setText(QObject::tr("Bereich löschen: %1")
                .arg(area ? area->name() : QStringLiteral("?")));
}

void DeleteAreaCommand::undo()
{
    m_project->insertAreaAt(m_index, std::move(m_area));
}

void DeleteAreaCommand::redo()
{
    m_area = m_project->takeAreaAt(m_index);
}

// ─── DeleteGroupAddressCommand ────────────────────────────────────────────────

DeleteGroupAddressCommand::DeleteGroupAddressCommand(Project *project, int index, QUndoCommand *parent)
    : QUndoCommand(parent), m_project(project), m_index(index)
    , m_ga(project->groupAddresses().value(index))
{
    setText(QObject::tr("Gruppenadresse löschen: %1 %2")
                .arg(m_ga.toString(), m_ga.name()));
}

void DeleteGroupAddressCommand::undo()
{
    m_project->groupAddresses().insert(m_index, m_ga);
}

void DeleteGroupAddressCommand::redo()
{
    if (m_index < m_project->groupAddresses().size())
        m_project->groupAddresses().removeAt(m_index);
}

// ─── AddGroupAddressCommand ───────────────────────────────────────────────────

AddGroupAddressCommand::AddGroupAddressCommand(Project *project, const GroupAddress &ga, QUndoCommand *parent)
    : QUndoCommand(parent), m_project(project), m_ga(ga)
{
    setText(QObject::tr("Gruppenadresse hinzufügen: %1 %2")
                .arg(ga.toString(), ga.name()));
}

void AddGroupAddressCommand::undo()
{
    m_project->removeGroupAddress(m_ga.toString());
}

void AddGroupAddressCommand::redo()
{
    m_project->addGroupAddress(m_ga);
}
