#ifndef MIMIC_WM_H
#define MIMIC_WM_H

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <stdbool.h>
#include <time.h>

#include "config.h"

typedef struct Client Client;
typedef struct WorkspaceState WorkspaceState;

typedef struct {
    char terminal[512];
    char menu[512];
    char picom_backend[64];
} RuntimeConfig;

struct Client {
    Window win;
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
    Display *dpy;
    int screen;
    Window root;
    unsigned int sw;
    unsigned int sh;
    unsigned long border_focus;
    unsigned long border_normal;

    Atom wm_delete;
    Atom wm_protocols;
    Atom net_active_window;
    Atom net_wm_state;
    Atom net_wm_state_fullscreen;
    Atom net_supported;
    Atom net_wm_desktop;
    Atom net_current_desktop;
    Atom net_wm_window_type;
    Atom net_wm_window_type_dock;

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
