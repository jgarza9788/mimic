#include "wm.hpp"

#include <X11/keysym.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include <xcb/xcb_icccm.h>

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

xcb_keysym_t parse_keysym_name(std::string key) {
  std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });

  if (key.size() == 1U) {
    return static_cast<xcb_keysym_t>(std::toupper(static_cast<unsigned char>(key[0])));
  }
  if (key == "enter" || key == "return") {
    return XK_Return;
  }
  if (key == "space") {
    return XK_space;
  }
  if (key == "tab") {
    return XK_Tab;
  }
  if (key.size() >= 2 && key[0] == 'f') {
    const std::string function_part = key.substr(1);
    if (!std::all_of(function_part.begin(), function_part.end(), [](unsigned char c) {
          return std::isdigit(c) != 0;
        })) {
      return XCB_NO_SYMBOL;
    }
    const int fn = std::stoi(function_part);
    if (fn >= 1 && fn <= 12) {
      return static_cast<xcb_keysym_t>(XK_F1 + (fn - 1));
    }
  }

  return XCB_NO_SYMBOL;
}

std::vector<std::string> split_tokens(const std::string& combo) {
  std::vector<std::string> tokens;
  std::stringstream stream(combo);
  std::string token;
  while (std::getline(stream, token, '+')) {
    token.erase(std::remove_if(token.begin(), token.end(), [](unsigned char c) {
                  return std::isspace(c) != 0;
                }),
                token.end());
    if (!token.empty()) {
      tokens.push_back(token);
    }
  }
  return tokens;
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
  std::array<xcb_atom_t, 9> supported = {
      atoms_.net_active_window,
      atoms_.net_client_list,
      atoms_.net_number_of_desktops,
      atoms_.net_current_desktop,
      atoms_.net_desktop_names,
      atoms_.net_wm_desktop,
      atoms_.net_wm_state,
      atoms_.net_wm_state_fullscreen,
      atoms_.net_supported,
  };
  xcb_change_property(connection_.raw(), XCB_PROP_MODE_REPLACE, connection_.screen()->root,
                      atoms_.net_supported, XCB_ATOM_ATOM, 32,
                      static_cast<uint32_t>(supported.size()), supported.data());
  set_desktop_properties();
  set_client_list_property();
}

void WM::setup_keys() {
  bindings_.clear();
  const auto add_binding = [this](const std::string& combo, KeyBinding::Action action) {
    auto parsed = parse_keybinding(combo, action);
    if (parsed.has_value()) {
      bindings_.push_back(*parsed);
    }
  };

  add_binding(config_.bindings.focus_next, KeyBinding::Action::FocusNext);
  add_binding(config_.bindings.focus_prev, KeyBinding::Action::FocusPrev);
  add_binding(config_.bindings.spawn_terminal, KeyBinding::Action::SpawnTerminal);
  add_binding(config_.bindings.close_window, KeyBinding::Action::CloseFocused);
  add_binding(config_.bindings.exit_wm, KeyBinding::Action::Exit);
  add_binding(config_.bindings.workspace_1, KeyBinding::Action::Workspace1);
  add_binding(config_.bindings.workspace_2, KeyBinding::Action::Workspace2);
  add_binding(config_.bindings.workspace_3, KeyBinding::Action::Workspace3);
  add_binding(config_.bindings.workspace_4, KeyBinding::Action::Workspace4);
  add_binding(config_.bindings.move_to_workspace_1, KeyBinding::Action::MoveToWorkspace1);
  add_binding(config_.bindings.move_to_workspace_2, KeyBinding::Action::MoveToWorkspace2);
  add_binding(config_.bindings.move_to_workspace_3, KeyBinding::Action::MoveToWorkspace3);
  add_binding(config_.bindings.move_to_workspace_4, KeyBinding::Action::MoveToWorkspace4);

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
      case KeyBinding::Action::MoveToWorkspace1:
        move_focused_to_workspace(0);
        break;
      case KeyBinding::Action::MoveToWorkspace2:
        move_focused_to_workspace(1);
        break;
      case KeyBinding::Action::MoveToWorkspace3:
        move_focused_to_workspace(2);
        break;
      case KeyBinding::Action::MoveToWorkspace4:
        move_focused_to_workspace(3);
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
    return;
  }

  if (event.type == atoms_.net_wm_desktop) {
    const int target = static_cast<int>(event.data.data32[0]);
    if (target < 0 || target >= static_cast<int>(workspaces_.size())) {
      return;
    }

    for (auto& ws : workspaces_) {
      if (auto idx = find_client_index(ws, event.window); idx.has_value()) {
        model::Client moved = ws.clients()[*idx];
        ws.remove_client(event.window);
        workspaces_[static_cast<size_t>(target)].add_client(moved);
        relayout();
        set_client_list_property();
        return;
      }
    }
  }

  if (event.type == atoms_.net_wm_state) {
    auto& ws = current_workspace();
    if (auto idx = find_client_index(ws, event.window); idx.has_value()) {
      auto& client = ws.clients()[*idx];
      const bool fullscreen_request =
          event.data.data32[1] == atoms_.net_wm_state_fullscreen ||
          event.data.data32[2] == atoms_.net_wm_state_fullscreen;
      if (fullscreen_request) {
        client.fullscreen = event.data.data32[0] != 0;
        update_window_state_property(client);
        relayout();
      }
    }
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

  const bool floating = is_dialog_window(window) || is_transient_window(window);
  const auto hints = query_size_constraints(window);

  uint32_t values[] = {
      static_cast<uint32_t>(XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_PROPERTY_CHANGE),
      static_cast<uint32_t>(config_.border_width),
  };
  xcb_change_window_attributes(connection_.raw(), window, XCB_CW_EVENT_MASK, values);
  xcb_configure_window(connection_.raw(), window, XCB_CONFIG_WINDOW_BORDER_WIDTH, &values[1]);

  ws.add_client(model::Client{.window = window, .floating = floating});
  const uint32_t desktop = static_cast<uint32_t>(current_workspace_idx_);
  xcb_change_property(connection_.raw(), XCB_PROP_MODE_REPLACE, window,
                      atoms_.net_wm_desktop, XCB_ATOM_CARDINAL, 32, 1, &desktop);
  size_constraints_[window] = hints;
  focus_window(window);
  if (floating) {
    uint32_t float_vals[] = {
        static_cast<uint32_t>(connection_.screen()->width_in_pixels / 6),
        static_cast<uint32_t>(connection_.screen()->height_in_pixels / 6),
        std::max(hints.min_width, connection_.screen()->width_in_pixels / 2),
        std::max(hints.min_height, connection_.screen()->height_in_pixels / 2),
    };
    xcb_configure_window(connection_.raw(), window,
                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                             XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                         float_vals);
  }
  set_client_list_property();
}

void WM::remove_client(xcb_window_t window) {
  size_constraints_.erase(window);
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

void WM::move_focused_to_workspace(int idx) {
  if (idx < 0 || idx >= static_cast<int>(workspaces_.size()) || idx == current_workspace_idx_) {
    return;
  }

  auto& from = current_workspace();
  auto* focused = from.focused_client();
  if (focused == nullptr) {
    return;
  }

  const model::Client moved = *focused;
  from.remove_client(focused->window);
  workspaces_[static_cast<size_t>(idx)].add_client(moved);
  const uint32_t desktop = static_cast<uint32_t>(idx);
  xcb_change_property(connection_.raw(), XCB_PROP_MODE_REPLACE, moved.window,
                      atoms_.net_wm_desktop, XCB_ATOM_CARDINAL, 32, 1, &desktop);
  xcb_unmap_window(connection_.raw(), moved.window);
  set_client_list_property();
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

  for (auto& client : ws.clients()) {
    if (client.fullscreen) {
      const uint32_t vals[] = {
          0,
          0,
          static_cast<uint32_t>(connection_.screen()->width_in_pixels),
          static_cast<uint32_t>(connection_.screen()->height_in_pixels),
          0,
      };
      xcb_configure_window(connection_.raw(), client.window,
                           XCB_CONFIG_WINDOW_X |
                               XCB_CONFIG_WINDOW_Y |
                               XCB_CONFIG_WINDOW_WIDTH |
                               XCB_CONFIG_WINDOW_HEIGHT |
                               XCB_CONFIG_WINDOW_BORDER_WIDTH,
                           vals);
      focus_window(client.window);
      return;
    }
  }

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
    if (clients[i].floating) {
      continue;
    }
    uint32_t width = static_cast<uint32_t>(rects[i].width);
    uint32_t height = static_cast<uint32_t>(rects[i].height);
    const auto it = size_constraints_.find(clients[i].window);
    if (it != size_constraints_.end()) {
      width = std::max(width, it->second.min_width);
      height = std::max(height, it->second.min_height);
    }

    const uint32_t vals[] = {
        static_cast<uint32_t>(rects[i].x),
        static_cast<uint32_t>(rects[i].y),
        width,
        height,
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

  std::string names;
  for (uint32_t i = 0; i < desktop_count; ++i) {
    names += "Workspace ";
    names += std::to_string(i + 1);
    names.push_back('\0');
  }

  xcb_change_property(connection_.raw(), XCB_PROP_MODE_REPLACE, connection_.screen()->root,
                      atoms_.net_desktop_names, XCB_ATOM_STRING, 8,
                      static_cast<uint32_t>(names.size()), names.data());
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

void WM::update_window_state_property(const model::Client& client) {
  if (client.fullscreen) {
    const xcb_atom_t fullscreen = atoms_.net_wm_state_fullscreen;
    xcb_change_property(connection_.raw(), XCB_PROP_MODE_REPLACE, client.window,
                        atoms_.net_wm_state, XCB_ATOM_ATOM, 32, 1, &fullscreen);
    return;
  }

  xcb_delete_property(connection_.raw(), client.window, atoms_.net_wm_state);
}

bool WM::is_dialog_window(xcb_window_t window) const {
  auto cookie = xcb_get_property(connection_.raw(), 0, window, atoms_.net_wm_window_type,
                                 XCB_ATOM_ATOM, 0, 32);
  xcb_get_property_reply_t* reply = xcb_get_property_reply(connection_.raw(), cookie, nullptr);
  if (reply == nullptr) {
    return false;
  }

  const xcb_atom_t* atoms = static_cast<xcb_atom_t*>(xcb_get_property_value(reply));
  const int len = xcb_get_property_value_length(reply) / static_cast<int>(sizeof(xcb_atom_t));
  bool is_dialog = false;
  for (int i = 0; i < len; ++i) {
    if (atoms[i] == atoms_.net_wm_window_type_dialog) {
      is_dialog = true;
      break;
    }
  }
  free(reply);
  return is_dialog;
}

bool WM::is_transient_window(xcb_window_t window) const {
  auto cookie = xcb_get_property(connection_.raw(), 0, window, atoms_.wm_transient_for,
                                 XCB_ATOM_WINDOW, 0, 1);
  xcb_get_property_reply_t* reply = xcb_get_property_reply(connection_.raw(), cookie, nullptr);
  if (reply == nullptr) {
    return false;
  }
  const bool is_transient = xcb_get_property_value_length(reply) >= static_cast<int>(sizeof(xcb_window_t));
  free(reply);
  return is_transient;
}

WM::SizeConstraints WM::query_size_constraints(xcb_window_t window) const {
  SizeConstraints constraints;
  xcb_size_hints_t hints{};

  xcb_get_property_cookie_t cookie = xcb_icccm_get_wm_normal_hints(connection_.raw(), window);
  uint8_t supplied = 0;
  if (xcb_icccm_get_wm_normal_hints_reply(connection_.raw(), cookie, &hints, &supplied) == 1) {
    if ((hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) != 0U) {
      constraints.min_width = static_cast<uint32_t>(std::max(0, hints.min_width));
      constraints.min_height = static_cast<uint32_t>(std::max(0, hints.min_height));
    }
  }

  return constraints;
}

std::optional<WM::KeyBinding> WM::parse_keybinding(const std::string& combo,
                                                   KeyBinding::Action action) const {
  uint16_t modifiers = 0;
  xcb_keysym_t keysym = XCB_NO_SYMBOL;

  for (const auto& token : split_tokens(combo)) {
    std::string normalized = token;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });

    if (normalized == "mod") {
      modifiers = static_cast<uint16_t>(modifiers | parse_mod_mask(config_.mod_key));
      continue;
    }
    if (normalized == "shift") {
      modifiers = static_cast<uint16_t>(modifiers | XCB_MOD_MASK_SHIFT);
      continue;
    }
    if (normalized == "control" || normalized == "ctrl") {
      modifiers = static_cast<uint16_t>(modifiers | XCB_MOD_MASK_CONTROL);
      continue;
    }
    if (normalized == "alt" || normalized == "mod1") {
      modifiers = static_cast<uint16_t>(modifiers | XCB_MOD_MASK_1);
      continue;
    }
    if (normalized == "super" || normalized == "mod4") {
      modifiers = static_cast<uint16_t>(modifiers | XCB_MOD_MASK_4);
      continue;
    }

    keysym = parse_keysym_name(normalized);
  }

  if (keysym == XCB_NO_SYMBOL) {
    util::log(util::LogLevel::Warn, "skipping invalid binding: " + combo);
    return std::nullopt;
  }

  return KeyBinding{.modifiers = modifiers, .keysym = keysym, .action = action};
}

}  // namespace scrollwm
