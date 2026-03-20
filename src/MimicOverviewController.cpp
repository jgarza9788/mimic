#include "mimic/MimicOverviewController.hh"

#include <cmath>

namespace mimic {

bool MimicOverviewController::enter(const std::vector<Window>& visible_windows,
                                    int screen_width,
                                    int screen_height) {
  if (visible_windows.empty() || screen_width <= 0 || screen_height <= 0) {
    return false;
  }

  tiles_.clear();
  const int count = static_cast<int>(visible_windows.size());
  const int cols = static_cast<int>(std::ceil(std::sqrt(count)));
  const int rows = static_cast<int>(std::ceil(static_cast<double>(count) / cols));

  const int tile_width = screen_width / cols;
  const int tile_height = screen_height / rows;

  for (int i = 0; i < count; ++i) {
    const int row = i / cols;
    const int col = i % cols;
    tiles_.push_back({
        visible_windows[static_cast<std::size_t>(i)],
        col * tile_width,
        row * tile_height,
        tile_width,
        tile_height,
    });
  }

  state_ = State::kActive;
  return true;
}

std::optional<Window> MimicOverviewController::pick_window(std::size_t index) {
  if (state_ != State::kActive || index >= tiles_.size()) {
    return std::nullopt;
  }

  Window picked = tiles_[index].window;
  exit();
  return picked;
}

void MimicOverviewController::exit() {
  tiles_.clear();
  state_ = State::kInactive;
}

}  // namespace mimic
