#pragma once

#include <string>

#include <xcb/xcb.h>

namespace scrollwm::x11 {

struct Atoms {
  xcb_atom_t wm_protocols = XCB_ATOM_NONE;
  xcb_atom_t wm_delete_window = XCB_ATOM_NONE;
  xcb_atom_t net_active_window = XCB_ATOM_NONE;
  xcb_atom_t net_client_list = XCB_ATOM_NONE;
  xcb_atom_t net_number_of_desktops = XCB_ATOM_NONE;
  xcb_atom_t net_current_desktop = XCB_ATOM_NONE;
  xcb_atom_t net_wm_desktop = XCB_ATOM_NONE;
};

xcb_atom_t intern_atom(xcb_connection_t* conn, const std::string& name);
Atoms create_atoms(xcb_connection_t* conn);

}  // namespace scrollwm::x11
