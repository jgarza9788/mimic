#pragma once

#include "mimic/types.hpp"

namespace mimic {

/**
 * @brief Future-facing compositor integration facade for picom metadata.
 */
class CompositorBridge {
public:
    /**
     * @brief Publishes current focus and layout context for compositor consumers.
     */
    void publish_focus(WindowId id) const;

    /**
     * @brief Publishes workspace scroll offset for animation-aware compositors.
     */
    void publish_scroll_offset(int offset) const;
};

} // namespace mimic
