#pragma once

#include <X11/Xlib.h>

#include <optional>
#include <vector>

namespace mimic {

struct OverviewTile {
  Window window;
  int x;
  int y;
  int width;
  int height;
};

class MimicOverviewController {
 public:
  enum class State { kInactive, kActive };

  State state() const { return state_; }
  bool enter(const std::vector<Window>& visible_windows, int screen_width, int screen_height);
  std::optional<Window> pick_window(std::size_t index);
  void exit();

  const std::vector<OverviewTile>& tiles() const { return tiles_; }

 private:
  State state_ = State::kInactive;
  std::vector<OverviewTile> tiles_;
};

}  // namespace mimic
