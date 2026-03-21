#include "mimic/compositor_bridge.hpp"

#include <cstdlib>
#include <cstring>

#include <xcb/xproto.h>

namespace mimic {

void CompositorBridge::attach_x11_context(xcb_connection_t* connection, xcb_window_t root_window) {
    connection_ = connection;
    root_window_ = root_window;
    focused_window_atom_ = XCB_NONE;
    scroll_offset_atom_ = XCB_NONE;
}

void CompositorBridge::publish_focus(WindowId id) const {
    if (!connection_ || root_window_ == XCB_NONE) {
        return;
    }
    if (focused_window_atom_ == XCB_NONE) {
        focused_window_atom_ = intern_atom("_MIMIC_FOCUSED_WINDOW");
    }
    if (focused_window_atom_ == XCB_NONE) {
        return;
    }
    const uint32_t focused = id;
    xcb_change_property(connection_, XCB_PROP_MODE_REPLACE, root_window_, focused_window_atom_, XCB_ATOM_WINDOW, 32,
                        1, &focused);
    xcb_flush(connection_);
}

void CompositorBridge::publish_scroll_offset(int offset) const {
    if (!connection_ || root_window_ == XCB_NONE) {
        return;
    }
    if (scroll_offset_atom_ == XCB_NONE) {
        scroll_offset_atom_ = intern_atom("_MIMIC_SCROLL_OFFSET");
    }
    if (scroll_offset_atom_ == XCB_NONE) {
        return;
    }
    const int32_t scroll = offset;
    xcb_change_property(connection_, XCB_PROP_MODE_REPLACE, root_window_, scroll_offset_atom_, XCB_ATOM_CARDINAL, 32,
                        1, &scroll);
    xcb_flush(connection_);
}

xcb_atom_t CompositorBridge::intern_atom(const char* name) const {
    if (!connection_) {
        return XCB_NONE;
    }
    const auto cookie = xcb_intern_atom(connection_, 0, static_cast<uint16_t>(std::strlen(name)), name);
    xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(connection_, cookie, nullptr);
    if (!reply) {
        return XCB_NONE;
    }
    const xcb_atom_t atom = reply->atom;
    free(reply);
    return atom;
}

} // namespace mimic
