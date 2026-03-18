#pragma once

#include <string>

namespace scrollwm::util {

enum class LogLevel {
  Debug,
  Info,
  Warn,
  Error,
};

void set_debug_enabled(bool enabled);
void log(LogLevel level, const std::string& message);

}  // namespace scrollwm::util
