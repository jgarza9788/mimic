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
