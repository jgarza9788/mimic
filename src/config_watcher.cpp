#include "mimic/config_watcher.hpp"

#include <filesystem>

namespace mimic {

ConfigWatcher::ConfigWatcher(std::filesystem::path path, std::function<void()> on_change)
    : path_(std::move(path)), on_change_(std::move(on_change)) {
    if (std::filesystem::exists(path_)) {
        last_write_ = std::filesystem::last_write_time(path_);
    }
}

void ConfigWatcher::poll() {
    if (!std::filesystem::exists(path_)) {
        return;
    }
    const auto now_write = std::filesystem::last_write_time(path_);
    if (now_write != last_write_) {
        last_write_ = now_write;
        on_change_();
    }
}

} // namespace mimic
