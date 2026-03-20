#include <iostream>
#include <string>
#include <cstdlib>
#include <exception>

#include "util/log.hpp"
#include "wm.hpp"

int main(int argc, char** argv) {
  bool enable_verbose = false;
  bool enable_sync = false;
  bool startup_check_only = false;
  std::string log_file_path;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      std::cout << "scrollwm - scrollable tiling WM for X11\n"
                   "Usage: scrollwm [--help] [--version] [--verbose] [--sync]"
                   " [--check] [--log-file PATH]\n";
      return 0;
    }
    if (arg == "--version" || arg == "-v") {
      std::cout << "scrollwm 0.1.0\n";
      return 0;
    }
    if (arg == "--verbose") {
      enable_verbose = true;
      continue;
    }
    if (arg == "--sync") {
      enable_sync = true;
      continue;
    }
    if (arg == "--check") {
      startup_check_only = true;
      continue;
    }
    if (arg == "--log-file") {
      if (i + 1 >= argc) {
        std::cerr << "scrollwm: --log-file requires a path\n";
        return 2;
      }
      log_file_path = argv[++i];
      continue;
    }
    std::cerr << "scrollwm: unknown option: " << arg << '\n';
    return 2;
  }

  scrollwm::util::set_debug_enabled(enable_verbose);
  if (!log_file_path.empty() && !scrollwm::util::set_log_file(log_file_path)) {
    std::cerr << "scrollwm: failed to open log file: " << log_file_path << '\n';
    return 2;
  }

  if (enable_sync) {
    setenv("LIBXCB_SYNCHRONIZE", "1", 1);
    scrollwm::util::log(scrollwm::util::LogLevel::Info,
                        "enabled LIBXCB_SYNCHRONIZE=1 for synchronous X11 diagnostics");
  }

  if (startup_check_only) {
    return scrollwm::WM::check_startup_environment() ? 0 : 1;
  }

  try {
    scrollwm::WM wm;
    return wm.run();
  } catch (const std::exception& ex) {
    scrollwm::util::log(scrollwm::util::LogLevel::Error,
                        std::string("fatal exception during startup/runtime: ") + ex.what());
    return 1;
  } catch (...) {
    scrollwm::util::log(scrollwm::util::LogLevel::Error,
                        "fatal unknown exception during startup/runtime");
    return 1;
  }
}
