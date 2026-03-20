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

struct RuntimeOptions {
  bool log_to_stderr = true;
  bool scan_existing_windows_on_startup = true;
};

std::vector<std::string> config_candidates(const ConfigSearchInput& input);
std::optional<std::string> first_existing_path(const std::vector<std::string>& paths);
std::vector<std::string> toml_config_candidates(std::optional<std::string> xdg_config_home,
                                                std::optional<std::string> home,
                                                std::vector<std::string> fallback_paths = {});
RuntimeOptions load_runtime_options(const std::vector<std::string>& candidate_paths,
                                    std::optional<std::string>* loaded_path = nullptr);

}  // namespace mimic
