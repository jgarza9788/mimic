#include "config/config.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <unistd.h>

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

void flush_exec_binding_if_complete(const std::optional<Config::ExecBinding>& pending,
                                    Config& cfg) {
  if (!pending.has_value()) {
    return;
  }
  if (trim(pending->key).empty() || trim(pending->command).empty()) {
    return;
  }
  cfg.exec_bindings.push_back(*pending);
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

std::optional<int> parse_positive_int(const std::string& value) {
  int parsed = 0;
  const auto* begin = value.data();
  const auto* end = value.data() + value.size();
  const auto [ptr, ec] = std::from_chars(begin, end, parsed);
  if (ec != std::errc() || ptr != end || parsed < 1) {
    return std::nullopt;
  }
  return parsed;
}

std::optional<int> parse_int(const std::string& value) {
  int parsed = 0;
  const auto* begin = value.data();
  const auto* end = value.data() + value.size();
  const auto [ptr, ec] = std::from_chars(begin, end, parsed);
  if (ec != std::errc() || ptr != end) {
    return std::nullopt;
  }
  return parsed;
}

std::string first_command_token(const std::string& command) {
  const auto trimmed = trim(command);
  if (trimmed.empty()) {
    return {};
  }
  const auto first_space = trimmed.find_first_of(" \t");
  if (first_space == std::string::npos) {
    return trimmed;
  }
  return trimmed.substr(0, first_space);
}

}  // namespace

std::string Config::BindingSet::workspace_binding(int one_based_index) const {
  if (const auto it = workspace.find(one_based_index); it != workspace.end()) {
    return it->second;
  }

  switch (one_based_index) {
    case 1:
      return "Mod+1";
    case 2:
      return "Mod+2";
    case 3:
      return "Mod+3";
    case 4:
      return "Mod+4";
    default:
      return {};
  }
}

std::string Config::BindingSet::move_to_workspace_binding(int one_based_index) const {
  if (const auto it = move_to_workspace.find(one_based_index); it != move_to_workspace.end()) {
    return it->second;
  }

  switch (one_based_index) {
    case 1:
      return "Mod+Shift+1";
    case 2:
      return "Mod+Shift+2";
    case 3:
      return "Mod+Shift+3";
    case 4:
      return "Mod+Shift+4";
    default:
      return {};
  }
}

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

std::vector<std::string> terminal_fallback_candidates() {
  return {
      "xterm",
      "kitty",
      "alacritty",
      "foot",
      "x-terminal-emulator",
  };
}

bool command_exists_in_path(const std::string& command) {
  const std::string token = first_command_token(command);
  if (token.empty()) {
    return false;
  }

  std::filesystem::path cmd_path(token);
  if (cmd_path.is_absolute() || token.find('/') != std::string::npos) {
    return ::access(cmd_path.c_str(), X_OK) == 0;
  }

  const char* path_env = std::getenv("PATH");
  if (path_env == nullptr) {
    return false;
  }

  std::stringstream path_stream(path_env);
  std::string dir;
  while (std::getline(path_stream, dir, ':')) {
    std::filesystem::path candidate = std::filesystem::path(dir) / token;
    if (::access(candidate.c_str(), X_OK) == 0) {
      return true;
    }
  }

  return false;
}

std::string resolve_terminal_command(const std::string& configured_terminal) {
  if (command_exists_in_path(configured_terminal)) {
    return configured_terminal;
  }
  const auto fallbacks = terminal_fallback_candidates();
  const auto fallback_it = std::find_if(fallbacks.begin(), fallbacks.end(), [](const auto& fallback) {
    return command_exists_in_path(fallback);
  });
  if (fallback_it != fallbacks.end()) {
    return *fallback_it;
  }
  return {};
}

Config load_from_path(const std::filesystem::path& path) {
  Config cfg = load_default();

  std::ifstream file(path);
  if (!file.good()) {
    return cfg;
  }

  std::string current_section;
  std::optional<Config::ExecBinding> pending_exec;
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

    if (line.rfind("[[", 0) == 0 && line.size() >= 4 && line.substr(line.size() - 2) == "]]") {
      flush_exec_binding_if_complete(pending_exec, cfg);
      pending_exec.reset();
      current_section = trim(line.substr(2, line.size() - 4));
      if (current_section == "exec") {
        pending_exec = Config::ExecBinding{};
      }
      continue;
    }

    if (line.front() == '[' && line.back() == ']') {
      flush_exec_binding_if_complete(pending_exec, cfg);
      pending_exec.reset();
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
      if (auto parsed = parse_int(raw_value); parsed.has_value()) {
        cfg.schema_version = *parsed;
      }
    } else if (qualified_key == "general.workspace_count") {
      if (auto parsed = parse_int(raw_value); parsed.has_value()) {
        cfg.workspace_count = *parsed;
      }
    } else if (qualified_key == "general.focus_follows_mouse") {
      cfg.focus_follows_mouse = to_bool(raw_value);
    } else if (qualified_key == "layout.layout_direction") {
      cfg.layout_direction = to_direction(strip_quotes(raw_value));
    } else if (qualified_key == "appearance.gap") {
      if (auto parsed = parse_int(raw_value); parsed.has_value()) {
        cfg.gap = *parsed;
      }
    } else if (qualified_key == "appearance.border_width") {
      if (auto parsed = parse_int(raw_value); parsed.has_value()) {
        cfg.border_width = *parsed;
      }
    } else if (qualified_key == "appearance.outer_padding") {
      if (auto parsed = parse_int(raw_value); parsed.has_value()) {
        cfg.outer_padding = *parsed;
      }
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
    } else if (qualified_key == "bindings.toggle_layout_direction") {
      cfg.bindings.toggle_layout_direction = strip_quotes(raw_value);
    } else if (qualified_key == "bindings.reorder_next") {
      cfg.bindings.reorder_next = strip_quotes(raw_value);
    } else if (qualified_key == "bindings.reorder_prev") {
      cfg.bindings.reorder_prev = strip_quotes(raw_value);
    } else if (qualified_key == "bindings.toggle_fullscreen") {
      cfg.bindings.toggle_fullscreen = strip_quotes(raw_value);
    } else if (qualified_key == "bindings.toggle_overview") {
      cfg.bindings.toggle_overview = strip_quotes(raw_value);
    } else if (qualified_key == "bindings.activate_overview") {
      cfg.bindings.activate_overview = strip_quotes(raw_value);
    } else if (qualified_key == "exec.key" && pending_exec.has_value()) {
      pending_exec->key = strip_quotes(raw_value);
    } else if (qualified_key == "exec.command" && pending_exec.has_value()) {
      pending_exec->command = strip_quotes(raw_value);
    } else if (qualified_key.rfind("bindings.workspace_", 0) == 0) {
      const auto maybe_index = parse_positive_int(qualified_key.substr(std::string("bindings.workspace_").size()));
      if (maybe_index.has_value()) {
        cfg.bindings.workspace[*maybe_index] = strip_quotes(raw_value);
      }
    } else if (qualified_key.rfind("bindings.move_to_workspace_", 0) == 0) {
      const auto maybe_index = parse_positive_int(
          qualified_key.substr(std::string("bindings.move_to_workspace_").size()));
      if (maybe_index.has_value()) {
        cfg.bindings.move_to_workspace[*maybe_index] = strip_quotes(raw_value);
      }
    }
  }

  flush_exec_binding_if_complete(pending_exec, cfg);

  if (cfg.workspace_count < 1) {
    cfg.workspace_count = 1;
  }
  if (cfg.schema_version < 1) {
    cfg.schema_version = 1;
  }

  return cfg;
}

}  // namespace scrollwm::config
