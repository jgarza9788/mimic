#pragma once

#include <filesystem>
#include <functional>

namespace mimic {

/**
 * @brief Poll-based file watcher for automatic config reload behavior.
 */
class ConfigWatcher {
public:
    /**
     * @brief Constructs watcher with a target path and callback.
     */
    ConfigWatcher(std::filesystem::path path, std::function<void()> on_change);

    /**
     * @brief Polls file metadata and invokes callback when modification time changes.
     */
    void poll();

private:
    std::filesystem::path path_;
    std::function<void()> on_change_;
    std::filesystem::file_time_type last_write_;
};

} // namespace mimic
