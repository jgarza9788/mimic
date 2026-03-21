#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "mimic/workspace.hpp"

namespace mimic {

/**
 * @brief Manages dynamic workspaces per monitor.
 */
class WorkspaceManager {
public:
    /**
     * @brief Ensures monitor has at least one workspace.
     */
    void ensure_monitor(int monitor_id);

    /**
     * @brief Gets active workspace for monitor.
     */
    [[nodiscard]] Workspace& active(int monitor_id);

    /**
     * @brief Gets active workspace for monitor as const.
     */
    [[nodiscard]] const Workspace& active(int monitor_id) const;

    /**
     * @brief Switches to workspace index, creating new ones on demand.
     */
    void switch_to(int monitor_id, size_t index);

    /**
     * @brief Lists workspace names for monitor.
     */
    [[nodiscard]] std::vector<std::string> names(int monitor_id) const;

private:
    /**
     * @brief Small monitor-local workspace container.
     */
    struct MonitorWorkspaces {
        std::vector<Workspace> workspaces;
        size_t active_index {0};
    };

    std::unordered_map<int, MonitorWorkspaces> by_monitor_;
};

} // namespace mimic
