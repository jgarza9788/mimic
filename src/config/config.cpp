#include "config/config.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace scrollwm::config {

namespace {

std::string trim(const std::string& value) {
  const auto begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return {};
  }
  const auto end = value.find_last_not_of(" \t\r\n");
  return value.substr(begin, end - begin + 1);
}

std::string strip_quotes(std::string value) {
  value = trim(value);
  if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                            (value.front() == '\'' && value.back() == '\''))) {
    return value.substr(1, value.size() - 2);
  }
  return value;
}

bool to_bool(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value == "true" || value == "1" || value == "yes";
}

Direction to_direction(const std::string& value) {
  auto normalized = value;
  std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (normalized == "vertical") {
    return Direction::Vertical;
  }
  return Direction::Horizontal;
}

}  // namespace

Config load_default() {
  return Config{};
}

std::filesystem::path default_config_path() {
  const char* home = std::getenv("HOME");
  if (home == nullptr) {
    return ".config/scrollwm/config.toml";
  }
  return std::filesystem::path(home) / ".config" / "scrollwm" / "config.toml";
}

Config load_from_path(const std::filesystem::path& path) {
  Config cfg = load_default();

  std::ifstream file(path);
  if (!file.good()) {
    return cfg;
  }

  std::string current_section;
  std::string line;
  while (std::getline(file, line)) {
    const auto comment_pos = line.find('#');
    if (comment_pos != std::string::npos) {
      line.erase(comment_pos);
    }
    line = trim(line);
    if (line.empty()) {
      continue;
    }

    if (line.front() == '[' && line.back() == ']') {
      current_section = trim(line.substr(1, line.size() - 2));
      continue;
    }

    const auto equal_pos = line.find('=');
    if (equal_pos == std::string::npos) {
      continue;
    }

    const std::string key = trim(line.substr(0, equal_pos));
    const std::string raw_value = trim(line.substr(equal_pos + 1));
    const std::string qualified_key = current_section.empty() ? key : current_section + "." + key;

    if (qualified_key == "general.mod_key") {
      cfg.mod_key = strip_quotes(raw_value);
    } else if (qualified_key == "schema.version") {
      cfg.schema_version = std::stoi(raw_value);
    } else if (qualified_key == "general.workspace_count") {
      cfg.workspace_count = std::stoi(raw_value);
    } else if (qualified_key == "general.focus_follows_mouse") {
      cfg.focus_follows_mouse = to_bool(raw_value);
    } else if (qualified_key == "layout.layout_direction") {
      cfg.layout_direction = to_direction(strip_quotes(raw_value));
    } else if (qualified_key == "appearance.gap") {
      cfg.gap = std::stoi(raw_value);
    } else if (qualified_key == "appearance.border_width") {
      cfg.border_width = std::stoi(raw_value);
    } else if (qualified_key == "appearance.outer_padding") {
      cfg.outer_padding = std::stoi(raw_value);
    } else if (qualified_key == "general.terminal") {
      cfg.terminal = strip_quotes(raw_value);
    } else if (qualified_key == "autostart.compositor") {
      cfg.compositor_cmd = strip_quotes(raw_value);
    } else if (qualified_key == "autostart.launch_picom") {
      cfg.autostart_picom = to_bool(raw_value);
    } else if (qualified_key == "bindings.focus_next") {
      cfg.bindings.focus_next = strip_quotes(raw_value);
    } else if (qualified_key == "bindings.focus_prev") {
      cfg.bindings.focus_prev = strip_quotes(raw_value);
    } else if (qualified_key == "bindings.spawn_terminal") {
      cfg.bindings.spawn_terminal = strip_quotes(raw_value);
    } else if (qualified_key == "bindings.close_window") {
      cfg.bindings.close_window = strip_quotes(raw_value);
    } else if (qualified_key == "bindings.exit_wm") {
      cfg.bindings.exit_wm = strip_quotes(raw_value);
    } else if (qualified_key == "bindings.workspace_1") {
      cfg.bindings.workspace_1 = strip_quotes(raw_value);
    } else if (qualified_key == "bindings.workspace_2") {
      cfg.bindings.workspace_2 = strip_quotes(raw_value);
    } else if (qualified_key == "bindings.workspace_3") {
      cfg.bindings.workspace_3 = strip_quotes(raw_value);
    } else if (qualified_key == "bindings.workspace_4") {
      cfg.bindings.workspace_4 = strip_quotes(raw_value);
    } else if (qualified_key == "bindings.move_to_workspace_1") {
      cfg.bindings.move_to_workspace_1 = strip_quotes(raw_value);
    } else if (qualified_key == "bindings.move_to_workspace_2") {
      cfg.bindings.move_to_workspace_2 = strip_quotes(raw_value);
    } else if (qualified_key == "bindings.move_to_workspace_3") {
      cfg.bindings.move_to_workspace_3 = strip_quotes(raw_value);
    } else if (qualified_key == "bindings.move_to_workspace_4") {
      cfg.bindings.move_to_workspace_4 = strip_quotes(raw_value);
    } else if (qualified_key == "bindings.toggle_layout_direction") {
      cfg.bindings.toggle_layout_direction = strip_quotes(raw_value);
    } else if (qualified_key == "bindings.reorder_next") {
      cfg.bindings.reorder_next = strip_quotes(raw_value);
    } else if (qualified_key == "bindings.reorder_prev") {
      cfg.bindings.reorder_prev = strip_quotes(raw_value);
    } else if (qualified_key == "bindings.toggle_fullscreen") {
      cfg.bindings.toggle_fullscreen = strip_quotes(raw_value);
    }
  }

  if (cfg.workspace_count < 1) {
    cfg.workspace_count = 1;
  }
  if (cfg.schema_version < 1) {
    cfg.schema_version = 1;
  }

  return cfg;
}

}  // namespace scrollwm::config
