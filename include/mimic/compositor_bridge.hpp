#pragma once

#include <cstdint>

#include <xcb/xcb.h>

#include "mimic/types.hpp"

namespace mimic {

/**
 * @brief Future-facing compositor integration facade for picom metadata.
 */
class CompositorBridge {
public:
    /**
     * @brief Binds bridge publication to an active X11 connection and root window.
     */
    void attach_x11_context(xcb_connection_t* connection, xcb_window_t root_window);

    /**
     * @brief Publishes current focus and layout context for compositor consumers.
     */
    void publish_focus(WindowId id) const;

    /**
     * @brief Publishes workspace scroll offset for animation-aware compositors.
     */
    void publish_scroll_offset(int offset) const;

private:
    [[nodiscard]] xcb_atom_t intern_atom(const char* name) const;

    xcb_connection_t* connection_ {nullptr};
    xcb_window_t root_window_ {XCB_NONE};
    mutable xcb_atom_t focused_window_atom_ {XCB_NONE};
    mutable xcb_atom_t scroll_offset_atom_ {XCB_NONE};
};

} // namespace mimic
