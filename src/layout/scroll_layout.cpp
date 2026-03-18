#include "layout/scroll_layout.hpp"

#include <algorithm>

namespace scrollwm::layout {

ScrollLayout::ScrollLayout(config::Direction direction) : direction_(direction) {}

std::vector<Rect> ScrollLayout::compute(const model::Workspace& workspace,
                                        int screen_width,
                                        int screen_height,
                                        int gap,
                                        int border_width,
                                        int outer_padding,
                                        int& in_out_scroll_offset) const {
  std::vector<Rect> output;
  const auto& clients = workspace.clients();
  if (clients.empty()) {
    in_out_scroll_offset = 0;
    return output;
  }

  const int count = static_cast<int>(clients.size());
  const int safe_gap = std::max(gap, 0);
  const int padding = std::max(outer_padding, 0);
  const int border = std::max(border_width, 0);

  const int inner_w = std::max(1, screen_width - (padding * 2));
  const int inner_h = std::max(1, screen_height - (padding * 2));

  const int cell_w = direction_ == config::Direction::Horizontal
                         ? std::max(50, inner_w - safe_gap * 2 - border * 2)
                         : std::max(100, inner_w - border * 2);
  const int cell_h = direction_ == config::Direction::Vertical
                         ? std::max(50, inner_h - safe_gap * 2 - border * 2)
                         : std::max(100, inner_h - border * 2);

  const int stride = (direction_ == config::Direction::Horizontal)
                         ? (cell_w + safe_gap)
                         : (cell_h + safe_gap);

  if (auto focused = workspace.focused_index(); focused.has_value()) {
    const int idx = static_cast<int>(*focused);
    const int focused_center = idx * stride + stride / 2;
    const int viewport_center = (direction_ == config::Direction::Horizontal) ? inner_w / 2 : inner_h / 2;
    in_out_scroll_offset = focused_center - viewport_center;
    in_out_scroll_offset = std::max(0, in_out_scroll_offset);
  }

  output.reserve(clients.size());
  for (int i = 0; i < count; ++i) {
    if (direction_ == config::Direction::Horizontal) {
      const int virtual_x = i * stride;
      output.push_back(Rect{
          .x = padding + virtual_x - in_out_scroll_offset,
          .y = padding + safe_gap,
          .width = cell_w,
          .height = std::max(20, inner_h - safe_gap * 2),
      });
    } else {
      const int virtual_y = i * stride;
      output.push_back(Rect{
          .x = padding + safe_gap,
          .y = padding + virtual_y - in_out_scroll_offset,
          .width = std::max(20, inner_w - safe_gap * 2),
          .height = cell_h,
      });
    }
  }

  return output;
}

}  // namespace scrollwm::layout
