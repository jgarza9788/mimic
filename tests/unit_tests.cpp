#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "config/config.hpp"
#include "layout/scroll_layout.hpp"
#include "model/workspace.hpp"

namespace {

bool test_workspace_focus_cycle() {
  scrollwm::model::Workspace ws(0);
  ws.add_client({.window = 100});
  ws.add_client({.window = 101});
  ws.add_client({.window = 102});

  if (!ws.focused_index().has_value() || *ws.focused_index() != 2) {
    std::cerr << "expected last added client to be focused\n";
    return false;
  }

  ws.focus_next();
  if (!ws.focused_index().has_value() || *ws.focused_index() != 0) {
    std::cerr << "focus_next should wrap to 0\n";
    return false;
  }

  ws.focus_prev();
  if (!ws.focused_index().has_value() || *ws.focused_index() != 2) {
    std::cerr << "focus_prev should wrap to end\n";
    return false;
  }

  ws.remove_client(102);
  if (!ws.focused_index().has_value() || *ws.focused_index() != 1) {
    std::cerr << "focus should track to new end after remove\n";
    return false;
  }

  ws.reorder_focused_backward();
  if (!ws.focused_index().has_value() || *ws.focused_index() != 0) {
    std::cerr << "reorder backward should move focus to previous index\n";
    return false;
  }

  ws.clients()[1].urgent = true;
  ws.focus_urgent();
  if (!ws.focused_index().has_value() || *ws.focused_index() != 1) {
    std::cerr << "focus_urgent should jump to urgent client\n";
    return false;
  }

  return true;
}

bool test_scroll_layout_focus_centering() {
  scrollwm::model::Workspace ws(0);
  ws.add_client({.window = 10});
  ws.add_client({.window = 11});
  ws.add_client({.window = 12});
  ws.focus_index(2);

  scrollwm::layout::ScrollLayout layout(scrollwm::config::Direction::Horizontal);
  int offset = 0;
  const auto rects = layout.compute(ws, 1920, 1080, 12, 2, 12, offset);

  if (rects.size() != 3) {
    std::cerr << "expected 3 rects\n";
    return false;
  }
  if (offset <= 0) {
    std::cerr << "expected positive scroll offset for focused tail window\n";
    return false;
  }

  const auto& focused = rects[2];
  const int focused_center = focused.x + (focused.width / 2);
  const int viewport_center = 1920 / 2;
  if (std::abs(focused_center - viewport_center) > 300) {
    std::cerr << "focused window should be kept near viewport center\n";
    return false;
  }

  layout.set_direction(scrollwm::config::Direction::Vertical);
  offset = 0;
  const auto vertical_rects = layout.compute(ws, 1920, 1080, 12, 2, 12, offset);
  if (vertical_rects.size() != 3 || vertical_rects[0].x == rects[0].x) {
    std::cerr << "direction toggle should affect computed geometry\n";
    return false;
  }

  return true;
}

bool test_config_parse() {
  const auto tmp = std::filesystem::temp_directory_path() / "scrollwm-config-test.toml";
  {
    std::ofstream out(tmp);
    out << "[schema]\n";
    out << "version = 1\n";
    out << "[general]\n";
    out << "mod_key = \"Mod1\"\n";
    out << "workspace_count = 6\n";
    out << "focus_follows_mouse = true\n";
    out << "terminal = \"alacritty\"\n";
    out << "[layout]\n";
    out << "layout_direction = \"vertical\"\n";
    out << "[appearance]\n";
    out << "gap = 20\n";
    out << "border_width = 3\n";
    out << "outer_padding = 8\n";
    out << "[bindings]\n";
    out << "workspace_1 = \"Mod+F1\"\n";
    out << "move_to_workspace_1 = \"Mod+Shift+F1\"\n";
    out << "[autostart]\n";
    out << "launch_picom = true\n";
    out << "compositor = \"picom --backend glx\"\n";
  }

  const auto cfg = scrollwm::config::load_from_path(tmp);
  std::filesystem::remove(tmp);

  if (cfg.schema_version != 1 || cfg.mod_key != "Mod1" || cfg.workspace_count != 6 || !cfg.focus_follows_mouse ||
      cfg.terminal != "alacritty") {
    std::cerr << "general config parse mismatch\n";
    return false;
  }

  if (cfg.layout_direction != scrollwm::config::Direction::Vertical) {
    std::cerr << "layout direction parse mismatch\n";
    return false;
  }

  if (cfg.gap != 20 || cfg.border_width != 3 || cfg.outer_padding != 8) {
    std::cerr << "appearance parse mismatch\n";
    return false;
  }

  if (!cfg.autostart_picom || cfg.compositor_cmd != "picom --backend glx") {
    std::cerr << "autostart parse mismatch\n";
    return false;
  }

  if (cfg.bindings.workspace_1 != "Mod+F1" || cfg.bindings.move_to_workspace_1 != "Mod+Shift+F1") {
    std::cerr << "bindings parse mismatch\n";
    return false;
  }

  if (cfg.bindings.toggle_layout_direction != "Mod+Space" ||
      cfg.bindings.reorder_next != "Mod+Shift+J" ||
      cfg.bindings.reorder_prev != "Mod+Shift+K" ||
      cfg.bindings.toggle_fullscreen != "Mod+F") {
    std::cerr << "new bindings should preserve defaults when unspecified\n";
    return false;
  }

  return true;
}

}  // namespace

int main() {
  const bool ok = test_workspace_focus_cycle() && test_scroll_layout_focus_centering() && test_config_parse();
  if (!ok) {
    return 1;
  }

  std::cout << "All unit tests passed\n";
  return 0;
}
