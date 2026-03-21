#pragma once

#include <vector>

namespace mimic {

/**
 * @brief Lightweight monitor descriptor used by initial implementation.
 */
struct MonitorInfo {
    int id {0};
    int x {0};
    int y {0};
    int width {0};
    int height {0};
};

/**
 * @brief Tracks monitor topology and supports safe replacement updates.
 */
class MonitorManager {
public:
    /**
     * @brief Replaces monitor list with latest detected topology.
     */
    void set_monitors(std::vector<MonitorInfo> monitors);

    /**
     * @brief Returns current known monitors.
     */
    [[nodiscard]] const std::vector<MonitorInfo>& monitors() const;

    /**
     * @brief Returns first monitor id if any.
     */
    [[nodiscard]] int primary_monitor_id() const;

private:
    std::vector<MonitorInfo> monitors_;
};

} // namespace mimic
