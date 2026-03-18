#include "x11/atoms.hpp"

namespace scrollwm::x11 {

xcb_atom_t intern_atom(xcb_connection_t* conn, const std::string& name) {
  auto cookie = xcb_intern_atom(conn, 0, static_cast<uint16_t>(name.size()), name.c_str());
  xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(conn, cookie, nullptr);
  if (reply == nullptr) {
    return XCB_ATOM_NONE;
  }
  const xcb_atom_t atom = reply->atom;
  free(reply);
  return atom;
}

Atoms create_atoms(xcb_connection_t* conn) {
  Atoms atoms;
  atoms.wm_protocols = intern_atom(conn, "WM_PROTOCOLS");
  atoms.wm_delete_window = intern_atom(conn, "WM_DELETE_WINDOW");
  atoms.net_active_window = intern_atom(conn, "_NET_ACTIVE_WINDOW");
  atoms.net_client_list = intern_atom(conn, "_NET_CLIENT_LIST");
  atoms.net_number_of_desktops = intern_atom(conn, "_NET_NUMBER_OF_DESKTOPS");
  atoms.net_current_desktop = intern_atom(conn, "_NET_CURRENT_DESKTOP");
  atoms.net_wm_desktop = intern_atom(conn, "_NET_WM_DESKTOP");
  return atoms;
}

}  // namespace scrollwm::x11
