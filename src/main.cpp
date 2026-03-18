#include <iostream>
#include <string>

#include "wm.hpp"

int main(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      std::cout << "scrollwm - scrollable tiling WM for X11\n"
                   "Usage: scrollwm [--help] [--version]\n";
      return 0;
    }
    if (arg == "--version" || arg == "-v") {
      std::cout << "scrollwm 0.1.0\n";
      return 0;
    }
  }

  scrollwm::WM wm;
  return wm.run();
}
