#include "mimic/window_manager.hpp"

#include <sstream>

namespace mimic {

WindowManager::WindowManager(XcbBackend& backend,
                             WorkspaceManager& workspace_manager,
                             MonitorManager& monitor_manager,
                             RulesEngine& rules_engine,
                             CompositorBridge& compositor,
                             Logger& logger,
                             Config config)
    : backend_(backend),
      workspace_manager_(workspace_manager),
      monitor_manager_(monitor_manager),
      rules_engine_(rules_engine),
      compositor_(compositor),
      logger_(logger),
      config_(std::move(config)) {}

bool WindowManager::initialize() {
    if (!backend_.connect()) {
        logger_.log(LogLevel::Error, "Failed to connect to X server");
        return false;
    }
    compositor_.attach_x11_context(backend_.connection(), backend_.root_window());

    monitor_manager_.set_monitors({MonitorInfo {.id = 0, .x = 0, .y = 0, .width = 1920, .height = 1080}});
    workspace_manager_.ensure_monitor(monitor_manager_.primary_monitor_id());

    backend_.grab_default_keys();
    for (const auto window : backend_.query_windows()) {
        ManagedWindow managed {.id = window};
        if (!rules_engine_.should_float(managed)) {
            workspace_manager_.active(0).layout().insert_after_focus(window);
        }
        managed_[window] = managed;
    }
    relayout();
    apply_focus();
    backend_.flush();
    return true;
}

void WindowManager::poll_once() {
    auto* raw = backend_.poll_event();
    if (!raw) {
        return;
    }

    const auto type = raw->response_type & ~0x80;
    switch (type) {
    case XCB_MAP_REQUEST:
        on_map_request(reinterpret_cast<xcb_map_request_event_t*>(raw));
        break;
    case XCB_DESTROY_NOTIFY:
        on_destroy_notify(reinterpret_cast<xcb_destroy_notify_event_t*>(raw));
        break;
    case XCB_KEY_PRESS:
        on_key_press(reinterpret_cast<xcb_key_press_event_t*>(raw));
        break;
    default:
        break;
    }
    free(raw);
}

std::string WindowManager::dispatch_command(const std::string& command) {
    auto& layout = workspace_manager_.active(monitor_manager_.primary_monitor_id()).layout();

    if (command == "focus left") {
        layout.move_focus(Direction::Left);
        relayout();
        apply_focus();
        return "ok\n";
    }
    if (command == "focus right") {
        layout.move_focus(Direction::Right);
        relayout();
        apply_focus();
        return "ok\n";
    }
    if (command == "query focused") {
        if (auto focused = layout.focused(); focused.has_value()) {
            return std::to_string(focused.value()) + "\n";
        }
        return "none\n";
    }
    if (command == "query windows") {
        std::ostringstream out;
        for (auto id : layout.windows()) {
            out << id << '\n';
        }
        return out.str();
    }
    if (command == "toggle overview") {
        return "todo:overview\n";
    }
    return "unknown command\n";
}

void WindowManager::on_map_request(xcb_map_request_event_t* event) {
    ManagedWindow managed {.id = event->window};
    managed_[event->window] = managed;

    if (!rules_engine_.should_float(managed)) {
        workspace_manager_.active(monitor_manager_.primary_monitor_id()).layout().insert_after_focus(event->window);
    }

    relayout();
    apply_focus();
    backend_.flush();
}

void WindowManager::on_destroy_notify(xcb_destroy_notify_event_t* event) {
    managed_.erase(event->window);
    workspace_manager_.active(monitor_manager_.primary_monitor_id()).layout().remove(event->window);
    relayout();
    apply_focus();
    backend_.flush();
}

void WindowManager::on_key_press(xcb_key_press_event_t* event) {
    auto& layout = workspace_manager_.active(monitor_manager_.primary_monitor_id()).layout();
    if (event->detail == 43) {
        layout.move_focus(Direction::Left);
    } else if (event->detail == 46) {
        layout.move_focus(Direction::Right);
    } else {
        return;
    }
    relayout();
    apply_focus();
    backend_.flush();
}

void WindowManager::relayout() {
    const auto monitor_id = monitor_manager_.primary_monitor_id();
    auto& layout = workspace_manager_.active(monitor_id).layout();
    const auto placements = layout.compute(1920, 1080, config_.gaps);
    for (const auto& placement : placements) {
        backend_.apply_placement(placement);
    }

    const auto focused = layout.focused();
    if (focused.has_value()) {
        compositor_.publish_scroll_offset(0);
    }
}

void WindowManager::apply_focus() {
    auto& layout = workspace_manager_.active(monitor_manager_.primary_monitor_id()).layout();
    if (const auto focused = layout.focused(); focused.has_value()) {
        backend_.focus_window(focused.value());
        compositor_.publish_focus(focused.value());
    }
}

} // namespace mimic
