#pragma once
#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <iostream>
#include "engine/ECS/Registry.h"
#include "engine/ECS/Components.h"

namespace Core {

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual void redo() { execute(); }
    virtual std::string description() const = 0;
};

// Generic Functional Undo/Redo Action
class ActionCommand : public ICommand {
public:
    ActionCommand(std::string name, std::function<void()> doAction, std::function<void()> undoAction)
        : m_name(std::move(name)), m_do(std::move(doAction)), m_undo(std::move(undoAction)) {}

    void execute() override { if (m_do) m_do(); }
    void undo() override { if (m_undo) m_undo(); }
    std::string description() const override { return m_name; }

private:
    std::string m_name;
    std::function<void()> m_do;
    std::function<void()> m_undo;
};

// Transform Gizmo Edit Command
class TransformCommand : public ICommand {
public:
    TransformCommand(Registry* reg, Entity e, const Transform& oldTr, const Transform& newTr)
        : m_reg(reg), m_entity(e), m_oldTr(oldTr), m_newTr(newTr) {}

    void execute() override {
        if (m_reg && m_reg->valid(m_entity) && m_reg->has<Transform>(m_entity)) {
            m_reg->get<Transform>(m_entity) = m_newTr;
        }
    }

    void undo() override {
        if (m_reg && m_reg->valid(m_entity) && m_reg->has<Transform>(m_entity)) {
            m_reg->get<Transform>(m_entity) = m_oldTr;
        }
    }

    std::string description() const override {
        return "Modify Transform (" + std::to_string(static_cast<uint32_t>(m_entity)) + ")";
    }

private:
    Registry* m_reg = nullptr;
    Entity m_entity = NullEntity;
    Transform m_oldTr;
    Transform m_newTr;
};

class UndoManager {
public:
    static UndoManager& Get() {
        static UndoManager s_instance;
        return s_instance;
    }

    void execute(std::shared_ptr<ICommand> cmd) {
        if (!cmd) return;
        cmd->execute();

        // Clear redo stack on new command
        if (m_currentIndex < static_cast<int>(m_history.size())) {
            m_history.erase(m_history.begin() + m_currentIndex, m_history.end());
        }

        m_history.push_back(cmd);
        m_currentIndex = static_cast<int>(m_history.size());

        // Cap history at 100 entries
        if (m_history.size() > 100) {
            m_history.erase(m_history.begin());
            m_currentIndex--;
        }
    }

    bool canUndo() const {
        return m_currentIndex > 0;
    }

    bool canRedo() const {
        return m_currentIndex < static_cast<int>(m_history.size());
    }

    void undo() {
        if (!canUndo()) return;
        m_currentIndex--;
        m_history[static_cast<size_t>(m_currentIndex)]->undo();
    }

    void redo() {
        if (!canRedo()) return;
        m_history[static_cast<size_t>(m_currentIndex)]->redo();
        m_currentIndex++;
    }

    void clear() {
        m_history.clear();
        m_currentIndex = 0;
    }

    const std::vector<std::shared_ptr<ICommand>>& history() const { return m_history; }
    int currentIndex() const { return m_currentIndex; }

private:
    std::vector<std::shared_ptr<ICommand>> m_history;
    int m_currentIndex = 0;
};

} // namespace Core
