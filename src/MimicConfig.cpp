#include "mimic/MimicConfig.hh"

#include <algorithm>
#include <cctype>
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
    const auto parsed_bool = parse_toml_bool(value);
    if (!parsed_bool.has_value()) {
      continue;
    }

    if (current_section == "runtime" && key == "log_to_stderr") {
      options.log_to_stderr = *parsed_bool;
    }

    if (current_section == "runtime" && key == "scan_existing_windows_on_startup") {
      options.scan_existing_windows_on_startup = *parsed_bool;
    }
  }

  return options;
}

}  // namespace mimic
