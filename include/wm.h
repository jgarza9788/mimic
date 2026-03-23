#ifndef MIMIC_WM_H
#define MIMIC_WM_H

#include <stdbool.h>
#include <time.h>
#include <xcb/xcb.h>

#include "config.h"

typedef struct Client Client;
typedef struct WorkspaceState WorkspaceState;

typedef struct {
    /* User-overridable command strings loaded from runtime config. */
    char terminal[512];
    char menu[512];
    char picom_backend[64];
} RuntimeConfig;

struct Client {
    /* X11 window id tracked by the window manager. */
    xcb_window_t win;
    /* Current geometry in root-window coordinates. */
    int x;
    int y;
    int w;
    int h;
    /* Saved geometry used when toggling fullscreen/floating modes. */
    int oldx;
    int oldy;
    int oldw;
    int oldh;
    /* Workspace index where this client currently belongs. */
    int workspace;
    /* State flags that control window behavior. */
    bool is_fullscreen;
    bool is_floating;
    bool is_transient;
    bool is_minimized;
    /* Global client linked list across all workspaces. */
    Client *next;
    /* Per-workspace ordering list for focus/stack traversal. */
    Client *ws_prev;
    Client *ws_next;
};

struct WorkspaceState {
    /* Head of the managed (scroll/tiled) client order for this workspace. */
    Client *scroll_head;
    /* Last focused managed client for this workspace. */
    Client *scroll_focus;
    /* Cursor used to rotate through minimized windows when restoring. */
    Client *min_restore_cursor;
};

typedef struct {
    /* XCB/X11 connection and root-screen metadata. */
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

    /* Per-workspace bookkeeping plus currently active workspace id. */
    WorkspaceState workspaces[WORKSPACE_COUNT];
    int current_workspace;
    bool running;

    /* Mouse drag state used for moving/resizing floating windows. */
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

/* Initialize X11 connection, state, grabs, and EWMH metadata. */
void wm_init(void);
/* Enter the event loop and process incoming X11 events until shutdown. */
void wm_run(void);
/* Release resources and close down cleanly. */
void wm_cleanup(void);

#endif
