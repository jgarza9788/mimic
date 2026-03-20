#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

#include "mimic/MimicCommandRegistry.hh"
#include "mimic/MimicConfig.hh"
#include "mimic/MimicLayoutEngine.hh"
#include "mimic/MimicOverviewController.hh"
#include "mimic/MimicWorkspaceModel.hh"

namespace {

volatile std::sig_atomic_t g_running = 1;
bool g_bad_access = false;
std::ofstream g_log_file;

void init_log_file() {
  const char* runtime_dir = std::getenv("XDG_RUNTIME_DIR");
  const std::string path = runtime_dir != nullptr ? std::string(runtime_dir) + "/mimic.log" : "/tmp/mimic.log";
  g_log_file.open(path, std::ios::app);
}

void log_line(const std::string& line) {
  std::cerr << line << '\n';
  if (g_log_file.is_open()) {
    g_log_file << line << '\n';
    g_log_file.flush();
  }
}

void on_signal(int) { g_running = 0; }

int wm_detect_error_handler(Display*, XErrorEvent* event) {
  if (event->error_code == BadAccess) {
    g_bad_access = true;
  }
  return 0;
}

int x_error_handler(Display* display, XErrorEvent* event) {
  char error_text[1024];
  XGetErrorText(display, event->error_code, error_text, sizeof(error_text));
  std::ostringstream oss;
  oss << "Mimic X11 error: request=" << static_cast<int>(event->request_code)
      << " minor=" << static_cast<int>(event->minor_code)
      << " resource=0x" << std::hex << event->resourceid << std::dec
      << " code=" << static_cast<int>(event->error_code) << " (" << error_text << ")";
  log_line(oss.str());
  return 0;
}

unsigned int clean_modifiers(unsigned int state) {
  return state & ~(LockMask | Mod2Mask);
}

struct ParsedKeyCombo {
  unsigned int modifiers = 0;
  KeySym keysym = NoSymbol;
};

std::optional<ParsedKeyCombo> parse_key_combo(const std::string& combo) {
  ParsedKeyCombo parsed;
  std::string token;
  std::stringstream ss(combo);

  while (std::getline(ss, token, '+')) {
    if (token == "Shift") {
      parsed.modifiers |= ShiftMask;
    } else if (token == "Control" || token == "Ctrl") {
      parsed.modifiers |= ControlMask;
    } else if (token == "Alt" || token == "Mod1") {
      parsed.modifiers |= Mod1Mask;
    } else if (token == "Mod4" || token == "Super") {
      parsed.modifiers |= Mod4Mask;
    } else {
      parsed.keysym = XStringToKeysym(token.c_str());
      if (parsed.keysym == NoSymbol) {
        return std::nullopt;
      }
    }
  }

  if (parsed.keysym == NoSymbol) {
    return std::nullopt;
  }

  return parsed;
}

struct Client {
  Window window = 0;
  std::size_t workspace = 0;
};

class MimicRuntime {
 public:
  MimicRuntime(Display* display,
               Window root,
               mimic::MimicCommandRegistry* command_registry,
               mimic::MimicWorkspaceModel* workspace_model,
               mimic::MimicLayoutEngine* layout_engine,
               mimic::MimicOverviewController* overview)
      : display_(display),
        root_(root),
        command_registry_(command_registry),
        workspace_model_(workspace_model),
        layout_engine_(layout_engine),
        overview_(overview) {}

  void register_key_bindings() {
    constexpr unsigned int modifiers_to_grab[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};

    for (const auto& binding : command_registry_->exec_bindings()) {
      auto parsed = parse_key_combo(binding.key_combo);
      if (!parsed) {
        log_line("Mimic warning: could not parse key combo '" + binding.key_combo + "'");
        continue;
      }

      const KeyCode code = XKeysymToKeycode(display_, parsed->keysym);
      if (code == 0) {
        log_line("Mimic warning: no keycode for binding '" + binding.key_combo + "'");
        continue;
      }

      for (unsigned int extra : modifiers_to_grab) {
        XGrabKey(display_, code, parsed->modifiers | extra, root_, True, GrabModeAsync, GrabModeAsync);
      }

      key_commands_[{code, clean_modifiers(parsed->modifiers)}] = binding.command;
    }

    XSync(display_, False);
  }

  void scan_existing_windows() {
    Window root_return;
    Window parent_return;
    Window* children = nullptr;
    unsigned int nchildren = 0;

    if (!XQueryTree(display_, root_, &root_return, &parent_return, &children, &nchildren)) {
      return;
    }

    for (unsigned int i = 0; i < nchildren; ++i) {
      XWindowAttributes attrs;
      if (!XGetWindowAttributes(display_, children[i], &attrs)) {
        continue;
      }

      if (attrs.override_redirect || attrs.map_state != IsViewable) {
        continue;
      }

      manage_window(children[i]);
    }

    if (children != nullptr) {
      XFree(children);
    }

    apply_layout();
  }

  void run() {
    while (g_running) {
      XEvent event;
      XNextEvent(display_, &event);

      switch (event.type) {
        case MapRequest:
          on_map_request(event.xmaprequest);
          break;
        case ConfigureRequest:
          on_configure_request(event.xconfigurerequest);
          break;
        case DestroyNotify:
          unmanage_window(event.xdestroywindow.window);
          break;
        case UnmapNotify:
          if (event.xunmap.event == root_) {
            break;
          }
          unmanage_window(event.xunmap.window);
          break;
        case KeyPress:
          on_key_press(event.xkey);
          break;
        default:
          break;
      }
    }
  }

 private:
  void on_map_request(const XMapRequestEvent& event) {
    manage_window(event.window);
    XMapWindow(display_, event.window);
    apply_layout();
  }

  void on_configure_request(const XConfigureRequestEvent& event) {
    XWindowChanges wc;
    wc.x = event.x;
    wc.y = event.y;
    wc.width = event.width;
    wc.height = event.height;
    wc.border_width = event.border_width;
    wc.sibling = event.above;
    wc.stack_mode = event.detail;

    XConfigureWindow(display_, event.window, static_cast<unsigned int>(event.value_mask), &wc);

    if (is_managed(event.window)) {
      apply_layout();
    }
  }

  void on_key_press(const XKeyEvent& event) {
    const auto it = key_commands_.find({event.keycode, clean_modifiers(event.state)});
    if (it == key_commands_.end()) {
      return;
    }

    spawn_command(it->second);
  }

  bool is_managed(Window window) const {
    for (const auto& client : clients_) {
      if (client.window == window) {
        return true;
      }
    }
    return false;
  }

  void manage_window(Window window) {
    if (is_managed(window)) {
      return;
    }

    XWindowAttributes attrs;
    if (!XGetWindowAttributes(display_, window, &attrs)) {
      return;
    }

    if (attrs.override_redirect) {
      return;
    }

    XSelectInput(display_, window, EnterWindowMask | FocusChangeMask | PropertyChangeMask | StructureNotifyMask);
    clients_.push_back({window, workspace_model_->active_workspace_index()});
    workspace_model_->add_window(workspace_model_->active_workspace_index());
    layout_engine_->append_window(window);
    log_line("Mimic: managing window=" + std::to_string(window));
  }

  void unmanage_window(Window window) {
    for (std::size_t i = 0; i < clients_.size(); ++i) {
      if (clients_[i].window != window) {
        continue;
      }

      const auto workspace = clients_[i].workspace;
      clients_.erase(clients_.begin() + static_cast<long>(i));
      workspace_model_->remove_window(workspace);
      workspace_model_->maybe_remove_empty_workspace(workspace);
      layout_engine_->remove_window(window);
      log_line("Mimic: unmanaged window=" + std::to_string(window));
      apply_layout();
      return;
    }
  }

  void apply_layout() {
    std::vector<Window> ordered;
    for (const auto& client : clients_) {
      if (client.workspace == workspace_model_->active_workspace_index()) {
        ordered.push_back(client.window);
      }
    }

    layout_engine_->set_window_order(ordered);

    XWindowAttributes root_attrs;
    if (!XGetWindowAttributes(display_, root_, &root_attrs)) {
      return;
    }

    const int count = static_cast<int>(ordered.size());
    for (int i = 0; i < count; ++i) {
      const int width = root_attrs.width / std::max(1, count);
      const int x = i * width + layout_engine_->viewport().offset_x;
      const int y = layout_engine_->viewport().offset_y;
      const int final_width = i == count - 1 ? root_attrs.width - (width * i) : width;
      XMoveResizeWindow(display_, ordered[static_cast<std::size_t>(i)], x, y, final_width, root_attrs.height);
    }

    if (overview_->state() == mimic::MimicOverviewController::State::kActive) {
      overview_->enter(ordered, root_attrs.width, root_attrs.height);
    }

    XSync(display_, False);
  }

  void spawn_command(const std::string& command) {
    log_line("Mimic: exec -> " + command);
    const pid_t pid = fork();
    if (pid == 0) {
      setsid();
      execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char*>(nullptr));
      _exit(127);
    }
  }

  Display* display_;
  Window root_;
  mimic::MimicCommandRegistry* command_registry_;
  mimic::MimicWorkspaceModel* workspace_model_;
  mimic::MimicLayoutEngine* layout_engine_;
  mimic::MimicOverviewController* overview_;
  std::vector<Client> clients_;
  std::map<std::pair<KeyCode, unsigned int>, std::string> key_commands_;
};

}  // namespace

int main(int argc, char** argv) {
  init_log_file();

  mimic::MimicCommandRegistry command_registry;
  mimic::MimicWorkspaceModel workspace_model;
  mimic::MimicLayoutEngine layout_engine;
  mimic::MimicOverviewController overview;

  const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME");
  const char* home = std::getenv("HOME");

  const std::vector<std::string> candidates = mimic::config_candidates({
      argc > 1 ? std::optional<std::string>(argv[1]) : std::nullopt,
      xdg_config_home != nullptr ? std::optional<std::string>(xdg_config_home) : std::nullopt,
      home != nullptr ? std::optional<std::string>(home) : std::nullopt,
      {"/etc/xdg/mimic/mimic.keys", "/usr/local/share/mimic/examples/mimic.keys", "/usr/share/mimic/examples/mimic.keys"},
  });

  log_line("Mimic: startup begin.");

  const auto config_path = mimic::first_existing_path(candidates);
  if (config_path) {
    std::ifstream config(*config_path);
    std::string text((std::istreambuf_iterator<char>(config)), std::istreambuf_iterator<char>());
    auto parse_error = command_registry.parse_and_register(text);
    if (parse_error) {
      log_line("Mimic config parse error at line " + std::to_string(parse_error->line) + ": " + parse_error->reason);
    }
    log_line("Mimic: loaded config from " + *config_path + " with " +
             std::to_string(command_registry.exec_bindings().size()) + " exec bindings.");
  } else {
    log_line("Mimic: no config file found; continuing with defaults and 0 bindings.");
  }

  log_line("Mimic: attempting to open X display.");
  Display* display = XOpenDisplay(nullptr);
  if (display == nullptr) {
    log_line("Mimic: unable to connect to X server. Running in dry mode.");
    log_line("Mimic dry-mode status: exec_bindings=" + std::to_string(command_registry.exec_bindings().size()) +
             " workspaces=" + std::to_string(workspace_model.workspace_count()));
    return 0;
  }

  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  const int screen = DefaultScreen(display);
  const Window root = RootWindow(display, screen);

  log_line("Mimic: attempting to claim root SubstructureRedirectMask.");
  XSetErrorHandler(wm_detect_error_handler);
  XSelectInput(display, root, SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask);
  XSync(display, False);

  if (g_bad_access) {
    log_line("Mimic: root SubstructureRedirectMask claim failed with BadAccess.");
    log_line("Mimic: failed to become WM on root window; another window manager is already running.");
    XCloseDisplay(display);
    return 1;
  }

  XSetErrorHandler(x_error_handler);
  log_line("Mimic: root SubstructureRedirectMask claim succeeded.");
  log_line("Mimic: connected to X display and claimed root window successfully.");

  MimicRuntime runtime(display, root, &command_registry, &workspace_model, &layout_engine, &overview);
  runtime.register_key_bindings();
  runtime.scan_existing_windows();

  log_line("Mimic: entering event loop.");
  runtime.run();

  log_line("Mimic: shutting down.");
  XCloseDisplay(display);
  return 0;
}
