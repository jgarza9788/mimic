#include "mimic/xcb_backend.hpp"

#include <xcb/xproto.h>

#include <cstdlib>

namespace mimic {

bool XcbBackend::connect() {
    int screen_number = 0;
    connection_ = xcb_connect(nullptr, &screen_number);
    if (xcb_connection_has_error(connection_)) {
        return false;
    }

    auto setup = xcb_get_setup(connection_);
    auto iter = xcb_setup_roots_iterator(setup);
    for (int i = 0; i < screen_number; ++i) {
        xcb_screen_next(&iter);
    }
    screen_ = iter.data;

    const uint32_t event_mask = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                                XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                                XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                                XCB_EVENT_MASK_PROPERTY_CHANGE |
                                XCB_EVENT_MASK_BUTTON_PRESS;
    xcb_change_window_attributes(connection_, screen_->root, XCB_CW_EVENT_MASK, &event_mask);
    xcb_flush(connection_);
    return true;
}

XcbBackend::~XcbBackend() {
    if (connection_ != nullptr) {
        xcb_disconnect(connection_);
    }
}

bool XcbBackend::is_connected() const {
    return connection_ != nullptr && !xcb_connection_has_error(connection_);
}

xcb_window_t XcbBackend::root_window() const {
    return screen_ ? screen_->root : XCB_NONE;
}

void XcbBackend::grab_default_keys() {
    if (!connection_ || !screen_) {
        return;
    }
    // Mod4+h and Mod4+l (keycodes commonly 43 and 46 on US layouts).
    xcb_grab_key(connection_, 1, screen_->root, XCB_MOD_MASK_4, 43, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    xcb_grab_key(connection_, 1, screen_->root, XCB_MOD_MASK_4, 46, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    xcb_flush(connection_);
}

xcb_generic_event_t* XcbBackend::poll_event() {
    if (!connection_) {
        return nullptr;
    }
    return xcb_poll_for_event(connection_);
}

void XcbBackend::focus_window(xcb_window_t window) {
    if (!connection_) {
        return;
    }
    xcb_set_input_focus(connection_, XCB_INPUT_FOCUS_POINTER_ROOT, window, XCB_CURRENT_TIME);
    const uint32_t stack_mode[] = {XCB_STACK_MODE_ABOVE};
    xcb_configure_window(connection_, window, XCB_CONFIG_WINDOW_STACK_MODE, stack_mode);
}

void XcbBackend::apply_placement(const WindowPlacement& placement) {
    if (!connection_) {
        return;
    }
    const uint32_t values[] = {static_cast<uint32_t>(placement.x), static_cast<uint32_t>(placement.y),
                               static_cast<uint32_t>(placement.width), static_cast<uint32_t>(placement.height)};
    xcb_configure_window(connection_, placement.id,
                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH |
                             XCB_CONFIG_WINDOW_HEIGHT,
                         values);
    xcb_map_window(connection_, placement.id);
}

void XcbBackend::flush() {
    if (connection_) {
        xcb_flush(connection_);
    }
}

std::vector<xcb_window_t> XcbBackend::query_windows() const {
    std::vector<xcb_window_t> windows;
    if (!connection_ || !screen_) {
        return windows;
    }

    auto cookie = xcb_query_tree(connection_, screen_->root);
    auto* reply = xcb_query_tree_reply(connection_, cookie, nullptr);
    if (!reply) {
        return windows;
    }

    const int len = xcb_query_tree_children_length(reply);
    auto* children = xcb_query_tree_children(reply);
    for (int i = 0; i < len; ++i) {
        windows.push_back(children[i]);
    }
    free(reply);
    return windows;
}

} // namespace mimic
