#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace mimic {

/**
 * @brief Core identifier type used for windows in layout simulations and runtime state.
 */
using WindowId = uint32_t;

/**
 * @brief Small structure representing a managed window.
 */
struct ManagedWindow {
    WindowId id {0};
    bool floating {false};
    bool fullscreen {false};
    std::string app_class;
    std::string title;
};

/**
 * @brief Spatial placement metadata for a tiled window.
 */
struct WindowPlacement {
    WindowId id {0};
    int x {0};
    int y {0};
    int width {0};
    int height {0};
};

/**
 * @brief Focus move direction enum.
 */
enum class Direction {
    Left,
    Right,
};

} // namespace mimic
