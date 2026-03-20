#pragma once

#include <optional>
#include <string>
#include <vector>

namespace mimic {

struct ExecBinding {
  std::string key_combo;
  std::string command;
};

class MimicCommandRegistry {
 public:
  struct ParseError {
    std::size_t line = 0;
    std::string reason;
  };

  std::optional<ParseError> parse_and_register(const std::string& config_text);
  bool register_exec_binding(const std::string& key_combo, const std::string& command);

  const std::vector<ExecBinding>& exec_bindings() const { return bindings_; }
  std::optional<std::string> command_for_key(const std::string& key_combo) const;

 private:
  std::vector<ExecBinding> bindings_;
};

}  // namespace mimic
