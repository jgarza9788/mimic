#pragma once

#include <filesystem>
#include <string>

namespace scrollwm::util {

enum class LogLevel {
  Debug,
  Info,
  Warn,
  Error,
};

void set_debug_enabled(bool enabled);
bool set_log_file(const std::filesystem::path& path);
void log(LogLevel level, const std::string& message);

}  // namespace scrollwm::util
