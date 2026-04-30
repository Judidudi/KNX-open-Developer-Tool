#pragma once

#include "GroupAddress.h"
#include <QUndoCommand>
#include <memory>

class Project;
class TopologyNode;
class DeviceInstance;

// ─── Delete device ────────────────────────────────────────────────────────────

class DeleteDeviceCommand : public QUndoCommand
{
public:
    DeleteDeviceCommand(TopologyNode *line, int index, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
private:
    TopologyNode *m_line;
    int m_index;
    std::unique_ptr<DeviceInstance> m_device;
};

// ─── Delete line ─────────────────────────────────────────────────────────────

class DeleteLineCommand : public QUndoCommand
{
public:
    DeleteLineCommand(TopologyNode *area, int index, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
private:
    TopologyNode *m_area;
    int m_index;
    std::unique_ptr<TopologyNode> m_line;
};

// ─── Delete area ─────────────────────────────────────────────────────────────

class DeleteAreaCommand : public QUndoCommand
{
public:
    DeleteAreaCommand(Project *project, int index, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
private:
    Project *m_project;
    int m_index;
    std::unique_ptr<TopologyNode> m_area;
};

// ─── Delete group address ─────────────────────────────────────────────────────

class DeleteGroupAddressCommand : public QUndoCommand
{
public:
    DeleteGroupAddressCommand(Project *project, int index, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
private:
    Project      *m_project;
    int           m_index;
    GroupAddress  m_ga;
};

// ─── Add group address ────────────────────────────────────────────────────────

class AddGroupAddressCommand : public QUndoCommand
{
public:
    AddGroupAddressCommand(Project *project, const GroupAddress &ga, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
private:
    Project      *m_project;
    GroupAddress  m_ga;
};
