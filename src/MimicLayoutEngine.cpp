#include "mimic/MimicLayoutEngine.hh"

#include <algorithm>

namespace mimic {

void MimicLayoutEngine::set_layout_options(const MimicLayoutOptions& options) { options_ = options; }

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

std::vector<MimicWindowFrame> MimicLayoutEngine::compute_frames(int output_width, int output_height) const {
  std::vector<MimicWindowFrame> frames;
  if (ordered_windows_.empty() || output_width <= 0 || output_height <= 0) {
    return frames;
  }

  const int safe_padding = std::max(0, options_.edge_padding_px);
  const int available_width = std::max(1, output_width - (safe_padding * 2));
  const int available_height = std::max(1, output_height - (safe_padding * 2));
  const double clamped_ratio = std::clamp(options_.primary_window_width_ratio, 0.1, 1.0);
  const int preferred_width = static_cast<int>(available_width * clamped_ratio);
  const int minimum_width = std::min(available_width, std::max(1, options_.minimum_window_width_px));
  const int window_width = std::max(1, std::min(available_width, std::max(minimum_width, preferred_width)));
  const int gap = std::max(0, options_.gap_px);

  frames.reserve(ordered_windows_.size());
  for (std::size_t i = 0; i < ordered_windows_.size(); ++i) {
    const int index = static_cast<int>(i);
    const int x = safe_padding + index * (window_width + gap) + viewport_.offset_x;
    const int y = safe_padding + viewport_.offset_y;
    frames.push_back({x, y, window_width, available_height});
  }

  return frames;
}

void MimicLayoutEngine::scroll_by(int dx, int dy) {
  viewport_.offset_x += dx;
  viewport_.offset_y += dy;
}

}  // namespace mimic
