#pragma once

#include <xcb/xcb.h>

namespace scrollwm::x11 {

inline uint8_t event_type(const xcb_generic_event_t* event) {
  return static_cast<uint8_t>(event->response_type & ~0x80U);
}

}  // namespace scrollwm::x11
