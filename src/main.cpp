#include <X11/Xlib.h>

#include <fstream>
#include <iostream>
#include <string>

#include "mimic/MimicCommandRegistry.hh"
#include "mimic/MimicLayoutEngine.hh"
#include "mimic/MimicOverviewController.hh"
#include "mimic/MimicWorkspaceModel.hh"

int main(int argc, char** argv) {
  mimic::MimicCommandRegistry command_registry;
  mimic::MimicWorkspaceModel workspace_model;
  mimic::MimicLayoutEngine layout_engine;
  mimic::MimicOverviewController overview;

  std::string config_path = argc > 1 ? argv[1] : "config/mimic.keys";

  std::ifstream config(config_path);
  if (config.good()) {
    std::string text((std::istreambuf_iterator<char>(config)), std::istreambuf_iterator<char>());
    auto parse_error = command_registry.parse_and_register(text);
    if (parse_error) {
      std::cerr << "Mimic config parse error at line " << parse_error->line << ": "
                << parse_error->reason << '\n';
    }
  }

  Display* display = XOpenDisplay(nullptr);
  if (display == nullptr) {
    std::cerr << "Mimic: unable to connect to X server. Running in dry mode.\n";
    std::cout << "Registered exec bindings: " << command_registry.exec_bindings().size() << '\n';
    std::cout << "Workspace count: " << workspace_model.workspace_count() << '\n';
    return 0;
  }

  XCloseDisplay(display);

  // TODO(mimic): Wire MimicLayoutEngine and MimicOverviewController into full X11 event loop.
  std::cout << "Mimic initialized with " << command_registry.exec_bindings().size()
            << " exec bindings.\n";
  (void)layout_engine;
  (void)overview;
  return 0;
}
