#pragma once

#include <vector>

#include "config/config.hpp"
#include "model/workspace.hpp"

namespace scrollwm::layout {

struct Rect {
  int x;
  int y;
  int width;
  int height;
};

class ScrollLayout {
 public:
  explicit ScrollLayout(config::Direction direction);
  void set_direction(config::Direction direction);
  config::Direction direction() const;

  std::vector<Rect> compute(const model::Workspace& workspace,
                            int screen_width,
                            int screen_height,
                            int gap,
                            int border_width,
                            int outer_padding,
                            int& in_out_scroll_offset) const;

 private:
  config::Direction direction_;
};

}  // namespace scrollwm::layout
