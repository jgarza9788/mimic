#include "mimic/MimicConfig.hh"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace {

std::string trim(std::string value) {
  value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char c) { return !std::isspace(c); }));
  value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char c) { return !std::isspace(c); }).base(),
              value.end());
  return value;
}

std::optional<bool> parse_toml_bool(const std::string& value) {
  if (value == "true") {
    return true;
  }

  if (value == "false") {
    return false;
  }

  return std::nullopt;
}

std::optional<int> parse_toml_int(const std::string& value) {
  if (value.empty()) {
    return std::nullopt;
  }

  int number = 0;
  const auto* begin = value.data();
  const auto* end = begin + value.size();
  const auto result = std::from_chars(begin, end, number);
  if (result.ec != std::errc() || result.ptr != end) {
    return std::nullopt;
  }

  return number;
}

std::optional<double> parse_toml_double(const std::string& value) {
  if (value.empty()) {
    return std::nullopt;
  }

  char* parse_end = nullptr;
  const double number = std::strtod(value.c_str(), &parse_end);
  if (parse_end != value.c_str() + value.size()) {
    return std::nullopt;
  }

  return number;
}

std::optional<std::string> parse_toml_string(const std::string& value) {
  if (value.size() < 2 || value.front() != '"' || value.back() != '"') {
    return std::nullopt;
  }

  return value.substr(1, value.size() - 2);
}

}  // namespace

namespace mimic {

std::vector<std::string> config_candidates(const ConfigSearchInput& input) {
  std::vector<std::string> paths;

  if (input.cli_path && !input.cli_path->empty()) {
    paths.push_back(*input.cli_path);
  }

  if (input.xdg_config_home && !input.xdg_config_home->empty()) {
    paths.push_back(*input.xdg_config_home + "/mimic/mimic.keys");
  }

  if (input.home && !input.home->empty()) {
    paths.push_back(*input.home + "/.config/mimic/mimic.keys");
  }

  for (const auto& fallback : input.fallback_paths) {
    paths.push_back(fallback);
  }

  return paths;
}

std::optional<std::string> first_existing_path(const std::vector<std::string>& paths) {
  for (const auto& path : paths) {
    if (std::filesystem::exists(path)) {
      return path;
    }
  }

  return std::nullopt;
}

std::vector<std::string> toml_config_candidates(std::optional<std::string> xdg_config_home,
                                                std::optional<std::string> home,
                                                std::vector<std::string> fallback_paths) {
  std::vector<std::string> paths;

  if (xdg_config_home && !xdg_config_home->empty()) {
    paths.push_back(*xdg_config_home + "/mimic/config.toml");
    paths.push_back(*xdg_config_home + "/mimic/config.tml");
    paths.push_back(*xdg_config_home + "/mimic/mimic.toml");
  }

  if (home && !home->empty()) {
    paths.push_back(*home + "/.config/mimic/config.toml");
    paths.push_back(*home + "/.config/mimic/config.tml");
    paths.push_back(*home + "/.config/mimic/mimic.toml");
  }

  paths.insert(paths.end(), fallback_paths.begin(), fallback_paths.end());
  return paths;
}

RuntimeOptions load_runtime_options(const std::vector<std::string>& candidate_paths,
                                    std::optional<std::string>* loaded_path) {
  RuntimeOptions options;
  const auto path = first_existing_path(candidate_paths);
  if (!path) {
    return options;
  }

  if (loaded_path != nullptr) {
    *loaded_path = path;
  }

  std::ifstream input(*path);
  std::string line;
  std::string current_section;

  while (std::getline(input, line)) {
    const auto comment_pos = line.find('#');
    if (comment_pos != std::string::npos) {
      line = line.substr(0, comment_pos);
    }

    line = trim(line);
    if (line.empty()) {
      continue;
    }

    if (line.front() == '[' && line.back() == ']') {
      current_section = trim(line.substr(1, line.size() - 2));
      continue;
    }

    const auto eq_pos = line.find('=');
    if (eq_pos == std::string::npos) {
      continue;
    }

    const auto key = trim(line.substr(0, eq_pos));
    const auto value = trim(line.substr(eq_pos + 1));
    if (current_section == "runtime" && key == "log_to_stderr") {
      if (const auto parsed_bool = parse_toml_bool(value); parsed_bool.has_value()) {
        options.log_to_stderr = *parsed_bool;
      }
      continue;
    }

    if (current_section == "runtime" && key == "scan_existing_windows_on_startup") {
      if (const auto parsed_bool = parse_toml_bool(value); parsed_bool.has_value()) {
        options.scan_existing_windows_on_startup = *parsed_bool;
      }
      continue;
    }

    if (current_section == "layout" && key == "gap_px") {
      if (const auto parsed_int = parse_toml_int(value); parsed_int.has_value()) {
        options.layout.gap_px = *parsed_int;
      }
      continue;
    }

    if (current_section == "layout" && key == "edge_padding_px") {
      if (const auto parsed_int = parse_toml_int(value); parsed_int.has_value()) {
        options.layout.edge_padding_px = *parsed_int;
      }
      continue;
    }

    if (current_section == "layout" && key == "minimum_window_width_px") {
      if (const auto parsed_int = parse_toml_int(value); parsed_int.has_value()) {
        options.layout.minimum_window_width_px = *parsed_int;
      }
      continue;
    }

    if (current_section == "layout" && key == "primary_window_width_ratio") {
      if (const auto parsed_double = parse_toml_double(value); parsed_double.has_value()) {
        options.layout.primary_window_width_ratio = *parsed_double;
      }
      continue;
    }

    if (current_section == "input" && key == "focus_follows_mouse") {
      if (const auto parsed_bool = parse_toml_bool(value); parsed_bool.has_value()) {
        options.input.focus_follows_mouse = *parsed_bool;
      }
      continue;
    }

    if (current_section == "input" && key == "warp_cursor_on_focus") {
      if (const auto parsed_bool = parse_toml_bool(value); parsed_bool.has_value()) {
        options.input.warp_cursor_on_focus = *parsed_bool;
      }
      continue;
    }

    if (current_section == "input" && key == "key_repeat_rate_hz") {
      if (const auto parsed_int = parse_toml_int(value); parsed_int.has_value()) {
        options.input.key_repeat_rate_hz = *parsed_int;
      }
      continue;
    }

    if (current_section == "input" && key == "key_repeat_delay_ms") {
      if (const auto parsed_int = parse_toml_int(value); parsed_int.has_value()) {
        options.input.key_repeat_delay_ms = *parsed_int;
      }
      continue;
    }

    if (current_section == "overview" && key == "enabled") {
      if (const auto parsed_bool = parse_toml_bool(value); parsed_bool.has_value()) {
        options.overview.enabled = *parsed_bool;
      }
      continue;
    }

    if (current_section == "overview" && key == "dim_background") {
      if (const auto parsed_bool = parse_toml_bool(value); parsed_bool.has_value()) {
        options.overview.dim_background = *parsed_bool;
      }
      continue;
    }

    if (current_section == "overview" && key == "columns") {
      if (const auto parsed_int = parse_toml_int(value); parsed_int.has_value()) {
        options.overview.columns = *parsed_int;
      }
      continue;
    }

    if (current_section == "overview" && key == "outer_gap_px") {
      if (const auto parsed_int = parse_toml_int(value); parsed_int.has_value()) {
        options.overview.outer_gap_px = *parsed_int;
      }
      continue;
    }

    if (current_section == "overview" && key == "animation_duration_ms") {
      if (const auto parsed_int = parse_toml_int(value); parsed_int.has_value()) {
        options.overview.animation_duration_ms = *parsed_int;
      }
      continue;
    }

    if (current_section == "startup" && key == "default_workspace") {
      if (const auto parsed_int = parse_toml_int(value); parsed_int.has_value()) {
        options.startup.default_workspace = *parsed_int;
      }
      continue;
    }

    if (current_section == "startup" && key == "spawn_status_notifier") {
      if (const auto parsed_bool = parse_toml_bool(value); parsed_bool.has_value()) {
        options.startup.spawn_status_notifier = *parsed_bool;
      }
      continue;
    }

    if (current_section == "startup" && key == "startup_shell_command") {
      if (const auto parsed_string = parse_toml_string(value); parsed_string.has_value()) {
        options.startup.startup_shell_command = *parsed_string;
      }
      continue;
    }
  }

  return options;
}

}  // namespace mimic
