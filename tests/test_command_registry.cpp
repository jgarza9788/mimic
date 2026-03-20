#include "mimic/MimicCommandRegistry.hh"

MIMIC_TEST(command_registry_parses_exec_bindings) {
  mimic::MimicCommandRegistry registry;
  auto error = registry.parse_and_register(
      "# sample\n"
      "bind Mod4+Return exec xterm\n"
      "bind Mod4+d exec dmenu_run\n");

  MIMIC_ASSERT(!error.has_value());
  MIMIC_ASSERT(registry.exec_bindings().size() == 2);
  MIMIC_ASSERT(registry.command_for_key("Mod4+Return").value() == "xterm");
}

MIMIC_TEST(command_registry_rejects_missing_command) {
  mimic::MimicCommandRegistry registry;
  auto error = registry.parse_and_register("bind Mod4+Return exec\n");

  MIMIC_ASSERT(error.has_value());
  MIMIC_ASSERT(error->line == 1);
}
