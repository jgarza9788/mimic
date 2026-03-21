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

struct MimicWindowFrame {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

struct MimicLayoutOptions {
  int gap_px = 16;
  int edge_padding_px = 0;
  int minimum_window_width_px = 640;
  double primary_window_width_ratio = 0.6;
};

class MimicLayoutEngine {
 public:
  void set_layout_options(const MimicLayoutOptions& options);
  void set_window_order(const std::vector<Window>& ordered_windows);
  void append_window(Window window);
  void remove_window(Window window);

  std::optional<Window> current_window() const;
  std::optional<Window> focus_next();
  std::optional<Window> focus_previous();

  std::vector<MimicWindowFrame> compute_frames(int output_width, int output_height) const;

  const MimicViewport& viewport() const { return viewport_; }
  void scroll_by(int dx, int dy);

 private:
  MimicLayoutOptions options_;
  std::vector<Window> ordered_windows_;
  std::size_t focused_index_ = 0;
  MimicViewport viewport_;
};

}  // namespace mimic
