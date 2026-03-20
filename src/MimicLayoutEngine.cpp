#include "mimic/MimicLayoutEngine.hh"

#include <algorithm>

namespace mimic {

void MimicLayoutEngine::set_window_order(const std::vector<Window>& ordered_windows) {
  ordered_windows_ = ordered_windows;
  if (focused_index_ >= ordered_windows_.size()) {
    focused_index_ = 0;
  }
}

void MimicLayoutEngine::append_window(Window window) {
  ordered_windows_.push_back(window);
}

void MimicLayoutEngine::remove_window(Window window) {
  auto it = std::find(ordered_windows_.begin(), ordered_windows_.end(), window);
  if (it == ordered_windows_.end()) {
    return;
  }

  const auto removed_index = static_cast<std::size_t>(std::distance(ordered_windows_.begin(), it));
  ordered_windows_.erase(it);

  if (ordered_windows_.empty()) {
    focused_index_ = 0;
  } else if (removed_index <= focused_index_ && focused_index_ > 0) {
    --focused_index_;
  } else if (focused_index_ >= ordered_windows_.size()) {
    focused_index_ = ordered_windows_.size() - 1;
  }
}

std::optional<Window> MimicLayoutEngine::current_window() const {
  if (ordered_windows_.empty()) {
    return std::nullopt;
  }

  return ordered_windows_[focused_index_];
}

std::optional<Window> MimicLayoutEngine::focus_next() {
  if (ordered_windows_.empty()) {
    return std::nullopt;
  }

  focused_index_ = (focused_index_ + 1) % ordered_windows_.size();
  return ordered_windows_[focused_index_];
}

std::optional<Window> MimicLayoutEngine::focus_previous() {
  if (ordered_windows_.empty()) {
    return std::nullopt;
  }

  focused_index_ = (focused_index_ + ordered_windows_.size() - 1) % ordered_windows_.size();
  return ordered_windows_[focused_index_];
}

void MimicLayoutEngine::scroll_by(int dx, int dy) {
  viewport_.offset_x += dx;
  viewport_.offset_y += dy;
}

}  // namespace mimic
