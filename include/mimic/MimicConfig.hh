#pragma once

#include <optional>
#include <string>
#include <vector>

namespace mimic {

struct ConfigSearchInput {
  std::optional<std::string> cli_path;
  std::optional<std::string> xdg_config_home;
  std::optional<std::string> home;
  std::vector<std::string> fallback_paths;
};

struct LayoutOptions {
  int gap_px = 16;
  int edge_padding_px = 0;
  int minimum_window_width_px = 640;
  double primary_window_width_ratio = 0.6;
};

struct InputOptions {
  bool focus_follows_mouse = false;
  bool warp_cursor_on_focus = false;
  int key_repeat_rate_hz = 30;
  int key_repeat_delay_ms = 300;
};

struct OverviewOptions {
  bool enabled = true;
  bool dim_background = true;
  int columns = 3;
  int outer_gap_px = 24;
  int animation_duration_ms = 180;
};

struct StartupOptions {
  int default_workspace = 1;
  bool spawn_status_notifier = false;
  std::string startup_shell_command;
};

struct RuntimeOptions {
  bool log_to_stderr = true;
  bool scan_existing_windows_on_startup = true;
  LayoutOptions layout;
  InputOptions input;
  OverviewOptions overview;
  StartupOptions startup;
};

std::vector<std::string> config_candidates(const ConfigSearchInput& input);
std::optional<std::string> first_existing_path(const std::vector<std::string>& paths);
std::vector<std::string> toml_config_candidates(std::optional<std::string> xdg_config_home,
                                                std::optional<std::string> home,
                                                std::vector<std::string> fallback_paths = {});
RuntimeOptions load_runtime_options(const std::vector<std::string>& candidate_paths,
                                    std::optional<std::string>* loaded_path = nullptr);

}  // namespace mimic
