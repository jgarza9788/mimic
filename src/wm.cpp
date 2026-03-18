#include "wm.hpp"

#include <X11/keysym.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <string>

#include "util/log.hpp"
#include "x11/events.hpp"

namespace scrollwm {

namespace {

uint16_t parse_mod_mask(const std::string& mod_key) {
  if (mod_key == "Mod1") {
    return XCB_MOD_MASK_1;
  }
  if (mod_key == "Mod4") {
    return XCB_MOD_MASK_4;
  }
  return XCB_MOD_MASK_4;
}

constexpr std::array<uint16_t, 4> kIgnoredLocks = {
    0,
    XCB_MOD_MASK_LOCK,
    XCB_MOD_MASK_2,
    static_cast<uint16_t>(XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2),
};

}  // namespace

WM::WM()
    : config_(config::load_from_path(config::default_config_path())),
      layout_engine_(config_.layout_direction) {
  for (int i = 0; i < config_.workspace_count; ++i) {
    workspaces_.emplace_back(i);
  }
  if (workspaces_.empty()) {
    workspaces_.emplace_back(0);
  }
}

int WM::run() {
  if (!setup()) {
    return 1;
  }

  util::log(util::LogLevel::Info, "scrollwm started");
  event_loop();

  if (key_symbols_ != nullptr) {
    xcb_key_symbols_free(key_symbols_);
  }
  util::log(util::LogLevel::Info, "scrollwm stopped");
  return 0;
}

bool WM::setup() {
  if (!connection_.valid()) {
    util::log(util::LogLevel::Error, "failed to open X11 connection");
    return false;
  }

  atoms_ = x11::create_atoms(connection_.raw());
  key_symbols_ = xcb_key_symbols_alloc(connection_.raw());
  if (key_symbols_ == nullptr) {
    util::log(util::LogLevel::Error, "failed to allocate key symbol table");
    return false;
  }

  setup_root_events();

  xcb_generic_error_t* err = nullptr;
  const uint32_t root_mask[] = {XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                                 XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                                 XCB_EVENT_MASK_ENTER_WINDOW |
                                 XCB_EVENT_MASK_PROPERTY_CHANGE};
  xcb_void_cookie_t check_cookie = xcb_change_window_attributes_checked(
      connection_.raw(), connection_.screen()->root, XCB_CW_EVENT_MASK, root_mask);

  err = xcb_request_check(connection_.raw(), check_cookie);
  if (err != nullptr) {
    util::log(util::LogLevel::Error,
              "could not acquire WM ownership (is another WM running?)");
    free(err);
    return false;
  }

  setup_ewmh();
  setup_keys();
  manage_existing_windows();
  relayout();
  xcb_flush(connection_.raw());
  return true;
}

void WM::setup_root_events() {
  uint32_t values[] = {XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                       XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                       XCB_EVENT_MASK_ENTER_WINDOW |
                       XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                       XCB_EVENT_MASK_PROPERTY_CHANGE};
  xcb_change_window_attributes(connection_.raw(), connection_.screen()->root,
                               XCB_CW_EVENT_MASK, values);
}

void WM::setup_ewmh() {
  set_desktop_properties();
  set_client_list_property();
}

void WM::setup_keys() {
  const uint16_t mod = parse_mod_mask(config_.mod_key);
  bindings_ = {
      {mod, XK_j, KeyBinding::Action::FocusNext},
      {mod, XK_k, KeyBinding::Action::FocusPrev},
      {mod, XK_Return, KeyBinding::Action::SpawnTerminal},
      {mod, XK_q, KeyBinding::Action::CloseFocused},
      {static_cast<uint16_t>(mod | XCB_MOD_MASK_SHIFT), XK_e, KeyBinding::Action::Exit},
      {mod, XK_1, KeyBinding::Action::Workspace1},
      {mod, XK_2, KeyBinding::Action::Workspace2},
      {mod, XK_3, KeyBinding::Action::Workspace3},
      {mod, XK_4, KeyBinding::Action::Workspace4},
  };

  for (const auto& binding : bindings_) {
    xcb_keycode_t* keycodes = xcb_key_symbols_get_keycode(key_symbols_, binding.keysym);
    if (keycodes == nullptr) {
      continue;
    }

    for (int i = 0; keycodes[i] != XCB_NO_SYMBOL; ++i) {
      for (uint16_t ignored : kIgnoredLocks) {
        xcb_grab_key(connection_.raw(), 1, connection_.screen()->root,
                     static_cast<uint16_t>(binding.modifiers | ignored), keycodes[i],
                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
      }
    }
    free(keycodes);
  }
}

void WM::event_loop() {
  while (running_) {
    xcb_generic_event_t* event = xcb_wait_for_event(connection_.raw());
    if (event == nullptr) {
      continue;
    }
    handle_event(event);
    free(event);
    xcb_flush(connection_.raw());
  }
}

void WM::handle_event(xcb_generic_event_t* event) {
  switch (x11::event_type(event)) {
    case XCB_MAP_REQUEST:
      handle_map_request(*reinterpret_cast<xcb_map_request_event_t*>(event));
      break;
    case XCB_UNMAP_NOTIFY:
      handle_unmap_notify(*reinterpret_cast<xcb_unmap_notify_event_t*>(event));
      break;
    case XCB_DESTROY_NOTIFY:
      handle_destroy_notify(*reinterpret_cast<xcb_destroy_notify_event_t*>(event));
      break;
    case XCB_CONFIGURE_REQUEST:
      handle_configure_request(*reinterpret_cast<xcb_configure_request_event_t*>(event));
      break;
    case XCB_ENTER_NOTIFY:
      handle_enter_notify(*reinterpret_cast<xcb_enter_notify_event_t*>(event));
      break;
    case XCB_KEY_PRESS:
      handle_key_press(*reinterpret_cast<xcb_key_press_event_t*>(event));
      break;
    case XCB_CLIENT_MESSAGE:
      handle_client_message(*reinterpret_cast<xcb_client_message_event_t*>(event));
      break;
    default:
      break;
  }
}

void WM::handle_map_request(const xcb_map_request_event_t& event) {
  add_client(event.window);
  xcb_map_window(connection_.raw(), event.window);
  relayout();
}

void WM::handle_unmap_notify(const xcb_unmap_notify_event_t& event) {
  remove_client(event.window);
}

void WM::handle_destroy_notify(const xcb_destroy_notify_event_t& event) {
  remove_client(event.window);
}

void WM::handle_configure_request(const xcb_configure_request_event_t& event) {
  constexpr uint16_t tiled_mask = XCB_CONFIG_WINDOW_BORDER_WIDTH;
  uint16_t mask = event.value_mask;
  if ((mask & tiled_mask) != 0U) {
    mask &= static_cast<uint16_t>(~tiled_mask);
  }

  uint32_t values[7];
  int idx = 0;
  if ((mask & XCB_CONFIG_WINDOW_X) != 0U) values[idx++] = static_cast<uint32_t>(event.x);
  if ((mask & XCB_CONFIG_WINDOW_Y) != 0U) values[idx++] = static_cast<uint32_t>(event.y);
  if ((mask & XCB_CONFIG_WINDOW_WIDTH) != 0U) values[idx++] = static_cast<uint32_t>(event.width);
  if ((mask & XCB_CONFIG_WINDOW_HEIGHT) != 0U) values[idx++] = static_cast<uint32_t>(event.height);
  if ((mask & XCB_CONFIG_WINDOW_SIBLING) != 0U) values[idx++] = event.sibling;
  if ((mask & XCB_CONFIG_WINDOW_STACK_MODE) != 0U) values[idx++] = event.stack_mode;

  if (mask != 0 && idx > 0) {
    xcb_configure_window(connection_.raw(), event.window, mask, values);
  }
}

void WM::handle_enter_notify(const xcb_enter_notify_event_t& event) {
  if (!config_.focus_follows_mouse) {
    return;
  }

  auto& ws = current_workspace();
  if (auto idx = find_client_index(ws, event.event); idx.has_value()) {
    ws.focus_index(*idx);
    focus_window(event.event);
    relayout();
  }
}

void WM::handle_key_press(const xcb_key_press_event_t& event) {
  const uint16_t clean_state = static_cast<uint16_t>(event.state & ~(XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2));
  const xcb_keysym_t keysym = xcb_key_symbols_get_keysym(key_symbols_, event.detail, 0);

  for (const auto& binding : bindings_) {
    if (binding.keysym != keysym || binding.modifiers != clean_state) {
      continue;
    }

    switch (binding.action) {
      case KeyBinding::Action::FocusNext:
        focus_next();
        break;
      case KeyBinding::Action::FocusPrev:
        focus_prev();
        break;
      case KeyBinding::Action::SpawnTerminal:
        spawn_command(config_.terminal);
        break;
      case KeyBinding::Action::CloseFocused:
        kill_focused();
        break;
      case KeyBinding::Action::Exit:
        running_ = false;
        break;
      case KeyBinding::Action::Workspace1:
        switch_workspace(0);
        break;
      case KeyBinding::Action::Workspace2:
        switch_workspace(1);
        break;
      case KeyBinding::Action::Workspace3:
        switch_workspace(2);
        break;
      case KeyBinding::Action::Workspace4:
        switch_workspace(3);
        break;
    }
    return;
  }
}

void WM::handle_client_message(const xcb_client_message_event_t& event) {
  if (event.type == atoms_.net_active_window) {
    focus_window(event.window);
    return;
  }

  if (event.type == atoms_.net_current_desktop) {
    switch_workspace(static_cast<int>(event.data.data32[0]));
  }
}

void WM::manage_existing_windows() {
  auto tree_cookie = xcb_query_tree(connection_.raw(), connection_.screen()->root);
  xcb_query_tree_reply_t* tree_reply = xcb_query_tree_reply(connection_.raw(), tree_cookie, nullptr);
  if (tree_reply == nullptr) {
    return;
  }

  const int len = xcb_query_tree_children_length(tree_reply);
  xcb_window_t* windows = xcb_query_tree_children(tree_reply);
  for (int i = 0; i < len; ++i) {
    xcb_get_window_attributes_cookie_t attr_cookie = xcb_get_window_attributes(connection_.raw(), windows[i]);
    xcb_get_window_attributes_reply_t* attrs =
        xcb_get_window_attributes_reply(connection_.raw(), attr_cookie, nullptr);
    if (attrs == nullptr) {
      continue;
    }
    if (!attrs->override_redirect && attrs->map_state == XCB_MAP_STATE_VIEWABLE) {
      add_client(windows[i]);
    }
    free(attrs);
  }

  free(tree_reply);
}

void WM::add_client(xcb_window_t window) {
  auto& ws = current_workspace();
  if (find_client_index(ws, window).has_value()) {
    return;
  }

  uint32_t values[] = {
      static_cast<uint32_t>(XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_PROPERTY_CHANGE),
      static_cast<uint32_t>(config_.border_width),
  };
  xcb_change_window_attributes(connection_.raw(), window, XCB_CW_EVENT_MASK, values);
  xcb_configure_window(connection_.raw(), window, XCB_CONFIG_WINDOW_BORDER_WIDTH, &values[1]);

  ws.add_client(model::Client{.window = window});
  focus_window(window);
  set_client_list_property();
}

void WM::remove_client(xcb_window_t window) {
  for (auto& ws : workspaces_) {
    ws.remove_client(window);
  }
  relayout();
  set_client_list_property();
}

void WM::focus_window(xcb_window_t window) {
  xcb_set_input_focus(connection_.raw(), XCB_INPUT_FOCUS_POINTER_ROOT, window, XCB_CURRENT_TIME);
  xcb_change_property(connection_.raw(), XCB_PROP_MODE_REPLACE, connection_.screen()->root,
                      atoms_.net_active_window, XCB_ATOM_WINDOW, 32, 1, &window);
}

void WM::focus_next() {
  auto& ws = current_workspace();
  ws.focus_next();
  if (auto* client = ws.focused_client(); client != nullptr) {
    focus_window(client->window);
  }
  relayout();
}

void WM::focus_prev() {
  auto& ws = current_workspace();
  ws.focus_prev();
  if (auto* client = ws.focused_client(); client != nullptr) {
    focus_window(client->window);
  }
  relayout();
}

void WM::switch_workspace(int idx) {
  if (idx < 0 || idx >= static_cast<int>(workspaces_.size()) || idx == current_workspace_idx_) {
    return;
  }

  for (const auto& client : current_workspace().clients()) {
    xcb_unmap_window(connection_.raw(), client.window);
  }

  current_workspace_idx_ = idx;
  for (const auto& client : current_workspace().clients()) {
    xcb_map_window(connection_.raw(), client.window);
  }

  set_desktop_properties();
  relayout();
}

void WM::kill_focused() {
  auto* client = current_workspace().focused_client();
  if (client == nullptr) {
    return;
  }

  xcb_client_message_event_t event{};
  event.response_type = XCB_CLIENT_MESSAGE;
  event.window = client->window;
  event.format = 32;
  event.type = atoms_.wm_protocols;
  event.data.data32[0] = atoms_.wm_delete_window;
  event.data.data32[1] = XCB_CURRENT_TIME;

  xcb_send_event(connection_.raw(), 0, client->window, XCB_EVENT_MASK_NO_EVENT,
                 reinterpret_cast<const char*>(&event));
}

void WM::relayout() {
  auto& ws = current_workspace();
  int offset = ws.scroll_offset();
  const auto rects = layout_engine_.compute(
      ws,
      static_cast<int>(connection_.screen()->width_in_pixels),
      static_cast<int>(connection_.screen()->height_in_pixels),
      config_.gap,
      config_.border_width,
      config_.outer_padding,
      offset);
  ws.set_scroll_offset(offset);

  const auto& clients = ws.clients();
  for (size_t i = 0; i < clients.size() && i < rects.size(); ++i) {
    const uint32_t vals[] = {
        static_cast<uint32_t>(rects[i].x),
        static_cast<uint32_t>(rects[i].y),
        static_cast<uint32_t>(rects[i].width),
        static_cast<uint32_t>(rects[i].height),
        static_cast<uint32_t>(config_.border_width),
    };
    xcb_configure_window(connection_.raw(), clients[i].window,
                         XCB_CONFIG_WINDOW_X |
                             XCB_CONFIG_WINDOW_Y |
                             XCB_CONFIG_WINDOW_WIDTH |
                             XCB_CONFIG_WINDOW_HEIGHT |
                             XCB_CONFIG_WINDOW_BORDER_WIDTH,
                         vals);
  }

  if (auto* client = ws.focused_client(); client != nullptr) {
    focus_window(client->window);
  }
}

model::Workspace& WM::current_workspace() { return workspaces_[static_cast<size_t>(current_workspace_idx_)]; }

const model::Workspace& WM::current_workspace() const {
  return workspaces_[static_cast<size_t>(current_workspace_idx_)];
}

std::optional<size_t> WM::find_client_index(const model::Workspace& workspace, xcb_window_t window) const {
  const auto& clients = workspace.clients();
  for (size_t i = 0; i < clients.size(); ++i) {
    if (clients[i].window == window) {
      return i;
    }
  }
  return std::nullopt;
}

void WM::set_client_list_property() {
  std::vector<xcb_window_t> all_windows;
  for (const auto& ws : workspaces_) {
    for (const auto& client : ws.clients()) {
      all_windows.push_back(client.window);
    }
  }

  xcb_change_property(connection_.raw(), XCB_PROP_MODE_REPLACE, connection_.screen()->root,
                      atoms_.net_client_list, XCB_ATOM_WINDOW, 32,
                      static_cast<uint32_t>(all_windows.size()), all_windows.data());
}

void WM::set_desktop_properties() {
  const uint32_t desktop_count = static_cast<uint32_t>(workspaces_.size());
  const uint32_t current_desktop = static_cast<uint32_t>(current_workspace_idx_);

  xcb_change_property(connection_.raw(), XCB_PROP_MODE_REPLACE, connection_.screen()->root,
                      atoms_.net_number_of_desktops, XCB_ATOM_CARDINAL, 32, 1,
                      &desktop_count);
  xcb_change_property(connection_.raw(), XCB_PROP_MODE_REPLACE, connection_.screen()->root,
                      atoms_.net_current_desktop, XCB_ATOM_CARDINAL, 32, 1,
                      &current_desktop);
}

void WM::spawn_command(const std::string& cmd) const {
  if (cmd.empty()) {
    return;
  }

  if (fork() == 0) {
    setsid();
    execl("/bin/sh", "sh", "-c", cmd.c_str(), static_cast<char*>(nullptr));
    _exit(127);
  }
}

}  // namespace scrollwm
