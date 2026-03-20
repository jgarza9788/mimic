#include "mimic/MimicCommandRegistry.hh"

#include <sstream>

namespace mimic {

bool MimicCommandRegistry::register_exec_binding(const std::string& key_combo, const std::string& command) {
  if (key_combo.empty() || command.empty()) {
    return false;
  }

  for (auto& existing : bindings_) {
    if (existing.key_combo == key_combo) {
      existing.command = command;
      return true;
    }
  }

  bindings_.push_back({key_combo, command});
  return true;
}

std::optional<MimicCommandRegistry::ParseError> MimicCommandRegistry::parse_and_register(
    const std::string& config_text) {
  std::istringstream stream(config_text);
  std::string line;

  for (std::size_t line_num = 1; std::getline(stream, line); ++line_num) {
    if (line.empty() || line[0] == '#') {
      continue;
    }

    std::istringstream line_stream(line);
    std::string keyword;
    line_stream >> keyword;
    if (keyword != "bind") {
      return ParseError{line_num, "expected 'bind'"};
    }

    std::string key_combo;
    std::string mode;
    line_stream >> key_combo >> mode;
    if (key_combo.empty() || mode != "exec") {
      return ParseError{line_num, "expected: bind <key-combo> exec <command>"};
    }

    std::string command;
    std::getline(line_stream >> std::ws, command);
    if (command.empty() || !register_exec_binding(key_combo, command)) {
      return ParseError{line_num, "command must not be empty"};
    }
  }

  return std::nullopt;
}

std::optional<std::string> MimicCommandRegistry::command_for_key(const std::string& key_combo) const {
  for (const auto& binding : bindings_) {
    if (binding.key_combo == key_combo) {
      return binding.command;
    }
  }

  return std::nullopt;
}

}  // namespace mimic
