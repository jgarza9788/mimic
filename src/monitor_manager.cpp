#include "mimic/monitor_manager.hpp"

namespace mimic {

void MonitorManager::set_monitors(std::vector<MonitorInfo> monitors) {
    monitors_ = std::move(monitors);
}

const std::vector<MonitorInfo>& MonitorManager::monitors() const {
    return monitors_;
}

int MonitorManager::primary_monitor_id() const {
    if (monitors_.empty()) {
        return 0;
    }
    return monitors_.front().id;
}

} // namespace mimic
