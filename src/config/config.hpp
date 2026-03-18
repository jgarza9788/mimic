#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace scrollwm::config {

enum class Direction {
  Horizontal,
  Vertical,
};

struct Config {
  int schema_version = 1;

  struct ExecBinding {
    std::string key;
    std::string command;
  };

  struct BindingSet {
    std::string focus_next = "Mod+J";
    std::string focus_prev = "Mod+K";
    std::string spawn_terminal = "Mod+Enter";
    std::string close_window = "Mod+Q";
    std::string exit_wm = "Mod+Shift+E";
    std::unordered_map<int, std::string> workspace;
    std::unordered_map<int, std::string> move_to_workspace;
    std::string toggle_layout_direction = "Mod+Space";
    std::string reorder_next = "Mod+Shift+J";
    std::string reorder_prev = "Mod+Shift+K";
    std::string toggle_fullscreen = "Mod+F";

    std::string workspace_binding(int one_based_index) const;
    std::string move_to_workspace_binding(int one_based_index) const;
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
  std::vector<ExecBinding> exec_bindings;
};

Config load_default();
Config load_from_path(const std::filesystem::path& path);
std::filesystem::path default_config_path();

}  // namespace scrollwm::config
