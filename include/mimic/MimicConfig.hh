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

std::vector<std::string> config_candidates(const ConfigSearchInput& input);
std::optional<std::string> first_existing_path(const std::vector<std::string>& paths);

}  // namespace mimic
