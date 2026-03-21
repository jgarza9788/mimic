#include "mimic/compositor_bridge.hpp"

namespace mimic {

void CompositorBridge::publish_focus(WindowId /*id*/) const {
    // TODO: Publish custom X atoms for picom-aware focus effects.
}

void CompositorBridge::publish_scroll_offset(int /*offset*/) const {
    // TODO: Publish scroll metadata for compositor-side animation strategies.
}

} // namespace mimic
