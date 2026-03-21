#include "mimic/layout_engine.hpp"

#include <algorithm>

namespace mimic {

void LayoutEngine::insert_after_focus(WindowId id) {
    if (!focused_.has_value()) {
        windows_.push_back(id);
        focused_ = id;
        return;
    }

    const auto it = std::find(windows_.begin(), windows_.end(), focused_.value());
    if (it == windows_.end()) {
        windows_.push_back(id);
        focused_ = id;
        return;
    }

    windows_.insert(std::next(it), id);
    focused_ = id;
}

void LayoutEngine::remove(WindowId id) {
    auto it = std::find(windows_.begin(), windows_.end(), id);
    if (it == windows_.end()) {
        return;
    }

    const auto removed_index = static_cast<size_t>(std::distance(windows_.begin(), it));
    windows_.erase(it);
    if (windows_.empty()) {
        focused_.reset();
        return;
    }

    if (focused_ && focused_.value() == id) {
        const auto new_index = std::min(removed_index, windows_.size() - 1);
        focused_ = windows_[new_index];
    }
}

void LayoutEngine::move_focus(Direction direction) {
    const auto index_opt = focused_index();
    if (!index_opt.has_value()) {
        return;
    }

    size_t index = index_opt.value();
    if (direction == Direction::Left && index > 0) {
        index -= 1;
    } else if (direction == Direction::Right && index + 1 < windows_.size()) {
        index += 1;
    }
    focused_ = windows_[index];
}

void LayoutEngine::set_focus(WindowId id) {
    if (std::find(windows_.begin(), windows_.end(), id) != windows_.end()) {
        focused_ = id;
    }
}

std::optional<WindowId> LayoutEngine::focused() const {
    return focused_;
}

std::vector<WindowPlacement> LayoutEngine::compute(int viewport_width, int viewport_height, int gaps) const {
    std::vector<WindowPlacement> out;
    if (windows_.empty()) {
        return out;
    }

    const int tile_width = std::max(1, viewport_width - (2 * gaps));
    const int tile_height = std::max(1, viewport_height - (2 * gaps));
    const int pitch = tile_width + gaps;

    size_t focused_idx = 0;
    if (const auto idx = focused_index(); idx.has_value()) {
        focused_idx = idx.value();
    }
    const int centered_x = (viewport_width - tile_width) / 2;

    for (size_t i = 0; i < windows_.size(); ++i) {
        const int relative = static_cast<int>(i) - static_cast<int>(focused_idx);
        WindowPlacement placement;
        placement.id = windows_[i];
        placement.x = centered_x + (relative * pitch);
        placement.y = gaps;
        placement.width = tile_width;
        placement.height = tile_height;
        out.push_back(placement);
    }
    return out;
}

const std::vector<WindowId>& LayoutEngine::windows() const {
    return windows_;
}

std::optional<size_t> LayoutEngine::focused_index() const {
    if (!focused_.has_value()) {
        return std::nullopt;
    }
    const auto it = std::find(windows_.begin(), windows_.end(), focused_.value());
    if (it == windows_.end()) {
        return std::nullopt;
    }
    return static_cast<size_t>(std::distance(windows_.begin(), it));
}

} // namespace mimic
