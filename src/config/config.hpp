#pragma once

#include <filesystem>
#include <string>

namespace scrollwm::config {

enum class Direction {
  Horizontal,
  Vertical,
};

struct Config {
  int schema_version = 1;

  struct BindingSet {
    std::string focus_next = "Mod+J";
    std::string focus_prev = "Mod+K";
    std::string spawn_terminal = "Mod+Enter";
    std::string close_window = "Mod+Q";
    std::string exit_wm = "Mod+Shift+E";
    std::string workspace_1 = "Mod+1";
    std::string workspace_2 = "Mod+2";
    std::string workspace_3 = "Mod+3";
    std::string workspace_4 = "Mod+4";
    std::string move_to_workspace_1 = "Mod+Shift+1";
    std::string move_to_workspace_2 = "Mod+Shift+2";
    std::string move_to_workspace_3 = "Mod+Shift+3";
    std::string move_to_workspace_4 = "Mod+Shift+4";
    std::string toggle_layout_direction = "Mod+Space";
    std::string reorder_next = "Mod+Shift+J";
    std::string reorder_prev = "Mod+Shift+K";
    std::string toggle_fullscreen = "Mod+F";
  } bindings;

  std::string mod_key = "Mod4";
  Direction layout_direction = Direction::Horizontal;
  int gap = 12;
  int border_width = 2;
  int outer_padding = 12;
  bool focus_follows_mouse = false;
  int workspace_count = 4;
  std::string terminal = "xterm";
  std::string compositor_cmd;
  bool autostart_picom = false;
};

Config load_default();
Config load_from_path(const std::filesystem::path& path);
std::filesystem::path default_config_path();

}  // namespace scrollwm::config
