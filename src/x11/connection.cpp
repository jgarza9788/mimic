#include "x11/connection.hpp"

namespace scrollwm::x11 {

Connection::Connection() {
  int screen_index = 0;
  conn_ = xcb_connect(nullptr, &screen_index);
  if (xcb_connection_has_error(conn_) != 0) {
    return;
  }

  const xcb_setup_t* setup = xcb_get_setup(conn_);
  xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
  for (int i = 0; i < screen_index; ++i) {
    xcb_screen_next(&iter);
  }
  screen_ = iter.data;
}

Connection::~Connection() {
  if (conn_ != nullptr) {
    xcb_disconnect(conn_);
  }
}

bool Connection::valid() const {
  return conn_ != nullptr && screen_ != nullptr && xcb_connection_has_error(conn_) == 0;
}

xcb_connection_t* Connection::raw() const { return conn_; }

xcb_screen_t* Connection::screen() const { return screen_; }

}  // namespace scrollwm::x11
