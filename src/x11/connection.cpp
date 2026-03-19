#include "x11/connection.hpp"

#include <sstream>

namespace scrollwm::x11 {

Connection::Connection() {
  int screen_index = 0;
  conn_ = xcb_connect(nullptr, &screen_index);
  if (conn_ == nullptr || xcb_connection_has_error(conn_) != 0) {
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

int Connection::error_code() const {
  if (conn_ == nullptr) {
    return XCB_CONN_ERROR;
  }
  return xcb_connection_has_error(conn_);
}

std::string Connection::error_message() const {
  switch (error_code()) {
    case 0:
      return "no X11 connection error";
    case XCB_CONN_ERROR:
      return "X server connection failed (check DISPLAY/X server availability)";
    case XCB_CONN_CLOSED_EXT_NOTSUPPORTED:
      return "required X11 extension not supported by server";
    case XCB_CONN_CLOSED_MEM_INSUFFICIENT:
      return "insufficient memory while connecting to X server";
    case XCB_CONN_CLOSED_REQ_LEN_EXCEED:
      return "X11 request length exceeded server limit";
    case XCB_CONN_CLOSED_PARSE_ERR:
      return "display string parse error (invalid DISPLAY value)";
    case XCB_CONN_CLOSED_INVALID_SCREEN:
      return "invalid X11 screen index";
    case XCB_CONN_CLOSED_FDPASSING_FAILED:
      return "file-descriptor passing failed in X11 transport";
    default: {
      std::ostringstream oss;
      oss << "unknown X11 connection error (" << error_code() << ")";
      return oss.str();
    }
  }
}

xcb_connection_t* Connection::raw() const { return conn_; }

xcb_screen_t* Connection::screen() const { return screen_; }

}  // namespace scrollwm::x11
