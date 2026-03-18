#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "config/config.hpp"
#include "layout/scroll_layout.hpp"
#include "model/workspace.hpp"

namespace {

using scrollwm::config::Config;
using scrollwm::config::Direction;
using scrollwm::layout::Rect;
using scrollwm::layout::ScrollLayout;
using scrollwm::model::Workspace;

Workspace make_workspace_with_clients(int idx, std::initializer_list<xcb_window_t> windows) {
  Workspace ws(idx);
  for (xcb_window_t window : windows) {
    ws.add_client({.window = window});
  }
  return ws;
}

bool expect(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    return false;
  }
  return true;
}

bool with_temp_config(const std::string& name,
                      const std::string& contents,
                      const std::function<bool(const Config&)>& check) {
  const auto path = std::filesystem::temp_directory_path() / name;
  {
    std::ofstream out(path);
    out << contents;
  }

  const auto cfg = scrollwm::config::load_from_path(path);
  std::filesystem::remove(path);
  return check(cfg);
}

bool test_config_defaults_when_sections_omitted() {
  return with_temp_config(
      "scrollwm-config-defaults.toml",
      "# intentionally sparse\n[general]\nmod_key=\"Mod1\"\n",
      [](const Config& cfg) {
        return expect(cfg.mod_key == "Mod1", "mod_key should parse") &&
               expect(cfg.workspace_count == 4, "workspace_count should keep default") &&
               expect(cfg.layout_direction == Direction::Horizontal, "layout direction should keep default") &&
               expect(cfg.bindings.focus_next == "Mod+J", "focus_next default should remain") &&
               expect(cfg.exec_bindings.empty(), "exec bindings should default to empty");
      });
}

bool test_config_builtins_and_dynamic_workspace_bindings() {
  return with_temp_config(
      "scrollwm-config-builtins.toml",
      "[bindings]\n"
      "focus_next = \"Mod+N\"\n"
      "focus_prev = \"Mod+P\"\n"
      "spawn_terminal = \"Mod+T\"\n"
      "workspace_1 = \"Mod+F1\"\n"
      "workspace_9 = \"Mod+F9\"\n"
      "move_to_workspace_1 = \"Mod+Shift+F1\"\n"
      "move_to_workspace_9 = \"Mod+Shift+F9\"\n",
      [](const Config& cfg) {
        return expect(cfg.bindings.focus_next == "Mod+N", "focus_next should parse") &&
               expect(cfg.bindings.focus_prev == "Mod+P", "focus_prev should parse") &&
               expect(cfg.bindings.spawn_terminal == "Mod+T", "spawn_terminal should parse") &&
               expect(cfg.bindings.workspace_binding(1) == "Mod+F1", "workspace_1 should parse") &&
               expect(cfg.bindings.workspace_binding(9) == "Mod+F9", "workspace_9 should parse") &&
               expect(cfg.bindings.move_to_workspace_binding(1) == "Mod+Shift+F1",
                      "move_to_workspace_1 should parse") &&
               expect(cfg.bindings.move_to_workspace_binding(9) == "Mod+Shift+F9",
                      "move_to_workspace_9 should parse");
      });
}

bool test_config_exec_entries_parse_in_order() {
  return with_temp_config(
      "scrollwm-config-exec-order.toml",
      "[[exec]]\n"
      "key = \"Mod+D\"\n"
      "command = \"rofi -show drun\"\n"
      "[[exec]]\n"
      "key=\"Mod+Shift+L\"\n"
      "command='~/.config/scrollwm/lock.sh'\n",
      [](const Config& cfg) {
        return expect(cfg.exec_bindings.size() == 2, "two exec bindings should parse") &&
               expect(cfg.exec_bindings[0].key == "Mod+D", "first exec key mismatch") &&
               expect(cfg.exec_bindings[0].command == "rofi -show drun", "first exec command mismatch") &&
               expect(cfg.exec_bindings[1].key == "Mod+Shift+L", "second exec key mismatch") &&
               expect(cfg.exec_bindings[1].command == "~/.config/scrollwm/lock.sh",
                      "second exec command mismatch");
      });
}

bool test_config_exec_malformed_entries_ignored() {
  return with_temp_config(
      "scrollwm-config-exec-malformed.toml",
      "[[exec]]\n"
      "key = \"Mod+1\"\n"
      "command = \"notify-send ok\"\n"
      "[[exec]]\n"
      "key = \"Mod+2\"\n"
      "# missing command\n"
      "[[exec]]\n"
      "command = \"missing-key\"\n"
      "[[exec]]\n"
      "key = \"\"\n"
      "command = \"notify-send bad\"\n"
      "[[exec]]\n"
      "key = \"Mod+3\"\n"
      "command = \"   \"\n"
      "[[exec]]\n"
      "key = \"Mod+4\"\n"
      "command = \"notify-send final\"\n",
      [](const Config& cfg) {
        return expect(cfg.exec_bindings.size() == 2, "only complete non-empty exec entries should remain") &&
               expect(cfg.exec_bindings[0].key == "Mod+1", "first valid exec should be preserved") &&
               expect(cfg.exec_bindings[1].key == "Mod+4", "last valid exec should be preserved");
      });
}

bool test_config_exec_whitespace_comments_unknown_sections() {
  return with_temp_config(
      "scrollwm-config-exec-whitespace.toml",
      "[unknown]\n"
      "ignored = \"value\"\n"
      "[[exec]]   # launcher\n"
      " key = \" Mod+Space \"   # inline comment\n"
      " command = \"  rofi -show run  \"\n"
      "[[exec]]\n"
      "key = \"Mod+X\"\n"
      "command = \"xkill\"\n"
      "[layout]\n"
      "layout_direction = \"vertical\"\n"
      "[bindings]\n"
      "workspace_abc = \"Mod+9\"\n",
      [](const Config& cfg) {
        return expect(cfg.layout_direction == Direction::Vertical, "known sections should still parse") &&
               expect(cfg.exec_bindings.size() == 2, "exec entries with comments should parse") &&
               expect(cfg.exec_bindings[0].key == " Mod+Space ", "value quoting should be preserved like other keys") &&
               expect(cfg.exec_bindings[0].command == "  rofi -show run  ",
                      "exec command should follow existing quote-strip behavior") &&
               expect(cfg.bindings.workspace.find(1) == cfg.bindings.workspace.end(),
                      "invalid dynamic binding key should be ignored");
      });
}

bool test_config_clamps_workspace_and_schema_versions() {
  return with_temp_config(
      "scrollwm-config-clamp.toml",
      "[schema]\n"
      "version = 0\n"
      "[general]\n"
      "workspace_count = -10\n",
      [](const Config& cfg) {
        return expect(cfg.workspace_count == 1, "workspace_count should clamp to 1") &&
               expect(cfg.schema_version == 1, "schema version should clamp to 1");
      });
}

bool test_workspace_remove_middle_focused_behavior() {
  Workspace ws(0);
  ws.add_client({.window = 100});
  ws.add_client({.window = 101});
  ws.add_client({.window = 102});
  ws.focus_index(1);
  ws.remove_client(101);

  return expect(ws.clients().size() == 2, "removing a middle client should shrink collection") &&
         expect(ws.focused_index().has_value() && *ws.focused_index() == 1,
                "focus should move to the next logical client") &&
         expect(ws.clients()[1].window == 102, "remaining ordering should stay stable");
}

bool test_workspace_remove_only_client_clears_focus() {
  Workspace ws(0);
  ws.add_client({.window = 500});
  ws.remove_client(500);

  return expect(ws.clients().empty(), "workspace should become empty") &&
         expect(!ws.focused_index().has_value(), "focus should clear when last client removed") &&
         expect(ws.focused_client() == nullptr, "focused_client should return nullptr when empty");
}

bool test_workspace_reorder_boundaries_and_wrap_behavior() {
  Workspace ws(0);
  ws.add_client({.window = 1});
  ws.add_client({.window = 2});
  ws.add_client({.window = 3});

  ws.focus_index(2);
  ws.reorder_focused_forward();
  const bool forward_boundary_ok = expect(*ws.focused_index() == 2,
                                          "reorder forward at end should be a no-op");

  ws.focus_index(0);
  ws.reorder_focused_backward();
  const bool backward_boundary_ok = expect(*ws.focused_index() == 0,
                                           "reorder backward at start should be a no-op");

  ws.focus_index(1);
  ws.reorder_focused_forward();
  const bool forward_swap_ok = expect(ws.clients()[1].window == 3 && ws.clients()[2].window == 2,
                                      "reorder forward should swap with next") &&
                               expect(*ws.focused_index() == 2,
                                      "focus should follow reordered client forward");

  ws.reorder_focused_backward();
  const bool backward_swap_ok = expect(ws.clients()[1].window == 2 && ws.clients()[2].window == 3,
                                       "reorder backward should swap with previous") &&
                                expect(*ws.focused_index() == 1,
                                       "focus should follow reordered client backward");

  return forward_boundary_ok && backward_boundary_ok && forward_swap_ok && backward_swap_ok;
}

bool test_workspace_focus_urgent_when_none_exists() {
  Workspace ws(0);
  ws.add_client({.window = 10});
  ws.add_client({.window = 11});
  ws.focus_index(1);
  ws.focus_urgent();

  return expect(ws.focused_index().has_value() && *ws.focused_index() == 1,
                "focus_urgent should leave focus unchanged when no urgent clients");
}

bool test_workspace_focus_index_out_of_range_noop() {
  Workspace ws(0);
  ws.add_client({.window = 70});
  ws.focus_index(0);
  ws.focus_index(42);

  return expect(ws.focused_index().has_value() && *ws.focused_index() == 0,
                "focus_index out of range should not change focus");
}

bool test_dynamic_workspace_cleanup_removes_empty_non_last_workspace() {
  std::vector<Workspace> workspaces;
  workspaces.push_back(make_workspace_with_clients(0, {1}));
  workspaces.push_back(make_workspace_with_clients(1, {}));
  workspaces.push_back(make_workspace_with_clients(2, {2}));
  std::vector<int> tracked = {2};

  scrollwm::model::cleanup_empty_workspaces(workspaces, tracked);

  return expect(workspaces.size() == 2, "empty middle workspace should be removed") &&
         expect(workspaces[0].index() == 0 && workspaces[1].index() == 1,
                "workspace indices should be compacted") &&
         expect(tracked[0] == 1, "tracked index should shift when earlier workspace removed");
}

bool test_dynamic_workspace_cleanup_keeps_last_workspace_alive() {
  std::vector<Workspace> workspaces;
  workspaces.push_back(make_workspace_with_clients(0, {}));
  std::vector<int> tracked = {0};

  scrollwm::model::cleanup_empty_workspaces(workspaces, tracked);

  return expect(workspaces.size() == 1, "cleanup should keep one workspace") &&
         expect(workspaces[0].index() == 0, "last workspace index should remain zero") &&
         expect(tracked[0] == 0, "tracked index should remain valid");
}

bool test_dynamic_workspace_cleanup_focused_workspace_falls_forward() {
  std::vector<Workspace> workspaces;
  workspaces.push_back(make_workspace_with_clients(0, {10}));
  workspaces.push_back(make_workspace_with_clients(1, {}));
  workspaces.push_back(make_workspace_with_clients(2, {20}));
  std::vector<int> tracked = {1};

  scrollwm::model::cleanup_empty_workspaces(workspaces, tracked);

  return expect(workspaces.size() == 2, "focused empty workspace should be removed") &&
         expect(tracked[0] == 1, "focus should fall forward to nearest surviving workspace");
}

bool test_dynamic_workspace_cleanup_focused_workspace_falls_backward_at_end() {
  std::vector<Workspace> workspaces;
  workspaces.push_back(make_workspace_with_clients(0, {10}));
  workspaces.push_back(make_workspace_with_clients(1, {}));
  std::vector<int> tracked = {1};

  scrollwm::model::cleanup_empty_workspaces(workspaces, tracked);

  return expect(workspaces.size() == 1, "tail empty workspace should be removed") &&
         expect(tracked[0] == 0, "focus should fall backward when no forward workspace exists");
}

bool test_dynamic_workspace_cleanup_repeated_deletions_stable() {
  std::vector<Workspace> workspaces;
  workspaces.push_back(make_workspace_with_clients(0, {}));
  workspaces.push_back(make_workspace_with_clients(1, {11}));
  workspaces.push_back(make_workspace_with_clients(2, {}));
  workspaces.push_back(make_workspace_with_clients(3, {33}));
  workspaces.push_back(make_workspace_with_clients(4, {}));
  std::vector<int> tracked = {3};

  scrollwm::model::cleanup_empty_workspaces(workspaces, tracked);
  const bool first_pass = expect(workspaces.size() == 2, "all non-last empty workspaces should be removed") &&
                          expect(tracked[0] == 1, "tracked index should be updated after repeated removals");

  workspaces[0].remove_client(11);
  scrollwm::model::cleanup_empty_workspaces(workspaces, tracked);
  const bool second_pass = expect(workspaces.size() == 1, "cleanup should continue to remove newly empty workspaces") &&
                           expect(tracked[0] == 0, "tracked index should remain valid after second cleanup");

  return first_pass && second_pass;
}

bool test_dynamic_workspace_cleanup_delete_first_and_last_and_middle() {
  std::vector<Workspace> first_case;
  first_case.push_back(make_workspace_with_clients(0, {}));
  first_case.push_back(make_workspace_with_clients(1, {1}));
  std::vector<int> first_tracked = {1};
  scrollwm::model::cleanup_empty_workspaces(first_case, first_tracked);
  const bool first_ok = expect(first_case.size() == 1, "first workspace deletion should work") &&
                        expect(first_tracked[0] == 0, "focus should shift after first deletion");

  std::vector<Workspace> last_case;
  last_case.push_back(make_workspace_with_clients(0, {1}));
  last_case.push_back(make_workspace_with_clients(1, {}));
  std::vector<int> last_tracked = {0};
  scrollwm::model::cleanup_empty_workspaces(last_case, last_tracked);
  const bool last_ok = expect(last_case.size() == 1, "last workspace deletion should work") &&
                       expect(last_tracked[0] == 0, "focus should remain on first workspace");

  std::vector<Workspace> middle_case;
  middle_case.push_back(make_workspace_with_clients(0, {1}));
  middle_case.push_back(make_workspace_with_clients(1, {}));
  middle_case.push_back(make_workspace_with_clients(2, {2}));
  std::vector<int> middle_tracked = {0};
  scrollwm::model::cleanup_empty_workspaces(middle_case, middle_tracked);
  const bool middle_ok = expect(middle_case.size() == 2, "middle workspace deletion should work") &&
                         expect(middle_tracked[0] == 0, "focus before removed workspace should remain stable");

  return first_ok && last_ok && middle_ok;
}

bool test_dynamic_workspace_ensure_exists_expands_contiguously() {
  std::vector<Workspace> workspaces;
  workspaces.push_back(make_workspace_with_clients(0, {1}));

  scrollwm::model::ensure_workspace_exists(workspaces, 3);

  return expect(workspaces.size() == 4, "ensure should create all missing intermediate workspaces") &&
         expect(workspaces[3].index() == 3, "new workspace index should match requested slot") &&
         expect(workspaces[1].clients().empty() && workspaces[2].clients().empty(),
                "new intermediate workspaces should start empty");
}

bool test_dynamic_workspace_move_client_cleanup_simulation() {
  std::vector<Workspace> workspaces;
  workspaces.push_back(make_workspace_with_clients(0, {1}));
  workspaces.push_back(make_workspace_with_clients(1, {2}));
  std::vector<int> tracked = {0};

  const auto moved = workspaces[0].clients()[0];
  workspaces[0].remove_client(moved.window);
  scrollwm::model::ensure_workspace_exists(workspaces, 1);
  workspaces[1].add_client(moved);
  scrollwm::model::cleanup_empty_workspaces(workspaces, tracked);

  return expect(workspaces.size() == 1, "moving only client out should remove empty source workspace") &&
         expect(workspaces[0].clients().size() == 2, "destination workspace should contain moved client") &&
         expect(tracked[0] == 0, "tracked index should remain valid after move cleanup");
}

bool test_layout_empty_workspace_returns_no_rects() {
  Workspace ws(0);
  ScrollLayout layout(Direction::Horizontal);
  int offset = 123;
  const auto rects = layout.compute(ws, 1280, 720, 10, 2, 10, offset);

  return expect(rects.empty(), "empty workspace should produce empty geometry") &&
         expect(offset == 0, "offset should reset to zero for empty workspace");
}

bool test_layout_single_client_is_sensible() {
  Workspace ws(0);
  ws.add_client({.window = 1});
  ScrollLayout layout(Direction::Horizontal);
  int offset = 0;
  const auto rects = layout.compute(ws, 1280, 720, 12, 2, 12, offset);

  return expect(rects.size() == 1, "single client should produce one rect") &&
         expect(rects[0].width > 0 && rects[0].height > 0, "single rect should have positive size") &&
         expect(offset == 0, "single focused client should not require scrolling");
}

bool test_layout_horizontal_vertical_geometry_sanity() {
  Workspace ws(0);
  ws.add_client({.window = 10});
  ws.add_client({.window = 11});
  ws.add_client({.window = 12});

  int horizontal_offset = 0;
  ScrollLayout horizontal(Direction::Horizontal);
  const auto horizontal_rects = horizontal.compute(ws, 1920, 1080, 8, 2, 16, horizontal_offset);

  int vertical_offset = 0;
  ScrollLayout vertical(Direction::Vertical);
  const auto vertical_rects = vertical.compute(ws, 1920, 1080, 8, 2, 16, vertical_offset);

  const bool horizontal_spacing_ok = expect(horizontal_rects.size() == 3, "horizontal should compute 3 rects") &&
                                     expect(horizontal_rects[1].x > horizontal_rects[0].x,
                                            "horizontal rects should advance on x") &&
                                     expect(horizontal_rects[1].y == horizontal_rects[0].y,
                                            "horizontal rects should share y baseline");

  const bool vertical_spacing_ok = expect(vertical_rects.size() == 3, "vertical should compute 3 rects") &&
                                   expect(vertical_rects[1].y > vertical_rects[0].y,
                                          "vertical rects should advance on y") &&
                                   expect(vertical_rects[1].x == vertical_rects[0].x,
                                          "vertical rects should share x baseline");

  return horizontal_spacing_ok && vertical_spacing_ok;
}

bool test_layout_offset_tracks_focus_changes() {
  Workspace ws(0);
  ws.add_client({.window = 1});
  ws.add_client({.window = 2});
  ws.add_client({.window = 3});

  ScrollLayout layout(Direction::Horizontal);
  int offset = 0;

  ws.focus_index(0);
  (void)layout.compute(ws, 1280, 720, 12, 2, 12, offset);
  const int first_offset = offset;

  ws.focus_index(2);
  (void)layout.compute(ws, 1280, 720, 12, 2, 12, offset);
  const int third_offset = offset;

  return expect(first_offset == 0, "focused head should not scroll") &&
         expect(third_offset > first_offset, "focused tail should increase scroll offset");
}

bool test_layout_gap_border_padding_influence_geometry() {
  Workspace ws(0);
  ws.add_client({.window = 1});
  ws.add_client({.window = 2});

  ScrollLayout layout(Direction::Horizontal);
  int compact_offset = 0;
  const auto compact = layout.compute(ws, 1200, 800, 4, 1, 4, compact_offset);

  int roomy_offset = 0;
  const auto roomy = layout.compute(ws, 1200, 800, 24, 8, 24, roomy_offset);

  return expect(compact.size() == 2 && roomy.size() == 2, "both geometry runs should have 2 rects") &&
         expect(roomy[0].x > compact[0].x, "larger padding should move windows inward") &&
         expect(roomy[0].width < compact[0].width, "larger gap/border/padding should reduce available width") &&
         expect(roomy[0].height < compact[0].height, "larger gap should reduce available height");
}

bool test_layout_focus_centering_and_direction_toggle() {
  Workspace ws(0);
  ws.add_client({.window = 10});
  ws.add_client({.window = 11});
  ws.add_client({.window = 12});
  ws.focus_index(2);

  ScrollLayout layout(Direction::Horizontal);
  int offset = 0;
  const auto rects = layout.compute(ws, 1920, 1080, 12, 2, 12, offset);

  const bool horizontal_focus_ok = expect(rects.size() == 3, "expected 3 horizontal rects") &&
                                   expect(offset > 0, "focused tail window should require positive offset");

  const auto& focused = rects[2];
  const int focused_center = focused.x + (focused.width / 2);
  const int viewport_center = 1920 / 2;
  const bool centered_ok = expect(std::abs(focused_center - viewport_center) <= 300,
                                  "focused client should remain near viewport center");

  layout.set_direction(Direction::Vertical);
  offset = 0;
  const auto vertical_rects = layout.compute(ws, 1920, 1080, 12, 2, 12, offset);
  const bool direction_change_ok = expect(vertical_rects.size() == 3, "expected 3 vertical rects") &&
                                   expect(vertical_rects[0].x != rects[0].x,
                                          "changing direction should alter layout geometry");

  return horizontal_focus_ok && centered_ok && direction_change_ok;
}

}  // namespace

int main() {
  const std::vector<std::pair<std::string, std::function<bool()>>> tests = {
      {"config defaults", test_config_defaults_when_sections_omitted},
      {"config builtins + dynamic workspace bindings", test_config_builtins_and_dynamic_workspace_bindings},
      {"config exec order", test_config_exec_entries_parse_in_order},
      {"config exec malformed ignored", test_config_exec_malformed_entries_ignored},
      {"config exec whitespace/comments/unknowns", test_config_exec_whitespace_comments_unknown_sections},
      {"config clamps", test_config_clamps_workspace_and_schema_versions},
      {"workspace remove middle focused", test_workspace_remove_middle_focused_behavior},
      {"workspace remove only", test_workspace_remove_only_client_clears_focus},
      {"workspace reorder boundaries", test_workspace_reorder_boundaries_and_wrap_behavior},
      {"workspace focus urgent none", test_workspace_focus_urgent_when_none_exists},
      {"workspace focus index out of range", test_workspace_focus_index_out_of_range_noop},
      {"workspace cleanup removes empty non-last", test_dynamic_workspace_cleanup_removes_empty_non_last_workspace},
      {"workspace cleanup keeps last", test_dynamic_workspace_cleanup_keeps_last_workspace_alive},
      {"workspace cleanup focused fallback forward", test_dynamic_workspace_cleanup_focused_workspace_falls_forward},
      {"workspace cleanup focused fallback backward", test_dynamic_workspace_cleanup_focused_workspace_falls_backward_at_end},
      {"workspace cleanup repeated deletions", test_dynamic_workspace_cleanup_repeated_deletions_stable},
      {"workspace cleanup first/last/middle", test_dynamic_workspace_cleanup_delete_first_and_last_and_middle},
      {"workspace ensure exists contiguous", test_dynamic_workspace_ensure_exists_expands_contiguously},
      {"workspace move cleanup simulation", test_dynamic_workspace_move_client_cleanup_simulation},
      {"layout empty", test_layout_empty_workspace_returns_no_rects},
      {"layout single", test_layout_single_client_is_sensible},
      {"layout horizontal+vertical sanity", test_layout_horizontal_vertical_geometry_sanity},
      {"layout focus offset", test_layout_offset_tracks_focus_changes},
      {"layout gap border padding", test_layout_gap_border_padding_influence_geometry},
      {"layout focus centering + direction", test_layout_focus_centering_and_direction_toggle},
  };

  for (const auto& [name, test] : tests) {
    if (!test()) {
      std::cerr << "FAILED: " << name << "\n";
      return 1;
    }
  }

  std::cout << "All unit tests passed\n";
  return 0;
}
