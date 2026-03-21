#pragma once

#include <optional>
#include <vector>

#include "mimic/types.hpp"

namespace mimic {

/**
 * @brief Niri-inspired horizontal strip layout model for one workspace.
 */
class LayoutEngine {
public:
    /**
     * @brief Inserts a new tiled window after focused window or appends if none.
     */
    void insert_after_focus(WindowId id);

    /**
     * @brief Removes a window and keeps focus valid if possible.
     */
    void remove(WindowId id);

    /**
     * @brief Moves focus one step left or right in the strip.
     */
    void move_focus(Direction direction);

    /**
     * @brief Sets focus to a specific window if it exists.
     */
    void set_focus(WindowId id);

    /**
     * @brief Returns current focused window id.
     */
    [[nodiscard]] std::optional<WindowId> focused() const;

    /**
     * @brief Computes tiled placements relative to viewport and gaps.
     */
    [[nodiscard]] std::vector<WindowPlacement> compute(int viewport_width,
                                                       int viewport_height,
                                                       int gaps) const;

    /**
     * @brief Provides immutable ordered window ids for tests and IPC.
     */
    [[nodiscard]] const std::vector<WindowId>& windows() const;

private:
    /**
     * @brief Returns focused index when available.
     */
    [[nodiscard]] std::optional<size_t> focused_index() const;

    std::vector<WindowId> windows_;
    std::optional<WindowId> focused_;
};

} // namespace mimic
