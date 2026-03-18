#include "util/log.hpp"

#include <iostream>

namespace scrollwm::util {

namespace {
bool g_debug_enabled = true;

const char* level_prefix(LogLevel level) {
  switch (level) {
    case LogLevel::Debug:
      return "DEBUG";
    case LogLevel::Info:
      return "INFO";
    case LogLevel::Warn:
      return "WARN";
    case LogLevel::Error:
      return "ERROR";
  }
  return "UNKNOWN";
}
}  // namespace

void set_debug_enabled(bool enabled) {
  g_debug_enabled = enabled;
}

void log(LogLevel level, const std::string& message) {
  if (level == LogLevel::Debug && !g_debug_enabled) {
    return;
  }

  std::cerr << "[scrollwm:" << level_prefix(level) << "] " << message << '\n';
}

}  // namespace scrollwm::util
