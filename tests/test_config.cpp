#include <filesystem>
#include <fstream>

#include "mimic/MimicConfig.hh"

MIMIC_TEST(config_candidates_respect_priority_order) {
  const auto candidates = mimic::config_candidates({
      std::optional<std::string>("/tmp/cli.keys"),
      std::optional<std::string>("/tmp/xdg"),
      std::optional<std::string>("/tmp/home"),
      {"/tmp/fallback.keys"},
  });

  MIMIC_ASSERT(candidates.size() == 4);
  MIMIC_ASSERT(candidates[0] == "/tmp/cli.keys");
  MIMIC_ASSERT(candidates[1] == "/tmp/xdg/mimic/mimic.keys");
  MIMIC_ASSERT(candidates[2] == "/tmp/home/.config/mimic/mimic.keys");
  MIMIC_ASSERT(candidates[3] == "/tmp/fallback.keys");
}

MIMIC_TEST(config_search_returns_first_existing_file) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "mimic-config-test";
  std::filesystem::create_directories(dir);
  const auto first = (dir / "first.keys").string();
  const auto second = (dir / "second.keys").string();

  std::ofstream second_file(second);
  second_file << "bind Mod4+Return exec xterm\n";
  second_file.close();

  const auto selected = mimic::first_existing_path({first, second});
  MIMIC_ASSERT(selected.has_value());
  MIMIC_ASSERT(selected.value() == second);

  std::filesystem::remove_all(dir);
}

MIMIC_TEST(toml_config_candidates_respect_priority_order) {
  const auto candidates = mimic::toml_config_candidates(
      std::optional<std::string>("/tmp/xdg"),
      std::optional<std::string>("/tmp/home"),
      {"/tmp/fallback.toml"});

  MIMIC_ASSERT(candidates.size() == 7);
  MIMIC_ASSERT(candidates[0] == "/tmp/xdg/mimic/config.toml");
  MIMIC_ASSERT(candidates[1] == "/tmp/xdg/mimic/config.tml");
  MIMIC_ASSERT(candidates[2] == "/tmp/xdg/mimic/mimic.toml");
  MIMIC_ASSERT(candidates[3] == "/tmp/home/.config/mimic/config.toml");
  MIMIC_ASSERT(candidates[4] == "/tmp/home/.config/mimic/config.tml");
  MIMIC_ASSERT(candidates[5] == "/tmp/home/.config/mimic/mimic.toml");
  MIMIC_ASSERT(candidates[6] == "/tmp/fallback.toml");
}

MIMIC_TEST(runtime_options_are_loaded_from_toml) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "mimic-toml-test";
  std::filesystem::create_directories(dir);
  const auto toml_path = (dir / "mimic.toml").string();

  std::ofstream toml_file(toml_path);
  toml_file << "[runtime]\n";
  toml_file << "log_to_stderr = false\n";
  toml_file << "scan_existing_windows_on_startup = false\n";
  toml_file << "[layout]\n";
  toml_file << "gap_px = 42\n";
  toml_file << "edge_padding_px = 8\n";
  toml_file << "minimum_window_width_px = 720\n";
  toml_file << "primary_window_width_ratio = 0.75\n";
  toml_file << "[input]\n";
  toml_file << "focus_follows_mouse = true\n";
  toml_file << "warp_cursor_on_focus = true\n";
  toml_file << "key_repeat_rate_hz = 25\n";
  toml_file << "key_repeat_delay_ms = 280\n";
  toml_file << "[overview]\n";
  toml_file << "enabled = false\n";
  toml_file << "dim_background = false\n";
  toml_file << "columns = 4\n";
  toml_file << "outer_gap_px = 30\n";
  toml_file << "animation_duration_ms = 220\n";
  toml_file << "[startup]\n";
  toml_file << "default_workspace = 3\n";
  toml_file << "spawn_status_notifier = true\n";
  toml_file << "startup_shell_command = \"waybar\"\n";
  toml_file.close();

  std::optional<std::string> loaded_path;
  const auto options = mimic::load_runtime_options({toml_path}, &loaded_path);

  MIMIC_ASSERT(loaded_path.has_value());
  MIMIC_ASSERT(loaded_path.value() == toml_path);
  MIMIC_ASSERT(options.log_to_stderr == false);
  MIMIC_ASSERT(options.scan_existing_windows_on_startup == false);
  MIMIC_ASSERT(options.layout.gap_px == 42);
  MIMIC_ASSERT(options.layout.edge_padding_px == 8);
  MIMIC_ASSERT(options.layout.minimum_window_width_px == 720);
  MIMIC_ASSERT(options.layout.primary_window_width_ratio == 0.75);
  MIMIC_ASSERT(options.input.focus_follows_mouse == true);
  MIMIC_ASSERT(options.input.warp_cursor_on_focus == true);
  MIMIC_ASSERT(options.input.key_repeat_rate_hz == 25);
  MIMIC_ASSERT(options.input.key_repeat_delay_ms == 280);
  MIMIC_ASSERT(options.overview.enabled == false);
  MIMIC_ASSERT(options.overview.dim_background == false);
  MIMIC_ASSERT(options.overview.columns == 4);
  MIMIC_ASSERT(options.overview.outer_gap_px == 30);
  MIMIC_ASSERT(options.overview.animation_duration_ms == 220);
  MIMIC_ASSERT(options.startup.default_workspace == 3);
  MIMIC_ASSERT(options.startup.spawn_status_notifier == true);
  MIMIC_ASSERT(options.startup.startup_shell_command == "waybar");

  std::filesystem::remove_all(dir);
}
