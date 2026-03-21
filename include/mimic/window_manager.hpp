#pragma once

#include <unordered_map>

#include <xcb/xcb.h>

#include "mimic/compositor_bridge.hpp"
#include "mimic/config.hpp"
#include "mimic/layout_engine.hpp"
#include "mimic/logger.hpp"
#include "mimic/monitor_manager.hpp"
#include "mimic/rules_engine.hpp"
#include "mimic/workspace_manager.hpp"
#include "mimic/xcb_backend.hpp"

namespace mimic {

/**
 * @brief Core orchestrator tying X11 events to layout and focus behavior.
 */
class WindowManager {
public:
    /**
     * @brief Constructs WM with external dependencies for testability.
     */
    WindowManager(XcbBackend& backend,
                  WorkspaceManager& workspace_manager,
                  MonitorManager& monitor_manager,
                  RulesEngine& rules_engine,
                  CompositorBridge& compositor,
                  Logger& logger,
                  Config config);

    /**
     * @brief Initializes X11 subscriptions and existing-window management.
     */
    bool initialize();

    /**
     * @brief Processes one X event if available.
     */
    void poll_once();

    /**
     * @brief Handles one text command from IPC layer.
     */
    [[nodiscard]] std::string dispatch_command(const std::string& command);

private:
    /**
     * @brief Handles map requests from clients.
     */
    void on_map_request(xcb_map_request_event_t* event);

    /**
     * @brief Handles destroy notifications from clients.
     */
    void on_destroy_notify(xcb_destroy_notify_event_t* event);

    /**
     * @brief Handles key press events for default navigation.
     */
    void on_key_press(xcb_key_press_event_t* event);

    /**
     * @brief Recomputes and applies tiled geometry for active workspace.
     */
    void relayout();

    /**
     * @brief Focuses currently selected tiled window.
     */
    void apply_focus();

    XcbBackend& backend_;
    WorkspaceManager& workspace_manager_;
    MonitorManager& monitor_manager_;
    RulesEngine& rules_engine_;
    CompositorBridge& compositor_;
    Logger& logger_;
    Config config_;
    std::unordered_map<WindowId, ManagedWindow> managed_;
};

} // namespace mimic
