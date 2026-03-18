#pragma once

#include <filesystem>
#include <string>

namespace scrollwm::config {

enum class Direction {
  Horizontal,
  Vertical,
};

struct Config {
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
