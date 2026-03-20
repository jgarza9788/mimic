#include "util/log.hpp"

#include <fstream>
#include <iostream>
#include <memory>

namespace scrollwm::util {

namespace {
bool g_debug_enabled = true;
std::unique_ptr<std::ofstream> g_log_file;

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

bool set_log_file(const std::filesystem::path& path) {
  auto out = std::make_unique<std::ofstream>(path, std::ios::app);
  if (!out->is_open()) {
    return false;
  }
  g_log_file = std::move(out);
  return true;
}

void log(LogLevel level, const std::string& message) {
  if (level == LogLevel::Debug && !g_debug_enabled) {
    return;
  }

  const std::string formatted = "[scrollwm:" + std::string(level_prefix(level)) + "] " + message + '\n';
  std::cerr << formatted;
  if (g_log_file != nullptr) {
    *g_log_file << formatted;
    g_log_file->flush();
  }
}

}  // namespace scrollwm::util
