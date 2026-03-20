#include "mimic/MimicConfig.hh"

#include <filesystem>

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

}  // namespace mimic
