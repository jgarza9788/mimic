#pragma once

#include <cstdint>

#include <xcb/xcb.h>

namespace scrollwm::model {

struct Client {
  xcb_window_t window = XCB_WINDOW_NONE;
  bool floating = false;
  bool fullscreen = false;
  bool urgent = false;
};

}  // namespace scrollwm::model
