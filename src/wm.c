#include <X11/XKBlib.h>
#include <X11/Xutil.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "wm.h"

WM wm = {0};

static void die(const char *msg) {
    fprintf(stderr, "mimicwm: %s\n", msg);
    exit(EXIT_FAILURE);
}

static int xerror(Display *dpy, XErrorEvent *ee) {
    char msg[1024] = {0};
    XGetErrorText(dpy, ee->error_code, msg, sizeof(msg));
    if (ee->error_code == BadAccess || ee->error_code == BadWindow) {
        fprintf(stderr, "mimicwm: XError ignored: %s\n", msg);
        return 0;
    }
    fprintf(stderr,
            "mimicwm: XError req=%d err=%d resource=0x%lx: %s\n",
            ee->request_code,
            ee->error_code,
            ee->resourceid,
            msg);
    return 0;
}

static unsigned long color_from_hex(unsigned int rgb) {
    XColor color;
    color.red = ((rgb >> 16) & 0xff) * 257;
    color.green = ((rgb >> 8) & 0xff) * 257;
    color.blue = (rgb & 0xff) * 257;
    color.flags = DoRed | DoGreen | DoBlue;
    if (!XAllocColor(wm.dpy, DefaultColormap(wm.dpy, wm.screen), &color)) {
        return BlackPixel(wm.dpy, wm.screen);
    }
    return color.pixel;
}

static void spawn(const char *cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        if (wm.dpy) {
            close(ConnectionNumber(wm.dpy));
        }
        setsid();
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        fprintf(stderr, "mimicwm: failed to exec '%s': %s\n", cmd, strerror(errno));
        _exit(EXIT_FAILURE);
    }
}

static Client *find_client(Window w) {
    for (Client *c = wm.clients; c; c = c->next) {
        if (c->win == w) {
            return c;
        }
    }
    return NULL;
}

static bool is_visible(Client *c) {
    return c && !c->is_minimized && c->workspace == wm.current_workspace;
}

static void update_current_desktop(void) {
    unsigned long desk = (unsigned long)wm.current_workspace;
    XChangeProperty(wm.dpy,
                    wm.root,
                    wm.net_current_desktop,
                    XA_CARDINAL,
                    32,
                    PropModeReplace,
                    (unsigned char *)&desk,
                    1);
}

static void set_client_desktop(Client *c) {
    unsigned long desk = (unsigned long)c->workspace;
    XChangeProperty(wm.dpy,
                    c->win,
                    wm.net_wm_desktop,
                    XA_CARDINAL,
                    32,
                    PropModeReplace,
                    (unsigned char *)&desk,
                    1);
}

static void set_input_focus(Client *c) {
    if (!c || !is_visible(c)) {
        XSetInputFocus(wm.dpy, wm.root, RevertToPointerRoot, CurrentTime);
        wm.focused = NULL;
        XDeleteProperty(wm.dpy, wm.root, wm.net_active_window);
        return;
    }

    wm.focused = c;
    XSetWindowBorder(wm.dpy, c->win, wm.border_focus);
    XSetInputFocus(wm.dpy, c->win, RevertToPointerRoot, CurrentTime);
    XRaiseWindow(wm.dpy, c->win);

    unsigned long active = c->win;
    XChangeProperty(wm.dpy,
                    wm.root,
                    wm.net_active_window,
                    XA_WINDOW,
                    32,
                    PropModeReplace,
                    (unsigned char *)&active,
                    1);
}

static void focus_next(void) {
    if (!wm.clients) {
        return;
    }

    Client *start = wm.focused ? wm.focused->next : wm.clients;
    Client *c = start;
    while (c && !is_visible(c)) {
        c = c->next;
    }

    if (!c) {
        c = wm.clients;
        while (c && !is_visible(c)) {
            c = c->next;
        }
    }

    if (wm.focused && wm.focused != c) {
        XSetWindowBorder(wm.dpy, wm.focused->win, wm.border_normal);
    }
    if (c) {
        set_input_focus(c);
    }
}

static void focus_prev(void) {
    Client *prev = NULL;
    Client *target = NULL;
    for (Client *c = wm.clients; c; c = c->next) {
        if (!is_visible(c)) {
            continue;
        }
        if (c == wm.focused) {
            break;
        }
        prev = c;
    }
    if (prev) {
        target = prev;
    } else {
        for (Client *c = wm.clients; c; c = c->next) {
            if (is_visible(c)) {
                target = c;
            }
        }
    }

    if (wm.focused && wm.focused != target) {
        XSetWindowBorder(wm.dpy, wm.focused->win, wm.border_normal);
    }
    if (target) {
        set_input_focus(target);
    }
}

static void update_visibility(void) {
    for (Client *c = wm.clients; c; c = c->next) {
        if (is_visible(c) || c->is_fullscreen) {
            XMapWindow(wm.dpy, c->win);
        } else {
            XUnmapWindow(wm.dpy, c->win);
        }
    }
}

static void tile_workspace(void) {
    if (!wm.tiling_mode) {
        return;
    }

    int count = 0;
    for (Client *c = wm.clients; c; c = c->next) {
        if (!is_visible(c) || c->is_floating || c->is_fullscreen) {
            continue;
        }
        count++;
    }

    if (count == 0) {
        return;
    }

    int master_w = wm.sw * 3 / 5;
    int stack_w = wm.sw - master_w;
    int idx = 0;
    int stack_n = count - 1;

    for (Client *c = wm.clients; c; c = c->next) {
        if (!is_visible(c) || c->is_floating || c->is_fullscreen) {
            continue;
        }

        if (idx == 0) {
            c->x = 0;
            c->y = 0;
            c->w = master_w - 2 * BORDER_WIDTH;
            c->h = wm.sh - 2 * BORDER_WIDTH;
        } else {
            int h = wm.sh / (stack_n > 0 ? stack_n : 1);
            c->x = master_w;
            c->y = (idx - 1) * h;
            c->w = stack_w - 2 * BORDER_WIDTH;
            c->h = h - 2 * BORDER_WIDTH;
        }

        XMoveResizeWindow(wm.dpy, c->win, c->x, c->y, c->w, c->h);
        idx++;
    }
}

static void arrange(void) {
    update_visibility();
    tile_workspace();

    if (!wm.focused || !is_visible(wm.focused)) {
        focus_next();
    }
    XFlush(wm.dpy);
}

static bool is_dock_window(Window w) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;

    int status = XGetWindowProperty(wm.dpy,
                                    w,
                                    wm.net_wm_window_type,
                                    0,
                                    8,
                                    False,
                                    XA_ATOM,
                                    &actual_type,
                                    &actual_format,
                                    &nitems,
                                    &bytes_after,
                                    &prop);
    if (status == Success && prop) {
        Atom *types = (Atom *)prop;
        for (unsigned long i = 0; i < nitems; i++) {
            if (types[i] == wm.net_wm_window_type_dock) {
                XFree(prop);
                return true;
            }
        }
        XFree(prop);
    }
    return false;
}

static Client *manage(Window w) {
    if (find_client(w)) {
        return NULL;
    }

    XWindowAttributes wa;
    if (!XGetWindowAttributes(wm.dpy, w, &wa)) {
        return NULL;
    }
    if (wa.override_redirect || wa.map_state == IsUnmapped || is_dock_window(w)) {
        return NULL;
    }

    Client *c = calloc(1, sizeof(*c));
    if (!c) {
        die("out of memory");
    }

    c->win = w;
    c->x = wa.x;
    c->y = wa.y;
    c->w = wa.width;
    c->h = wa.height;
    c->workspace = wm.current_workspace;
    c->is_floating = true;

    XWMHints *hints = XGetWMHints(wm.dpy, w);
    if (hints && (hints->flags & StateHint) && hints->initial_state == IconicState) {
        c->is_minimized = true;
    }
    if (hints) {
        XFree(hints);
    }

    Window trans;
    if (XGetTransientForHint(wm.dpy, w, &trans)) {
        c->is_transient = true;
        c->is_floating = true;
    }

    XSelectInput(wm.dpy,
                 w,
                 EnterWindowMask | FocusChangeMask | StructureNotifyMask | PropertyChangeMask);
    XSetWindowBorderWidth(wm.dpy, w, BORDER_WIDTH);
    XSetWindowBorder(wm.dpy, w, wm.border_normal);

    c->next = wm.clients;
    wm.clients = c;
    set_client_desktop(c);

    if (!c->is_minimized) {
        XMapWindow(wm.dpy, c->win);
    }

    set_input_focus(c);
    return c;
}

static void unmanage(Client *c) {
    if (!c) {
        return;
    }

    if (wm.focused == c) {
        wm.focused = NULL;
    }

    Client **pp = &wm.clients;
    while (*pp && *pp != c) {
        pp = &(*pp)->next;
    }
    if (*pp) {
        *pp = c->next;
    }

    XSetWindowBorderWidth(wm.dpy, c->win, 0);
    free(c);
}

static void client_close(Client *c) {
    if (!c) {
        return;
    }

    Atom *protocols = NULL;
    int n = 0;
    if (XGetWMProtocols(wm.dpy, c->win, &protocols, &n)) {
        for (int i = 0; i < n; i++) {
            if (protocols[i] == wm.wm_delete) {
                XEvent ev = {0};
                ev.xclient.type = ClientMessage;
                ev.xclient.window = c->win;
                ev.xclient.message_type = wm.wm_protocols;
                ev.xclient.format = 32;
                ev.xclient.data.l[0] = wm.wm_delete;
                ev.xclient.data.l[1] = CurrentTime;
                XSendEvent(wm.dpy, c->win, False, NoEventMask, &ev);
                XFree(protocols);
                return;
            }
        }
        XFree(protocols);
    }

    XKillClient(wm.dpy, c->win);
}

static void client_toggle_fullscreen(Client *c) {
    if (!c) {
        return;
    }

    c->is_fullscreen = !c->is_fullscreen;
    if (c->is_fullscreen) {
        c->oldx = c->x;
        c->oldy = c->y;
        c->oldw = c->w;
        c->oldh = c->h;
        c->x = 0;
        c->y = 0;
        c->w = wm.sw;
        c->h = wm.sh;
    } else {
        c->x = c->oldx;
        c->y = c->oldy;
        c->w = c->oldw;
        c->h = c->oldh;
    }

    if (c->is_fullscreen) {
        Atom fs = wm.net_wm_state_fullscreen;
        XChangeProperty(wm.dpy,
                        c->win,
                        wm.net_wm_state,
                        XA_ATOM,
                        32,
                        PropModeReplace,
                        (unsigned char *)&fs,
                        1);
    } else {
        XDeleteProperty(wm.dpy, c->win, wm.net_wm_state);
    }

    XMoveResizeWindow(wm.dpy, c->win, c->x, c->y, c->w, c->h);
    XRaiseWindow(wm.dpy, c->win);
}

static void client_move(Client *c, int dx, int dy) {
    if (!c || c->is_fullscreen) {
        return;
    }
    c->x += dx;
    c->y += dy;
    XMoveWindow(wm.dpy, c->win, c->x, c->y);
}

static void client_resize(Client *c, int dw, int dh) {
    if (!c || c->is_fullscreen) {
        return;
    }
    c->w += dw;
    c->h += dh;
    if (c->w < 120) c->w = 120;
    if (c->h < 80) c->h = 80;
    XResizeWindow(wm.dpy, c->win, c->w, c->h);
}

static void client_minimize(Client *c) {
    if (!c) {
        return;
    }
    c->is_minimized = true;
    XUnmapWindow(wm.dpy, c->win);
    focus_next();
}

static void client_restore_last(void) {
    for (Client *c = wm.clients; c; c = c->next) {
        if (c->workspace == wm.current_workspace && c->is_minimized) {
            c->is_minimized = false;
            XMapRaised(wm.dpy, c->win);
            set_input_focus(c);
            return;
        }
    }
}

static void set_workspace(int ws) {
    if (ws < 0 || ws >= WORKSPACE_COUNT || ws == wm.current_workspace) {
        return;
    }
    wm.current_workspace = ws;
    update_current_desktop();
    arrange();
}

static void move_client_workspace(Client *c, int ws) {
    if (!c || ws < 0 || ws >= WORKSPACE_COUNT) {
        return;
    }
    c->workspace = ws;
    c->is_minimized = false;
    set_client_desktop(c);
    if (ws != wm.current_workspace) {
        XUnmapWindow(wm.dpy, c->win);
        focus_next();
    }
    arrange();
}

static void grab_keys(void) {
    XUngrabKey(wm.dpy, AnyKey, AnyModifier, wm.root);

    int mods[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};
    KeySym keys[] = {
        XK_Return, XK_q, XK_space, XK_f, XK_m, XK_r,
        XK_Left, XK_Right, XK_Up, XK_Down,
        XK_1, XK_2, XK_3, XK_4
    };

    for (size_t i = 0; i < sizeof(keys)/sizeof(keys[0]); i++) {
        KeyCode code = XKeysymToKeycode(wm.dpy, keys[i]);
        for (size_t m = 0; m < sizeof(mods)/sizeof(mods[0]); m++) {
            XGrabKey(wm.dpy, code, MOD_MASK | mods[m], wm.root, True, GrabModeAsync, GrabModeAsync);
            XGrabKey(wm.dpy, code, MOD_MASK | ShiftMask | mods[m], wm.root, True, GrabModeAsync, GrabModeAsync);
        }
    }
}

static void grab_buttons(void) {
    XUngrabButton(wm.dpy, AnyButton, AnyModifier, wm.root);

    int mods[] = {MOD_MASK, MOD_MASK | LockMask, MOD_MASK | Mod2Mask, MOD_MASK | LockMask | Mod2Mask};
    for (size_t i = 0; i < sizeof(mods)/sizeof(mods[0]); i++) {
        XGrabButton(wm.dpy,
                    Button1,
                    mods[i],
                    wm.root,
                    True,
                    ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                    GrabModeAsync,
                    GrabModeAsync,
                    None,
                    None);

        XGrabButton(wm.dpy,
                    Button3,
                    mods[i],
                    wm.root,
                    True,
                    ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                    GrabModeAsync,
                    GrabModeAsync,
                    None,
                    None);
    }
}

static void keypress(XKeyEvent *e) {
    KeySym sym = XkbKeycodeToKeysym(wm.dpy, e->keycode, 0, 0);
    bool shift = e->state & ShiftMask;

    if (sym == XK_Return && !shift) {
        spawn(TERMINAL_CMD);
    } else if (sym == XK_r && !shift) {
        spawn(MENU_CMD);
    } else if (sym == XK_q && !shift) {
        client_close(wm.focused);
    } else if (sym == XK_f && !shift) {
        client_toggle_fullscreen(wm.focused);
    } else if (sym == XK_space && !shift) {
        wm.tiling_mode = !wm.tiling_mode;
        for (Client *c = wm.clients; c; c = c->next) {
            if (!c->is_transient) {
                c->is_floating = !wm.tiling_mode;
            }
        }
        arrange();
    } else if (sym == XK_m && !shift) {
        client_minimize(wm.focused);
    } else if (sym == XK_m && shift) {
        client_restore_last();
    } else if (sym == XK_Left && !shift) {
        focus_prev();
    } else if (sym == XK_Right && !shift) {
        focus_next();
    } else if (sym == XK_Up && !shift) {
        client_move(wm.focused, 0, -MOVE_STEP);
    } else if (sym == XK_Down && !shift) {
        client_move(wm.focused, 0, MOVE_STEP);
    } else if (sym == XK_Left && shift) {
        client_resize(wm.focused, -RESIZE_STEP, 0);
    } else if (sym == XK_Right && shift) {
        client_resize(wm.focused, RESIZE_STEP, 0);
    } else if (sym == XK_Up && shift) {
        client_resize(wm.focused, 0, -RESIZE_STEP);
    } else if (sym == XK_Down && shift) {
        client_resize(wm.focused, 0, RESIZE_STEP);
    } else if (sym >= XK_1 && sym <= XK_4) {
        int ws = (int)(sym - XK_1);
        if (shift) {
            move_client_workspace(wm.focused, ws);
        } else {
            set_workspace(ws);
        }
    }
}

static void buttonpress(XButtonEvent *e) {
    if (!(e->state & MOD_MASK)) {
        return;
    }

    Client *c = find_client(e->subwindow ? e->subwindow : e->window);
    if (!c || !is_visible(c)) {
        return;
    }

    set_input_focus(c);
    wm.drag_active = true;
    wm.drag_resize = (e->button == Button3);
    wm.drag_start_x = e->x_root;
    wm.drag_start_y = e->y_root;
    wm.drag_win_x = c->x;
    wm.drag_win_y = c->y;
    wm.drag_win_w = c->w;
    wm.drag_win_h = c->h;
    wm.drag_client = c;
}

static void motionnotify(XMotionEvent *e) {
    if (!wm.drag_active || !wm.drag_client) {
        return;
    }

    Client *c = wm.drag_client;
    int dx = e->x_root - wm.drag_start_x;
    int dy = e->y_root - wm.drag_start_y;

    if (wm.drag_resize) {
        c->w = wm.drag_win_w + dx;
        c->h = wm.drag_win_h + dy;
        if (c->w < 120) c->w = 120;
        if (c->h < 80) c->h = 80;
        XResizeWindow(wm.dpy, c->win, c->w, c->h);
    } else {
        c->x = wm.drag_win_x + dx;
        c->y = wm.drag_win_y + dy;
        XMoveWindow(wm.dpy, c->win, c->x, c->y);
    }
}

static void buttonrelease(XButtonEvent *e) {
    (void)e;
    wm.drag_active = false;
    wm.drag_client = NULL;
}

static void maprequest(XMapRequestEvent *e) {
    Client *c = manage(e->window);
    if (!c) {
        XMapWindow(wm.dpy, e->window);
        return;
    }
    arrange();
}

static void configurerequest(XConfigureRequestEvent *e) {
    XWindowChanges wc;
    wc.x = e->x;
    wc.y = e->y;
    wc.width = e->width;
    wc.height = e->height;
    wc.border_width = e->border_width;
    wc.sibling = e->above;
    wc.stack_mode = e->detail;

    Client *c = find_client(e->window);
    if (c) {
        if (e->value_mask & CWX) c->x = e->x;
        if (e->value_mask & CWY) c->y = e->y;
        if (e->value_mask & CWWidth) c->w = e->width;
        if (e->value_mask & CWHeight) c->h = e->height;
    }

    XConfigureWindow(wm.dpy, e->window, e->value_mask, &wc);
}

static void destroynotify(XDestroyWindowEvent *e) {
    Client *c = find_client(e->window);
    if (c) {
        unmanage(c);
        arrange();
    }
}

static void unmapnotify(XUnmapEvent *e) {
    Client *c = find_client(e->window);
    if (!c) {
        return;
    }

    if (e->send_event) {
        c->is_minimized = true;
    } else {
        unmanage(c);
    }
    arrange();
}

static void enternotify(XCrossingEvent *e) {
    Client *c = find_client(e->window);
    if (c && c != wm.focused && is_visible(c)) {
        if (wm.focused) {
            XSetWindowBorder(wm.dpy, wm.focused->win, wm.border_normal);
        }
        set_input_focus(c);
    }
}

static void clientmessage(XClientMessageEvent *e) {
    if (e->message_type == wm.net_active_window) {
        Client *c = find_client(e->window);
        if (c) {
            set_workspace(c->workspace);
            set_input_focus(c);
        }
        return;
    }

    if (e->message_type == wm.net_current_desktop) {
        set_workspace((int)e->data.l[0]);
    }
}

static void scan_existing_windows(void) {
    Window root_return, parent_return;
    Window *children = NULL;
    unsigned int nchildren = 0;

    if (!XQueryTree(wm.dpy, wm.root, &root_return, &parent_return, &children, &nchildren)) {
        return;
    }

    for (unsigned int i = 0; i < nchildren; i++) {
        XWindowAttributes wa;
        if (!XGetWindowAttributes(wm.dpy, children[i], &wa) || wa.override_redirect || wa.map_state != IsViewable) {
            continue;
        }
        manage(children[i]);
    }

    if (children) {
        XFree(children);
    }
    arrange();
}

static void setup_atoms(void) {
    wm.wm_protocols = XInternAtom(wm.dpy, "WM_PROTOCOLS", False);
    wm.wm_delete = XInternAtom(wm.dpy, "WM_DELETE_WINDOW", False);

    wm.net_active_window = XInternAtom(wm.dpy, "_NET_ACTIVE_WINDOW", False);
    wm.net_wm_state = XInternAtom(wm.dpy, "_NET_WM_STATE", False);
    wm.net_wm_state_fullscreen = XInternAtom(wm.dpy, "_NET_WM_STATE_FULLSCREEN", False);
    wm.net_supported = XInternAtom(wm.dpy, "_NET_SUPPORTED", False);
    wm.net_wm_desktop = XInternAtom(wm.dpy, "_NET_WM_DESKTOP", False);
    wm.net_current_desktop = XInternAtom(wm.dpy, "_NET_CURRENT_DESKTOP", False);
    wm.net_wm_window_type = XInternAtom(wm.dpy, "_NET_WM_WINDOW_TYPE", False);
    wm.net_wm_window_type_dock = XInternAtom(wm.dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);

    Atom supported[] = {
        wm.net_active_window,
        wm.net_wm_state,
        wm.net_wm_state_fullscreen,
        wm.net_wm_desktop,
        wm.net_current_desktop
    };
    XChangeProperty(wm.dpy,
                    wm.root,
                    wm.net_supported,
                    XA_ATOM,
                    32,
                    PropModeReplace,
                    (unsigned char *)supported,
                    (int)(sizeof(supported) / sizeof(supported[0])));

    unsigned long workspaces = WORKSPACE_COUNT;
    Atom net_number_of_desktops = XInternAtom(wm.dpy, "_NET_NUMBER_OF_DESKTOPS", False);
    XChangeProperty(wm.dpy,
                    wm.root,
                    net_number_of_desktops,
                    XA_CARDINAL,
                    32,
                    PropModeReplace,
                    (unsigned char *)&workspaces,
                    1);

    update_current_desktop();
}

void wm_init(void) {
    signal(SIGCHLD, SIG_IGN);

    XSetErrorHandler(xerror);
    wm.dpy = XOpenDisplay(NULL);
    if (!wm.dpy) {
        die("cannot connect to X server");
    }

    wm.screen = DefaultScreen(wm.dpy);
    wm.root = RootWindow(wm.dpy, wm.screen);
    wm.sw = DisplayWidth(wm.dpy, wm.screen);
    wm.sh = DisplayHeight(wm.dpy, wm.screen);
    wm.border_focus = color_from_hex(COLOR_BORDER_FOCUS);
    wm.border_normal = color_from_hex(COLOR_BORDER_NORMAL);
    wm.current_workspace = 0;
    wm.running = true;

    XSelectInput(wm.dpy,
                 wm.root,
                 SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask |
                     ButtonReleaseMask | PointerMotionMask | EnterWindowMask |
                     StructureNotifyMask | PropertyChangeMask | KeyPressMask);

    setup_atoms();
    grab_keys();
    grab_buttons();
    scan_existing_windows();

    XSync(wm.dpy, False);
}

void wm_run(void) {
    XEvent ev;

    while (wm.running && !XNextEvent(wm.dpy, &ev)) {
        switch (ev.type) {
            case KeyPress:
                keypress(&ev.xkey);
                break;
            case ButtonPress:
                buttonpress(&ev.xbutton);
                break;
            case ButtonRelease:
                buttonrelease(&ev.xbutton);
                break;
            case MotionNotify:
                motionnotify(&ev.xmotion);
                break;
            case MapRequest:
                maprequest(&ev.xmaprequest);
                break;
            case ConfigureRequest:
                configurerequest(&ev.xconfigurerequest);
                break;
            case DestroyNotify:
                destroynotify(&ev.xdestroywindow);
                break;
            case UnmapNotify:
                unmapnotify(&ev.xunmap);
                break;
            case EnterNotify:
                enternotify(&ev.xcrossing);
                break;
            case ClientMessage:
                clientmessage(&ev.xclient);
                break;
            default:
                break;
        }
    }
}

void wm_cleanup(void) {
    while (wm.clients) {
        Client *next = wm.clients->next;
        free(wm.clients);
        wm.clients = next;
    }

    if (wm.dpy) {
        XCloseDisplay(wm.dpy);
    }
}
