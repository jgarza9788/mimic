#pragma once

#include <optional>
#include <vector>

#include <xcb/xcb.h>

#include "mimic/types.hpp"

namespace mimic {

/**
 * @brief Thin XCB wrapper for root event subscriptions and window operations.
 */
class XcbBackend {
public:
    /**
     * @brief Initializes XCB connection and root window setup.
     */
    bool connect();

    /**
     * @brief Disconnects from X server.
     */
    ~XcbBackend();

    XcbBackend() = default;
    XcbBackend(const XcbBackend&) = delete;
    XcbBackend& operator=(const XcbBackend&) = delete;

    /**
     * @brief Returns true if backend is connected.
     */
    [[nodiscard]] bool is_connected() const;

    /**
     * @brief Gets root window id.
     */
    [[nodiscard]] xcb_window_t root_window() const;

    /**
     * @brief Grabs common keybindings used by first working slice.
     */
    void grab_default_keys();

    /**
     * @brief Polls next X event.
     */
    [[nodiscard]] xcb_generic_event_t* poll_event();

    /**
     * @brief Focuses a given window.
     */
    void focus_window(xcb_window_t window);

    /**
     * @brief Maps and tiles one window placement.
     */
    void apply_placement(const WindowPlacement& placement);

    /**
     * @brief Flushes pending X requests.
     */
    void flush();

    /**
     * @brief Queries top-level windows currently on root.
     */
    [[nodiscard]] std::vector<xcb_window_t> query_windows() const;

private:
    xcb_connection_t* connection_ {nullptr};
    xcb_screen_t* screen_ {nullptr};
};

} // namespace mimic
