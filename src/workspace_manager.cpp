#include "mimic/workspace_manager.hpp"

#include <stdexcept>

namespace mimic {

void WorkspaceManager::ensure_monitor(int monitor_id) {
    auto& state = by_monitor_[monitor_id];
    if (state.workspaces.empty()) {
        state.workspaces.emplace_back("1");
        state.active_index = 0;
    }
}

Workspace& WorkspaceManager::active(int monitor_id) {
    ensure_monitor(monitor_id);
    auto& state = by_monitor_.at(monitor_id);
    return state.workspaces[state.active_index];
}

const Workspace& WorkspaceManager::active(int monitor_id) const {
    const auto found = by_monitor_.find(monitor_id);
    if (found == by_monitor_.end() || found->second.workspaces.empty()) {
        throw std::runtime_error("monitor has no workspace");
    }
    return found->second.workspaces[found->second.active_index];
}

void WorkspaceManager::switch_to(int monitor_id, size_t index) {
    ensure_monitor(monitor_id);
    auto& state = by_monitor_.at(monitor_id);
    while (state.workspaces.size() <= index) {
        state.workspaces.emplace_back(std::to_string(state.workspaces.size() + 1));
    }
    state.active_index = index;
}

std::vector<std::string> WorkspaceManager::names(int monitor_id) const {
    std::vector<std::string> out;
    const auto found = by_monitor_.find(monitor_id);
    if (found == by_monitor_.end()) {
        return out;
    }
    for (const auto& workspace : found->second.workspaces) {
        out.push_back(workspace.name());
    }
    return out;
}

} // namespace mimic
