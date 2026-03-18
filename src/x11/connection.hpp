#pragma once

#include <memory>

#include <xcb/xcb.h>

namespace scrollwm::x11 {

class Connection {
 public:
  Connection();
  ~Connection();

  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  bool valid() const;
  xcb_connection_t* raw() const;
  xcb_screen_t* screen() const;

 private:
  xcb_connection_t* conn_ = nullptr;
  xcb_screen_t* screen_ = nullptr;
};

}  // namespace scrollwm::x11
