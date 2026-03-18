#include "wm.hpp"

#include <X11/keysym.h>
#include <cerrno>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <utility>
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
  monitors_.push_back(MonitorView{.workspace_idx = 0});
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
  for (int i = 0; i < config_.workspace_count; ++i) {
    if (auto parsed = parse_keybinding(config_.bindings.workspace_binding(i + 1), KeyBinding::Action::SwitchWorkspace);
        parsed.has_value()) {
      parsed->workspace_idx = i;
      bindings_.push_back(*parsed);
    }
    if (auto parsed =
            parse_keybinding(config_.bindings.move_to_workspace_binding(i + 1), KeyBinding::Action::MoveToWorkspace);
        parsed.has_value()) {
      parsed->workspace_idx = i;
      bindings_.push_back(*parsed);
    }
  }
  add_binding(config_.bindings.toggle_layout_direction, KeyBinding::Action::ToggleLayoutDirection);
  add_binding(config_.bindings.reorder_next, KeyBinding::Action::ReorderNext);
  add_binding(config_.bindings.reorder_prev, KeyBinding::Action::ReorderPrev);
  add_binding(config_.bindings.toggle_fullscreen, KeyBinding::Action::ToggleFullscreen);
  add_binding(config_.bindings.toggle_overview, KeyBinding::Action::ToggleOverview);
  add_binding(config_.bindings.activate_overview, KeyBinding::Action::ActivateOverviewSelection);
  for (const auto& exec_binding : config_.exec_bindings) {
    if (auto parsed = parse_keybinding(exec_binding.key, KeyBinding::Action::ExecCommand);
        parsed.has_value()) {
      parsed->command = exec_binding.command;
      bindings_.push_back(*parsed);
    }
  }

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
    case XCB_PROPERTY_NOTIFY:
      handle_property_notify(*reinterpret_cast<xcb_property_notify_event_t*>(event));
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
      case KeyBinding::Action::SwitchWorkspace:
        if (binding.workspace_idx >= 0) {
          switch_workspace(binding.workspace_idx);
        }
        break;
      case KeyBinding::Action::MoveToWorkspace:
        if (binding.workspace_idx >= 0) {
          move_focused_to_workspace(binding.workspace_idx);
        }
        break;
      case KeyBinding::Action::ToggleLayoutDirection:
        toggle_layout_direction();
        break;
      case KeyBinding::Action::ReorderNext:
        reorder_focused_forward();
        break;
      case KeyBinding::Action::ReorderPrev:
        reorder_focused_backward();
        break;
      case KeyBinding::Action::ToggleFullscreen:
        toggle_focused_fullscreen();
        break;
      case KeyBinding::Action::ToggleOverview:
        toggle_overview();
        break;
      case KeyBinding::Action::ActivateOverviewSelection:
        activate_overview_selection();
        break;
      case KeyBinding::Action::ExecCommand:
        spawn_command(binding.command);
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
    if (target < 0) {
      return;
    }
    ensure_workspace_exists(target);

    for (auto& ws : workspaces_) {
      if (auto idx = find_client_index(ws, event.window); idx.has_value()) {
        model::Client moved = ws.clients()[*idx];
        ws.remove_client(event.window);
        workspaces_[static_cast<size_t>(target)].add_client(moved);
        cleanup_empty_workspaces();

        const int desktop_idx = find_workspace_of_client(event.window);
        if (desktop_idx >= 0) {
          const uint32_t desktop = static_cast<uint32_t>(desktop_idx);
          xcb_change_property(connection_.raw(), XCB_PROP_MODE_REPLACE, event.window,
                              atoms_.net_wm_desktop, XCB_ATOM_CARDINAL, 32, 1, &desktop);
        }

        relayout();
        set_client_list_property();
        return;
      }
    }
  }

  if (event.type == atoms_.net_wm_state) {
    auto& ws = current_workspace();
    if (auto idx = find_client_index(ws, event.window); idx.has_value()) {
      const bool fullscreen_request =
          event.data.data32[1] == atoms_.net_wm_state_fullscreen ||
          event.data.data32[2] == atoms_.net_wm_state_fullscreen;
      if (fullscreen_request) {
        set_fullscreen(ws.clients()[*idx], event.data.data32[0] != 0);
        relayout();
      }
    }
  }
}

void WM::handle_property_notify(const xcb_property_notify_event_t& event) {
  if (event.atom != XCB_ATOM_WM_HINTS) {
    return;
  }

  for (auto& ws : workspaces_) {
    if (auto idx = find_client_index(ws, event.window); idx.has_value()) {
      ws.clients()[*idx].urgent = query_window_urgent(event.window);
      return;
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

  ws.add_client(model::Client{.window = window, .floating = floating, .urgent = query_window_urgent(window)});
  const uint32_t desktop = static_cast<uint32_t>(monitors_[static_cast<size_t>(active_monitor_idx_)].workspace_idx);
  xcb_change_property(connection_.raw(), XCB_PROP_MODE_REPLACE, window,
                      atoms_.net_wm_desktop, XCB_ATOM_CARDINAL, 32, 1, &desktop);
  size_constraints_[window] = hints;
  focus_window(window);
  if (floating) {
    uint32_t float_vals[] = {
        static_cast<uint32_t>(connection_.screen()->width_in_pixels / 6),
        static_cast<uint32_t>(connection_.screen()->height_in_pixels / 6),
        std::max<uint32_t>(hints.min_width,
                           static_cast<uint32_t>(connection_.screen()->width_in_pixels / 2)),
        std::max<uint32_t>(hints.min_height,
                           static_cast<uint32_t>(connection_.screen()->height_in_pixels / 2)),
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
  saved_geometry_.erase(window);
  for (auto& ws : workspaces_) {
    ws.remove_client(window);
  }
  cleanup_empty_workspaces();
  relayout();
  set_client_list_property();
}

void WM::focus_window(xcb_window_t window) {
  clear_urgency(window);
  for (auto& ws : workspaces_) {
    if (auto idx = find_client_index(ws, window); idx.has_value()) {
      ws.focus_index(*idx);
      break;
    }
  }
  xcb_set_input_focus(connection_.raw(), XCB_INPUT_FOCUS_POINTER_ROOT, window, XCB_CURRENT_TIME);
  xcb_change_property(connection_.raw(), XCB_PROP_MODE_REPLACE, connection_.screen()->root,
                      atoms_.net_active_window, XCB_ATOM_WINDOW, 32, 1, &window);
}

void WM::focus_next() {
  if (overview_state_.active) {
    move_overview_selection(1);
    relayout();
    return;
  }

  auto& ws = current_workspace();
  ws.focus_urgent();
  ws.focus_next();
  if (auto* client = ws.focused_client(); client != nullptr) {
    focus_window(client->window);
  }
  relayout();
}

void WM::focus_prev() {
  if (overview_state_.active) {
    move_overview_selection(-1);
    relayout();
    return;
  }

  auto& ws = current_workspace();
  ws.focus_urgent();
  ws.focus_prev();
  if (auto* client = ws.focused_client(); client != nullptr) {
    focus_window(client->window);
  }
  relayout();
}

void WM::switch_workspace(int idx) {
  if (idx < 0) {
    return;
  }
  ensure_workspace_exists(idx);

  if (overview_state_.active) {
    select_overview_workspace(idx);
    return;
  }

  auto& monitor = monitors_[static_cast<size_t>(active_monitor_idx_)];
  if (idx == monitor.workspace_idx) {
    return;
  }

  for (const auto& client : current_workspace().clients()) {
    xcb_unmap_window(connection_.raw(), client.window);
  }

  monitor.workspace_idx = idx;
  for (const auto& client : current_workspace().clients()) {
    xcb_map_window(connection_.raw(), client.window);
  }

  set_desktop_properties();
  relayout();
}

void WM::move_focused_to_workspace(int idx) {
  if (overview_state_.active) {
    return;
  }

  const int current_workspace_idx =
      monitors_[static_cast<size_t>(active_monitor_idx_)].workspace_idx;
  if (idx < 0 || idx == current_workspace_idx) {
    return;
  }
  ensure_workspace_exists(idx);

  auto& from = current_workspace();
  auto* focused = from.focused_client();
  if (focused == nullptr) {
    return;
  }

  const model::Client moved = *focused;
  from.remove_client(focused->window);
  workspaces_[static_cast<size_t>(idx)].add_client(moved);
  cleanup_empty_workspaces();

  const int desktop_idx = find_workspace_of_client(moved.window);
  if (desktop_idx >= 0) {
    const uint32_t desktop = static_cast<uint32_t>(desktop_idx);
    xcb_change_property(connection_.raw(), XCB_PROP_MODE_REPLACE, moved.window,
                        atoms_.net_wm_desktop, XCB_ATOM_CARDINAL, 32, 1, &desktop);
  }
  xcb_unmap_window(connection_.raw(), moved.window);
  set_client_list_property();
  relayout();
}

void WM::ensure_workspace_exists(int idx) {
  const size_t previous_size = workspaces_.size();
  model::ensure_workspace_exists(workspaces_, idx);
  if (workspaces_.size() != previous_size) {
    set_desktop_properties();
  }
  normalize_overview_after_workspace_change();
}

void WM::cleanup_empty_workspaces() {
  std::vector<int> monitor_indices;
  monitor_indices.reserve(monitors_.size());
  for (const auto& monitor : monitors_) {
    monitor_indices.push_back(monitor.workspace_idx);
  }

  const size_t previous_size = workspaces_.size();
  model::cleanup_empty_workspaces(workspaces_, monitor_indices);

  for (size_t i = 0; i < monitors_.size() && i < monitor_indices.size(); ++i) {
    monitors_[i].workspace_idx = monitor_indices[i];
  }

  if (workspaces_.size() != previous_size) {
    set_desktop_properties();
  }
  normalize_overview_after_workspace_change();
}

int WM::find_workspace_of_client(xcb_window_t window) const {
  for (size_t ws_idx = 0; ws_idx < workspaces_.size(); ++ws_idx) {
    if (find_client_index(workspaces_[ws_idx], window).has_value()) {
      return static_cast<int>(ws_idx);
    }
  }
  return -1;
}

void WM::reorder_focused_forward() {
  auto& ws = current_workspace();
  ws.reorder_focused_forward();
  relayout();
}

void WM::reorder_focused_backward() {
  auto& ws = current_workspace();
  ws.reorder_focused_backward();
  relayout();
}

void WM::toggle_layout_direction() {
  const auto direction = layout_engine_.direction();
  const auto next = direction == config::Direction::Horizontal ? config::Direction::Vertical
                                                               : config::Direction::Horizontal;
  layout_engine_.set_direction(next);
  relayout();
}

void WM::toggle_focused_fullscreen() {
  auto* client = current_workspace().focused_client();
  if (client == nullptr || client->floating) {
    return;
  }
  set_fullscreen(*client, !client->fullscreen);
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

void WM::toggle_overview() {
  if (!overview_state_.active) {
    overview_state_.active = true;
    overview_state_.anchor_workspace_idx =
        monitors_[static_cast<size_t>(active_monitor_idx_)].workspace_idx;
    overview_state_.selected_workspace_idx = overview_state_.anchor_workspace_idx;
    overview_state_.previous_workspace_idx = overview_state_.anchor_workspace_idx;
    overview_state_.selected_client.reset();
    overview_state_.previous_focused_client.reset();

    auto& ws = current_workspace();
    if (auto* focused = ws.focused_client(); focused != nullptr) {
      overview_state_.selected_client = focused->window;
      overview_state_.previous_focused_client = focused->window;
    }

    normalize_overview_after_workspace_change();
    relayout();
    return;
  }

  overview_state_.active = false;
  const int restore_workspace = overview_state_.previous_workspace_idx.value_or(
      monitors_[static_cast<size_t>(active_monitor_idx_)].workspace_idx);
  switch_workspace(restore_workspace);
  if (overview_state_.previous_focused_client.has_value()) {
    focus_window(*overview_state_.previous_focused_client);
  }

  for (size_t ws_idx = 0; ws_idx < workspaces_.size(); ++ws_idx) {
    if (static_cast<int>(ws_idx) == monitors_[static_cast<size_t>(active_monitor_idx_)].workspace_idx) {
      continue;
    }
    for (const auto& client : workspaces_[ws_idx].clients()) {
      xcb_unmap_window(connection_.raw(), client.window);
    }
  }

  relayout();
}

void WM::activate_overview_selection() {
  if (!overview_state_.active) {
    return;
  }

  if (overview_state_.selected_workspace_idx.has_value()) {
    monitors_[static_cast<size_t>(active_monitor_idx_)].workspace_idx = *overview_state_.selected_workspace_idx;
  }
  const auto client = overview_state_.selected_client;
  overview_state_.active = false;

  for (size_t ws_idx = 0; ws_idx < workspaces_.size(); ++ws_idx) {
    if (static_cast<int>(ws_idx) == monitors_[static_cast<size_t>(active_monitor_idx_)].workspace_idx) {
      continue;
    }
    for (const auto& hidden : workspaces_[ws_idx].clients()) {
      xcb_unmap_window(connection_.raw(), hidden.window);
    }
  }

  if (client.has_value()) {
    focus_window(*client);
  }
  set_desktop_properties();
  relayout();
}

void WM::move_overview_selection(int delta) {
  if (!overview_state_.active || delta == 0) {
    return;
  }

  std::vector<std::pair<int, xcb_window_t>> ordered_clients;
  for (const auto& ws : workspaces_) {
    for (const auto& client : ws.clients()) {
      ordered_clients.emplace_back(ws.index(), client.window);
    }
  }

  if (ordered_clients.empty()) {
    normalize_overview_after_workspace_change();
    return;
  }

  size_t selected_idx = 0;
  if (overview_state_.selected_client.has_value()) {
    for (size_t i = 0; i < ordered_clients.size(); ++i) {
      if (ordered_clients[i].second == *overview_state_.selected_client) {
        selected_idx = i;
        break;
      }
    }
  }

  const int size = static_cast<int>(ordered_clients.size());
  int next = static_cast<int>(selected_idx) + delta;
  while (next < 0) {
    next += size;
  }
  next %= size;

  overview_state_.selected_workspace_idx = ordered_clients[static_cast<size_t>(next)].first;
  overview_state_.selected_client = ordered_clients[static_cast<size_t>(next)].second;
}

void WM::select_overview_workspace(int idx) {
  if (!overview_state_.active || idx < 0 || idx >= static_cast<int>(workspaces_.size())) {
    return;
  }

  overview_state_.selected_workspace_idx = idx;
  const auto& ws = workspaces_[static_cast<size_t>(idx)];
  if (auto* focused = ws.focused_client(); focused != nullptr) {
    overview_state_.selected_client = focused->window;
  } else if (!ws.clients().empty()) {
    overview_state_.selected_client = ws.clients().front().window;
  } else {
    overview_state_.selected_client.reset();
  }

  relayout();
}

void WM::normalize_overview_after_workspace_change() {
  overview::normalize_overview_state(overview_state_, workspaces_);
  if (!overview_state_.active) {
    return;
  }

  if (overview_state_.selected_workspace_idx.has_value()) {
    monitors_[static_cast<size_t>(active_monitor_idx_)].workspace_idx = *overview_state_.selected_workspace_idx;
  }
}

void WM::relayout() {
  if (overview_state_.active) {
    normalize_overview_after_workspace_change();

    const int screen_w = static_cast<int>(connection_.screen()->width_in_pixels);
    const int screen_h = static_cast<int>(connection_.screen()->height_in_pixels);
    const int workspace_gap = std::max(0, config_.outer_padding * 2);

    const auto scene = overview::compute_workspace_scene_offsets(
        workspaces_, overview_state_.anchor_workspace_idx, screen_w, screen_h, workspace_gap);
    const auto camera = overview::build_overview_camera(scene, overview_state_.anchor_workspace_idx,
                                                        screen_w, screen_h);
    overview_state_.zoom_factor = camera.scale;

    std::vector<int> offsets;
    offsets.reserve(workspaces_.size());
    for (const auto& ws : workspaces_) {
      offsets.push_back(ws.scroll_offset());
    }

    const auto render_rects = overview::build_overview_render_rects(
        workspaces_, offsets, layout_engine_, camera, scene, screen_w, screen_h,
        config_.gap, config_.border_width, config_.outer_padding);

    for (auto& ws : workspaces_) {
      for (const auto& client : ws.clients()) {
        xcb_map_window(connection_.raw(), client.window);
      }
    }

    for (const auto& rr : render_rects) {
      if (!rr.client.has_value()) {
        continue;
      }
      uint32_t border = static_cast<uint32_t>(config_.border_width);
      if (overview_state_.selected_client.has_value() && *overview_state_.selected_client == *rr.client) {
        border = static_cast<uint32_t>(config_.border_width + 2);
      } else if (overview_state_.selected_workspace_idx.has_value() &&
                 *overview_state_.selected_workspace_idx == rr.workspace_idx) {
        border = static_cast<uint32_t>(config_.border_width + 1);
      }

      const uint32_t vals[] = {
          static_cast<uint32_t>(std::max(0, rr.rect.x)),
          static_cast<uint32_t>(std::max(0, rr.rect.y)),
          static_cast<uint32_t>(std::max(1, rr.rect.width)),
          static_cast<uint32_t>(std::max(1, rr.rect.height)),
          border,
      };
      xcb_configure_window(connection_.raw(), *rr.client,
                           XCB_CONFIG_WINDOW_X |
                               XCB_CONFIG_WINDOW_Y |
                               XCB_CONFIG_WINDOW_WIDTH |
                               XCB_CONFIG_WINDOW_HEIGHT |
                               XCB_CONFIG_WINDOW_BORDER_WIDTH,
                           vals);
    }

    if (overview_state_.selected_client.has_value()) {
      focus_window(*overview_state_.selected_client);
    }
    return;
  }

  auto& ws = current_workspace();

  const auto fullscreen_client = std::find_if(
      ws.clients().begin(), ws.clients().end(), [](const auto& client) { return client.fullscreen; });
  if (fullscreen_client != ws.clients().end()) {
    const uint32_t vals[] = {
        0,
        0,
        static_cast<uint32_t>(connection_.screen()->width_in_pixels),
        static_cast<uint32_t>(connection_.screen()->height_in_pixels),
        0,
    };
    xcb_configure_window(connection_.raw(), fullscreen_client->window,
                         XCB_CONFIG_WINDOW_X |
                             XCB_CONFIG_WINDOW_Y |
                             XCB_CONFIG_WINDOW_WIDTH |
                             XCB_CONFIG_WINDOW_HEIGHT |
                             XCB_CONFIG_WINDOW_BORDER_WIDTH,
                         vals);
    focus_window(fullscreen_client->window);
    return;
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

model::Workspace& WM::current_workspace() {
  const int workspace_idx = monitors_[static_cast<size_t>(active_monitor_idx_)].workspace_idx;
  return workspaces_[static_cast<size_t>(workspace_idx)];
}

const model::Workspace& WM::current_workspace() const {
  const int workspace_idx = monitors_[static_cast<size_t>(active_monitor_idx_)].workspace_idx;
  return workspaces_[static_cast<size_t>(workspace_idx)];
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
    std::transform(ws.clients().begin(), ws.clients().end(), std::back_inserter(all_windows),
                   [](const auto& client) { return client.window; });
  }

  xcb_change_property(connection_.raw(), XCB_PROP_MODE_REPLACE, connection_.screen()->root,
                      atoms_.net_client_list, XCB_ATOM_WINDOW, 32,
                      static_cast<uint32_t>(all_windows.size()), all_windows.data());
}

void WM::set_desktop_properties() {
  const uint32_t desktop_count = static_cast<uint32_t>(workspaces_.size());
  const uint32_t current_desktop = static_cast<uint32_t>(
      monitors_[static_cast<size_t>(active_monitor_idx_)].workspace_idx);

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

  const pid_t pid = fork();
  if (pid == 0) {
    setsid();
    execl("/bin/sh", "sh", "-c", cmd.c_str(), static_cast<char*>(nullptr));
    util::log(util::LogLevel::Error,
              "failed to exec command '" + cmd + "': " + std::strerror(errno));
    _exit(127);
  }
  if (pid < 0) {
    util::log(util::LogLevel::Error,
              "failed to fork for command '" + cmd + "': " + std::strerror(errno));
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

void WM::clear_urgency(xcb_window_t window) {
  for (auto& ws : workspaces_) {
    if (auto idx = find_client_index(ws, window); idx.has_value()) {
      ws.clients()[*idx].urgent = false;
      break;
    }
  }

  xcb_icccm_wm_hints_t hints{};
  xcb_get_property_cookie_t cookie = xcb_icccm_get_wm_hints(connection_.raw(), window);
  if (xcb_icccm_get_wm_hints_reply(connection_.raw(), cookie, &hints, nullptr) == 1) {
    const auto non_urgency_mask = static_cast<int32_t>(~XCB_ICCCM_WM_HINT_X_URGENCY);
    hints.flags &= non_urgency_mask;
    xcb_icccm_set_wm_hints(connection_.raw(), window, &hints);
  }
}

bool WM::query_window_urgent(xcb_window_t window) const {
  xcb_icccm_wm_hints_t hints{};
  xcb_get_property_cookie_t cookie = xcb_icccm_get_wm_hints(connection_.raw(), window);
  if (xcb_icccm_get_wm_hints_reply(connection_.raw(), cookie, &hints, nullptr) != 1) {
    return false;
  }
  return (hints.flags & XCB_ICCCM_WM_HINT_X_URGENCY) != 0U;
}

WM::SavedGeometry WM::query_geometry(xcb_window_t window) const {
  SavedGeometry geometry;
  auto cookie = xcb_get_geometry(connection_.raw(), window);
  xcb_get_geometry_reply_t* reply = xcb_get_geometry_reply(connection_.raw(), cookie, nullptr);
  if (reply == nullptr) {
    return geometry;
  }
  geometry.x = reply->x;
  geometry.y = reply->y;
  geometry.width = reply->width;
  geometry.height = reply->height;
  geometry.valid = true;
  free(reply);
  return geometry;
}

void WM::set_fullscreen(model::Client& client, bool enabled) {
  if (enabled) {
    for (auto& other : current_workspace().clients()) {
      if (other.window != client.window && other.fullscreen) {
        other.fullscreen = false;
        update_window_state_property(other);
        const auto it = saved_geometry_.find(other.window);
        if (it != saved_geometry_.end() && it->second.valid) {
          const auto& g = it->second;
          const uint32_t vals[] = {static_cast<uint32_t>(g.x), static_cast<uint32_t>(g.y), g.width, g.height,
                                   static_cast<uint32_t>(config_.border_width)};
          xcb_configure_window(connection_.raw(), other.window,
                               XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH |
                                   XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH,
                               vals);
        }
      }
    }
    saved_geometry_[client.window] = query_geometry(client.window);
    client.fullscreen = true;
    update_window_state_property(client);
    return;
  }

  client.fullscreen = false;
  update_window_state_property(client);
  const auto it = saved_geometry_.find(client.window);
  if (it != saved_geometry_.end() && it->second.valid) {
    const auto& g = it->second;
    const uint32_t vals[] = {
        static_cast<uint32_t>(g.x),
        static_cast<uint32_t>(g.y),
        g.width,
        g.height,
        static_cast<uint32_t>(config_.border_width),
    };
    xcb_configure_window(connection_.raw(), client.window,
                         XCB_CONFIG_WINDOW_X |
                             XCB_CONFIG_WINDOW_Y |
                             XCB_CONFIG_WINDOW_WIDTH |
                             XCB_CONFIG_WINDOW_HEIGHT |
                             XCB_CONFIG_WINDOW_BORDER_WIDTH,
                         vals);
  }
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
  if (xcb_icccm_get_wm_normal_hints_reply(connection_.raw(), cookie, &hints, nullptr) == 1) {
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

  return KeyBinding{
      .modifiers = modifiers,
      .keysym = keysym,
      .action = action,
      .workspace_idx = -1,
      .command = {},
  };
}

}  // namespace scrollwm
