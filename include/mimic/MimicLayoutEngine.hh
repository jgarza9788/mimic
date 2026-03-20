#pragma once

#include <X11/Xlib.h>

#include <cstddef>
#include <optional>
#include <vector>

namespace mimic {

class MimicViewport {
 public:
  int offset_x = 0;
  int offset_y = 0;
};

class MimicLayoutEngine {
 public:
  void set_window_order(const std::vector<Window>& ordered_windows);
  void append_window(Window window);
  void remove_window(Window window);

  std::optional<Window> current_window() const;
  std::optional<Window> focus_next();
  std::optional<Window> focus_previous();

  const MimicViewport& viewport() const { return viewport_; }
  void scroll_by(int dx, int dy);

 private:
  std::vector<Window> ordered_windows_;
  std::size_t focused_index_ = 0;
  MimicViewport viewport_;
};

}  // namespace mimic
