#pragma once

#include <atomic>
#include <functional>
#include <vector>

namespace mimic {

/**
 * @brief Cooperative event loop for periodic poll-style tasks.
 */
class EventLoop {
public:
    /**
     * @brief Registers a polling callback executed every tick.
     */
    void add_task(std::function<void()> task);

    /**
     * @brief Runs loop until stop is requested.
     */
    void run();

    /**
     * @brief Requests graceful loop stop.
     */
    void stop();

private:
    std::vector<std::function<void()>> tasks_;
    std::atomic<bool> running_ {false};
};

} // namespace mimic
