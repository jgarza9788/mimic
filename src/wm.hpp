#pragma once

#include <optional>
#include <unordered_set>
#include <string>
#include <unordered_map>
#include <vector>

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

#include "config/config.hpp"
#include "layout/scroll_layout.hpp"
#include "model/workspace.hpp"
#include "x11/atoms.hpp"
#include "x11/connection.hpp"

namespace scrollwm {

class WM {
 public:
  WM();
  int run();

 private:
  struct KeyBinding {
    uint16_t modifiers;
    xcb_keysym_t keysym;
    enum class Action {
      FocusNext,
      FocusPrev,
      SpawnTerminal,
      CloseFocused,
      Exit,
      SwitchWorkspace,
      MoveToWorkspace,
      ToggleLayoutDirection,
      ReorderNext,
      ReorderPrev,
      ToggleFullscreen,
      ExecCommand,
    } action;
    int workspace_idx = -1;
    std::string command;
  };

  struct SizeConstraints {
    uint32_t min_width = 0;
    uint32_t min_height = 0;
  };
  struct SavedGeometry {
    int16_t x = 0;
    int16_t y = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    bool valid = false;
  };
  struct MonitorView {
    int workspace_idx = 0;
  };

  bool setup();
  void setup_root_events();
  void setup_ewmh();
  void setup_keys();

  void event_loop();
  void handle_event(xcb_generic_event_t* event);
  void handle_map_request(const xcb_map_request_event_t& event);
  void handle_unmap_notify(const xcb_unmap_notify_event_t& event);
  void handle_destroy_notify(const xcb_destroy_notify_event_t& event);
  void handle_configure_request(const xcb_configure_request_event_t& event);
  void handle_enter_notify(const xcb_enter_notify_event_t& event);
  void handle_key_press(const xcb_key_press_event_t& event);
  void handle_client_message(const xcb_client_message_event_t& event);
  void handle_property_notify(const xcb_property_notify_event_t& event);

  void manage_existing_windows();
  void add_client(xcb_window_t window);
  void remove_client(xcb_window_t window);

  void focus_window(xcb_window_t window);
  void focus_next();
  void focus_prev();
  void switch_workspace(int idx);
  void move_focused_to_workspace(int idx);
  void reorder_focused_forward();
  void reorder_focused_backward();
  void toggle_layout_direction();
  void toggle_focused_fullscreen();
  void kill_focused();
  void update_window_state_property(const model::Client& client);
  void clear_urgency(xcb_window_t window);
  bool query_window_urgent(xcb_window_t window) const;
  SavedGeometry query_geometry(xcb_window_t window) const;
  void set_fullscreen(model::Client& client, bool enabled);
  bool is_dialog_window(xcb_window_t window) const;
  bool is_transient_window(xcb_window_t window) const;
  SizeConstraints query_size_constraints(xcb_window_t window) const;
  std::optional<KeyBinding> parse_keybinding(const std::string& combo, KeyBinding::Action action) const;

  void relayout();
  model::Workspace& current_workspace();
  const model::Workspace& current_workspace() const;
  std::optional<size_t> find_client_index(const model::Workspace& workspace, xcb_window_t window) const;

  void set_client_list_property();
  void set_desktop_properties();

  void spawn_command(const std::string& cmd) const;

  x11::Connection connection_;
  x11::Atoms atoms_;
  xcb_key_symbols_t* key_symbols_ = nullptr;
  bool running_ = true;

  config::Config config_;
  layout::ScrollLayout layout_engine_;
  std::vector<model::Workspace> workspaces_;
  std::vector<MonitorView> monitors_;
  int active_monitor_idx_ = 0;

  std::vector<KeyBinding> bindings_;
  std::unordered_map<xcb_window_t, SizeConstraints> size_constraints_;
  std::unordered_map<xcb_window_t, SavedGeometry> saved_geometry_;
};

}  // namespace scrollwm
