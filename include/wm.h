#ifndef MIMIC_WM_H
#define MIMIC_WM_H

#include <stdbool.h>
#include <time.h>
#include <xcb/xcb.h>

#include "config.h"

typedef struct Client Client;
typedef struct WorkspaceState WorkspaceState;

typedef struct {
    char terminal[512];
    char menu[512];
    char picom_backend[64];
} RuntimeConfig;

struct Client {
    xcb_window_t win;
    int x;
    int y;
    int w;
    int h;
    int oldx;
    int oldy;
    int oldw;
    int oldh;
    int workspace;
    bool is_fullscreen;
    bool is_floating;
    bool is_transient;
    bool is_minimized;
    Client *next;
    Client *ws_prev;
    Client *ws_next;
};

struct WorkspaceState {
    Client *scroll_head;
    Client *scroll_focus;
    Client *min_restore_cursor;
};

typedef struct {
    xcb_connection_t *dpy;
    xcb_screen_t *screen;
    xcb_window_t root;
    unsigned int sw;
    unsigned int sh;
    uint32_t border_focus;
    uint32_t border_normal;

    xcb_atom_t wm_delete;
    xcb_atom_t wm_protocols;
    xcb_atom_t net_active_window;
    xcb_atom_t net_wm_state;
    xcb_atom_t net_wm_state_fullscreen;
    xcb_atom_t net_supported;
    xcb_atom_t net_wm_desktop;
    xcb_atom_t net_current_desktop;
    xcb_atom_t net_wm_window_type;
    xcb_atom_t net_wm_window_type_dock;

    Client *clients;
    Client *focused;

    WorkspaceState workspaces[WORKSPACE_COUNT];
    int current_workspace;
    bool running;

    bool drag_active;
    bool drag_resize;
    int drag_start_x;
    int drag_start_y;
    int drag_win_x;
    int drag_win_y;
    int drag_win_w;
    int drag_win_h;
    Client *drag_client;

    unsigned int numlock_mask;

    char runtime_toml_path[1024];
    time_t runtime_toml_mtime_sec;
    RuntimeConfig runtime_config;
} WM;

extern WM wm;

void wm_init(void);
void wm_run(void);
void wm_cleanup(void);

#endif
