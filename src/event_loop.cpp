#include "mimic/event_loop.hpp"

#include <chrono>
#include <thread>

namespace mimic {

void EventLoop::add_task(std::function<void()> task) {
    tasks_.push_back(std::move(task));
}

void EventLoop::run() {
    running_ = true;
    while (running_) {
        for (auto& task : tasks_) {
            task();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
}

void EventLoop::stop() {
    running_ = false;
}

} // namespace mimic
