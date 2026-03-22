#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "wm.h"

WM wm = {0};

typedef enum {
    ACTION_SPAWN_TERMINAL,
    ACTION_SPAWN_MENU,
    ACTION_CLOSE_FOCUSED,
    ACTION_FOCUS_PREV,
    ACTION_FOCUS_NEXT,
    ACTION_MOVE_UP,
    ACTION_MOVE_DOWN,
    ACTION_RESIZE_LEFT,
    ACTION_RESIZE_RIGHT,
    ACTION_RESIZE_UP,
    ACTION_RESIZE_DOWN,
    ACTION_SET_WORKSPACE,
    ACTION_MOVE_TO_WORKSPACE,
    ACTION_TOGGLE_FULLSCREEN,
    ACTION_MINIMIZE,
    ACTION_RESTORE_MINIMIZED,
    ACTION_TOGGLE_FLOATING
} Action;

typedef struct {
    KeySym sym;
    unsigned int mod;
    Action action;
    int arg;
} KeyBinding;

static const KeyBinding keybindings[] = {
    {XK_Return, MOD_MASK, ACTION_SPAWN_TERMINAL, 0},
    {XK_r, MOD_MASK, ACTION_SPAWN_MENU, 0},
    {XK_q, MOD_MASK, ACTION_CLOSE_FOCUSED, 0},
    {XK_Left, MOD_MASK, ACTION_FOCUS_PREV, 0},
    {XK_Right, MOD_MASK, ACTION_FOCUS_NEXT, 0},
    {XK_Up, MOD_MASK, ACTION_MOVE_UP, 0},
    {XK_Down, MOD_MASK, ACTION_MOVE_DOWN, 0},
    {XK_Left, MOD_MASK | ShiftMask, ACTION_RESIZE_LEFT, 0},
    {XK_Right, MOD_MASK | ShiftMask, ACTION_RESIZE_RIGHT, 0},
    {XK_Up, MOD_MASK | ShiftMask, ACTION_RESIZE_UP, 0},
    {XK_Down, MOD_MASK | ShiftMask, ACTION_RESIZE_DOWN, 0},
    {XK_f, MOD_MASK, ACTION_TOGGLE_FULLSCREEN, 0},
    {XK_m, MOD_MASK, ACTION_MINIMIZE, 0},
    {XK_m, MOD_MASK | ShiftMask, ACTION_RESTORE_MINIMIZED, 0},
    {XK_space, MOD_MASK, ACTION_TOGGLE_FLOATING, 0},
    {XK_1, MOD_MASK, ACTION_SET_WORKSPACE, 0},
    {XK_2, MOD_MASK, ACTION_SET_WORKSPACE, 1},
    {XK_3, MOD_MASK, ACTION_SET_WORKSPACE, 2},
    {XK_4, MOD_MASK, ACTION_SET_WORKSPACE, 3},
    {XK_1, MOD_MASK | ShiftMask, ACTION_MOVE_TO_WORKSPACE, 0},
    {XK_2, MOD_MASK | ShiftMask, ACTION_MOVE_TO_WORKSPACE, 1},
    {XK_3, MOD_MASK | ShiftMask, ACTION_MOVE_TO_WORKSPACE, 2},
    {XK_4, MOD_MASK | ShiftMask, ACTION_MOVE_TO_WORKSPACE, 3},
};

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

static bool has_path_separator(const char *cmd) {
    return cmd && strchr(cmd, '/') != NULL;
}

static void trim_leading_spaces(const char **p) {
    while (**p && isspace((unsigned char)**p)) {
        (*p)++;
    }
}

static void trim_trailing_whitespace(char *s) {
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

static bool copy_config_value(char *dest, size_t dest_size, const char *src) {
    if (!dest || dest_size == 0) {
        return false;
    }
    dest[0] = '\0';
    if (!src) {
        return false;
    }

    const char *start = src;
    trim_leading_spaces(&start);
    size_t len = strlen(start);
    while (len > 0 && isspace((unsigned char)start[len - 1])) {
        len--;
    }

    if (len >= 2 && start[0] == '"' && start[len - 1] == '"') {
        start++;
        len -= 2;
    } else if (len > 0 && (start[0] == '"' || start[len - 1] == '"')) {
        return false;
    }

    if (len >= dest_size) {
        len = dest_size - 1;
    }
    memcpy(dest, start, len);
    dest[len] = '\0';
    return true;
}

static bool first_existing_path(char *dest, size_t dest_size, const char *const *candidates) {
    struct stat st = {0};
    for (size_t i = 0; candidates[i]; i++) {
        if (stat(candidates[i], &st) == 0 && S_ISREG(st.st_mode)) {
            snprintf(dest, dest_size, "%s", candidates[i]);
            return true;
        }
    }
    return false;
}

static void resolve_runtime_config_path(char *out, size_t out_size) {
    out[0] = '\0';
    const char *home = getenv("HOME");
    if (home && *home) {
        char user_toml[1024] = {0};
        char user_toml_upper[1024] = {0};
        snprintf(user_toml, sizeof(user_toml), "%s/.config/mimicwm/config.toml", home);
        snprintf(user_toml_upper, sizeof(user_toml_upper), "%s/.config/mimicwm/config.TOML", home);

        const char *user_candidates[] = {user_toml, user_toml_upper, NULL};
        if (first_existing_path(out, out_size, user_candidates)) {
            return;
        }
    }

    const char *system_candidates[] = {"/usr/local/share/mimicwm/config.toml", NULL};
    (void)first_existing_path(out, out_size, system_candidates);
}

static void load_runtime_config(void) {
    RuntimeConfig candidate = wm.runtime_config;
    char path[sizeof(wm.runtime_toml_path)] = {0};
    resolve_runtime_config_path(path, sizeof(path));

    if (!path[0]) {
        wm.runtime_toml_path[0] = '\0';
        wm.runtime_toml_mtime_sec = 0;
        return;
    }

    struct stat st = {0};
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        fprintf(stderr, "mimicwm: config file unavailable: %s\n", path);
        return;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "mimicwm: unable to read config %s: %s\n", path, strerror(errno));
        return;
    }

    bool parse_error = false;
    bool in_commands = false;
    bool in_picom = false;
    char line[2048] = {0};

    while (fgets(line, sizeof(line), fp)) {
        trim_trailing_whitespace(line);
        const char *p = line;
        trim_leading_spaces(&p);
        if (!*p || *p == '#') {
            continue;
        }

        if (*p == '[') {
            in_commands = strncmp(p, "[commands]", 10) == 0;
            in_picom = strncmp(p, "[picom]", 7) == 0;
            continue;
        }

        const char *eq = strchr(p, '=');
        if ((in_commands || in_picom) && !eq) {
            parse_error = true;
            fprintf(stderr, "mimicwm: parse error in %s: expected key=value near '%s'\n", path, p);
            continue;
        }

        if (in_commands && strncmp(p, "terminal", 8) == 0) {
            if (!copy_config_value(candidate.terminal, sizeof(candidate.terminal), eq + 1)) {
                parse_error = true;
                fprintf(stderr, "mimicwm: invalid commands.terminal in %s\n", path);
            }
        } else if (in_commands && strncmp(p, "menu", 4) == 0) {
            if (!copy_config_value(candidate.menu, sizeof(candidate.menu), eq + 1)) {
                parse_error = true;
                fprintf(stderr, "mimicwm: invalid commands.menu in %s\n", path);
            }
        } else if (in_picom && strncmp(p, "backend", 7) == 0) {
            if (!copy_config_value(candidate.picom_backend, sizeof(candidate.picom_backend), eq + 1)) {
                parse_error = true;
                fprintf(stderr, "mimicwm: invalid picom.backend in %s\n", path);
            }
        }
    }

    fclose(fp);

    if (parse_error) {
        fprintf(stderr, "mimicwm: keeping previous runtime config due to parse errors in %s\n", path);
        return;
    }

    wm.runtime_config = candidate;
    snprintf(wm.runtime_toml_path, sizeof(wm.runtime_toml_path), "%s", path);
    wm.runtime_toml_mtime_sec = st.st_mtime;
}

static void reload_runtime_config_if_changed(void) {
    char path[sizeof(wm.runtime_toml_path)] = {0};
    resolve_runtime_config_path(path, sizeof(path));

    if (!path[0]) {
        if (wm.runtime_toml_path[0]) {
            load_runtime_config();
        }
        return;
    }

    struct stat st = {0};
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        if (wm.runtime_toml_path[0]) {
            load_runtime_config();
        }
        return;
    }

    time_t sec = st.st_mtime;

    if (strncmp(path, wm.runtime_toml_path, sizeof(wm.runtime_toml_path)) != 0 ||
        sec != wm.runtime_toml_mtime_sec) {
        load_runtime_config();
    }
}

static bool command_exists(const char *cmd) {
    if (!cmd || !*cmd) {
        return false;
    }

    const char *p = cmd;
    trim_leading_spaces(&p);
    if (!*p) {
        return false;
    }

    size_t len = 0;
    while (p[len] && !isspace((unsigned char)p[len])) {
        len++;
    }
    if (len == 0 || len >= 512) {
        return false;
    }

    char token[512] = {0};
    memcpy(token, p, len);

    if (has_path_separator(token)) {
        return access(token, X_OK) == 0;
    }

    const char *path = getenv("PATH");
    if (!path || !*path) {
        path = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
    }

    const char *seg = path;
    while (*seg) {
        const char *end = strchr(seg, ':');
        size_t dir_len = end ? (size_t)(end - seg) : strlen(seg);

        char full[1024] = {0};
        if (dir_len == 0) {
            snprintf(full, sizeof(full), "%s", token);
        } else {
            snprintf(full, sizeof(full), "%.*s/%s", (int)dir_len, seg, token);
        }

        if (access(full, X_OK) == 0) {
            return true;
        }

        if (!end) {
            break;
        }
        seg = end + 1;
    }

    return false;
}

static void spawn(const char *cmd) {
    if (!command_exists(cmd)) {
        fprintf(stderr, "mimicwm: command not found: %s\n", cmd ? cmd : "(null)");
        return;
    }

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

static void spawn_first_available(const char *preferred, const char *const *fallbacks) {
    if (preferred && command_exists(preferred)) {
        spawn(preferred);
        return;
    }

    for (size_t i = 0; fallbacks[i]; i++) {
        if (command_exists(fallbacks[i])) {
            spawn(fallbacks[i]);
            return;
        }
    }

    fprintf(stderr,
            "mimicwm: no suitable command found (preferred=%s)\n",
            preferred ? preferred : "(null)");
}

static void spawn_terminal(void) {
    static const char *const fallbacks[] = {
        "xterm", "x-terminal-emulator", "alacritty", "kitty", "wezterm", "gnome-terminal", "konsole", "xfce4-terminal", NULL,
    };
    const char *preferred = getenv("MIMICWM_TERMINAL");
    if (!preferred || !*preferred) {
        preferred = wm.runtime_config.terminal[0] ? wm.runtime_config.terminal : TERMINAL_CMD;
    }
    spawn_first_available(preferred, fallbacks);
}

static void spawn_menu(void) {
    static const char *const fallbacks[] = {"dmenu_run", "rofi -show drun", "wofi --show drun", NULL};
    const char *preferred = getenv("MIMICWM_MENU");
    if (!preferred || !*preferred) {
        preferred = wm.runtime_config.menu[0] ? wm.runtime_config.menu : MENU_CMD;
    }
    spawn_first_available(preferred, fallbacks);
}

static WorkspaceState *current_workspace_state(void) {
    return &wm.workspaces[wm.current_workspace];
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

static bool is_scroll_managed(Client *c) {
    return c && !c->is_floating && !c->is_transient;
}

static void workspace_attach_tail(WorkspaceState *ws, Client *c) {
    c->ws_prev = NULL;
    c->ws_next = NULL;

    if (!ws->scroll_head) {
        ws->scroll_head = c;
        return;
    }

    Client *tail = ws->scroll_head;
    while (tail->ws_next) {
        tail = tail->ws_next;
    }
    tail->ws_next = c;
    c->ws_prev = tail;
}

static void workspace_detach(Client *c) {
    WorkspaceState *ws = &wm.workspaces[c->workspace];

    if (ws->scroll_head == c) {
        ws->scroll_head = c->ws_next;
    }
    if (ws->scroll_focus == c) {
        ws->scroll_focus = c->ws_next ? c->ws_next : c->ws_prev;
    }
    if (ws->min_restore_cursor == c) {
        ws->min_restore_cursor = NULL;
    }

    if (c->ws_prev) {
        c->ws_prev->ws_next = c->ws_next;
    }
    if (c->ws_next) {
        c->ws_next->ws_prev = c->ws_prev;
    }

    c->ws_prev = NULL;
    c->ws_next = NULL;
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

    if (wm.focused && wm.focused != c) {
        XSetWindowBorder(wm.dpy, wm.focused->win, wm.border_normal);
    }

    wm.focused = c;
    XSetWindowBorder(wm.dpy, c->win, wm.border_focus);
    XSetInputFocus(wm.dpy, c->win, RevertToPointerRoot, CurrentTime);
    XRaiseWindow(wm.dpy, c->win);

    WorkspaceState *ws = &wm.workspaces[c->workspace];
    if (is_scroll_managed(c)) {
        ws->scroll_focus = c;
    }

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

static Client *workspace_find_first_focusable(WorkspaceState *ws) {
    for (Client *c = ws->scroll_head; c; c = c->ws_next) {
        if (is_visible(c) && is_scroll_managed(c) && !c->is_fullscreen) {
            return c;
        }
    }
    return NULL;
}

static void focus_scroll_relative(int direction) {
    WorkspaceState *ws = current_workspace_state();
    Client *start = ws->scroll_focus;

    if (!start || !is_visible(start) || !is_scroll_managed(start) || start->is_fullscreen) {
        start = workspace_find_first_focusable(ws);
    }
    if (!start) {
        return;
    }

    Client *c = start;
    for (;;) {
        c = (direction > 0) ? c->ws_next : c->ws_prev;
        if (!c) {
            c = (direction > 0) ? ws->scroll_head : NULL;
            if (direction < 0) {
                c = ws->scroll_head;
                if (!c) {
                    break;
                }
                while (c->ws_next) {
                    c = c->ws_next;
                }
            }
        }

        if (!c || c == start) {
            break;
        }

        if (is_visible(c) && is_scroll_managed(c) && !c->is_fullscreen) {
            set_input_focus(c);
            return;
        }
    }

    set_input_focus(start);
}

static void update_visibility(void) {
    for (Client *c = wm.clients; c; c = c->next) {
        if (is_visible(c)) {
            XMapWindow(wm.dpy, c->win);
        } else {
            XUnmapWindow(wm.dpy, c->win);
        }
    }
}

static Client *workspace_fullscreen_client(WorkspaceState *ws) {
    for (Client *c = ws->scroll_head; c; c = c->ws_next) {
        if (is_visible(c) && c->is_fullscreen) {
            return c;
        }
    }

    for (Client *c = wm.clients; c; c = c->next) {
        if (is_visible(c) && c->is_floating && c->is_fullscreen) {
            return c;
        }
    }
    return NULL;
}

static void arrange_scroll_workspace(WorkspaceState *ws) {
    Client *focused = ws->scroll_focus;
    if (!focused || !is_visible(focused) || !is_scroll_managed(focused) || focused->is_fullscreen) {
        focused = workspace_find_first_focusable(ws);
        ws->scroll_focus = focused;
    }

    if (!focused) {
        return;
    }

    const int gap = 24;
    const int ypad = 24;
    const int main_w = (int)(wm.sw * 0.78);
    const int side_w = (int)(wm.sw * 0.58);
    const int main_h = (int)wm.sh - (2 * ypad) - 2 * BORDER_WIDTH;
    const int side_h = (int)wm.sh - (2 * ypad) - 2 * BORDER_WIDTH;
    const int center_x = ((int)wm.sw - main_w) / 2;

    focused->x = center_x;
    focused->y = ypad;
    focused->w = main_w - 2 * BORDER_WIDTH;
    focused->h = main_h;
    XMoveResizeWindow(wm.dpy, focused->win, focused->x, focused->y, focused->w, focused->h);

    int left_x = center_x - gap - side_w;
    for (Client *c = focused->ws_prev; c; c = c->ws_prev) {
        if (!is_visible(c) || !is_scroll_managed(c) || c->is_fullscreen) {
            continue;
        }
        c->x = left_x;
        c->y = ypad;
        c->w = side_w - 2 * BORDER_WIDTH;
        c->h = side_h;
        XMoveResizeWindow(wm.dpy, c->win, c->x, c->y, c->w, c->h);
        left_x -= side_w + gap;
    }

    int right_x = center_x + main_w + gap;
    for (Client *c = focused->ws_next; c; c = c->ws_next) {
        if (!is_visible(c) || !is_scroll_managed(c) || c->is_fullscreen) {
            continue;
        }
        c->x = right_x;
        c->y = ypad;
        c->w = side_w - 2 * BORDER_WIDTH;
        c->h = side_h;
        XMoveResizeWindow(wm.dpy, c->win, c->x, c->y, c->w, c->h);
        right_x += side_w + gap;
    }
}

static void arrange(void) {
    update_visibility();

    WorkspaceState *ws = current_workspace_state();
    Client *fullscreen = workspace_fullscreen_client(ws);
    if (fullscreen) {
        fullscreen->x = 0;
        fullscreen->y = 0;
        fullscreen->w = (int)wm.sw;
        fullscreen->h = (int)wm.sh;
        XMoveResizeWindow(wm.dpy, fullscreen->win, fullscreen->x, fullscreen->y, fullscreen->w, fullscreen->h);
        XRaiseWindow(wm.dpy, fullscreen->win);
        set_input_focus(fullscreen);
    } else {
        arrange_scroll_workspace(ws);

        for (Client *c = wm.clients; c; c = c->next) {
            if (!is_visible(c) || !c->is_floating || c->is_minimized || c->is_fullscreen) {
                continue;
            }
            XMoveResizeWindow(wm.dpy, c->win, c->x, c->y, c->w, c->h);
            XRaiseWindow(wm.dpy, c->win);
        }

        if (!wm.focused || !is_visible(wm.focused)) {
            if (ws->scroll_focus && is_visible(ws->scroll_focus)) {
                set_input_focus(ws->scroll_focus);
            } else {
                for (Client *c = wm.clients; c; c = c->next) {
                    if (is_visible(c)) {
                        set_input_focus(c);
                        break;
                    }
                }
            }
        }
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

    WorkspaceState *ws = current_workspace_state();
    if (!c->is_floating) {
        workspace_attach_tail(ws, c);
        if (!ws->scroll_focus) {
            ws->scroll_focus = c;
        }
    }

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

    if (!c->is_floating) {
        workspace_detach(c);
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

    arrange();
}

static bool can_floating_move_resize(Client *c) {
    if (!c || c->is_fullscreen) {
        return false;
    }
    return c->is_floating || c->is_transient;
}

static void client_move(Client *c, int dx, int dy) {
    if (!can_floating_move_resize(c)) {
        return;
    }
    c->x += dx;
    c->y += dy;
    XMoveWindow(wm.dpy, c->win, c->x, c->y);
}

static void client_resize(Client *c, int dw, int dh) {
    if (!can_floating_move_resize(c)) {
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
    if (wm.focused == c) {
        wm.focused = NULL;
    }
    arrange();
}

static void client_restore_last(void) {
    WorkspaceState *ws = current_workspace_state();
    Client *start = ws->min_restore_cursor ? ws->min_restore_cursor : wm.clients;

    Client *candidate = NULL;
    for (Client *c = start; c; c = c->next) {
        if (c->workspace == wm.current_workspace && c->is_minimized) {
            candidate = c;
            break;
        }
    }
    if (!candidate) {
        for (Client *c = wm.clients; c && c != start; c = c->next) {
            if (c->workspace == wm.current_workspace && c->is_minimized) {
                candidate = c;
                break;
            }
        }
    }

    if (!candidate) {
        return;
    }

    candidate->is_minimized = false;
    ws->min_restore_cursor = candidate->next;
    XMapRaised(wm.dpy, candidate->win);
    set_input_focus(candidate);
    arrange();
}

static void set_workspace(int ws) {
    if (ws < 0 || ws >= WORKSPACE_COUNT || ws == wm.current_workspace) {
        return;
    }
    wm.current_workspace = ws;
    update_current_desktop();
    wm.focused = NULL;
    arrange();
}

static void move_client_workspace(Client *c, int ws) {
    if (!c || ws < 0 || ws >= WORKSPACE_COUNT) {
        return;
    }

    if (!c->is_floating) {
        workspace_detach(c);
    }

    c->workspace = ws;
    c->is_minimized = false;

    if (!c->is_floating) {
        WorkspaceState *target = &wm.workspaces[ws];
        workspace_attach_tail(target, c);
        if (!target->scroll_focus) {
            target->scroll_focus = c;
        }
    }

    set_client_desktop(c);
    if (ws != wm.current_workspace) {
        XUnmapWindow(wm.dpy, c->win);
        if (wm.focused == c) {
            wm.focused = NULL;
        }
    }
    arrange();
}

static unsigned int clean_mod_mask(unsigned int state) {
    return state & ~(LockMask | wm.numlock_mask);
}

static void detect_numlock_mask(void) {
    wm.numlock_mask = 0;
    XModifierKeymap *modmap = XGetModifierMapping(wm.dpy);
    if (!modmap) {
        return;
    }

    KeyCode numlock = XKeysymToKeycode(wm.dpy, XK_Num_Lock);
    for (int mod = 0; mod < 8; mod++) {
        for (int k = 0; k < modmap->max_keypermod; k++) {
            KeyCode code = modmap->modifiermap[mod * modmap->max_keypermod + k];
            if (code == numlock) {
                wm.numlock_mask = (1u << mod);
            }
        }
    }

    XFreeModifiermap(modmap);
}

static void grab_keys(void) {
    XUngrabKey(wm.dpy, AnyKey, AnyModifier, wm.root);

    unsigned int modifiers[] = {0, LockMask, wm.numlock_mask, LockMask | wm.numlock_mask};

    for (size_t i = 0; i < sizeof(keybindings) / sizeof(keybindings[0]); i++) {
        KeyCode code = XKeysymToKeycode(wm.dpy, keybindings[i].sym);
        if (!code) {
            continue;
        }
        for (size_t m = 0; m < sizeof(modifiers) / sizeof(modifiers[0]); m++) {
            XGrabKey(wm.dpy,
                     code,
                     keybindings[i].mod | modifiers[m],
                     wm.root,
                     True,
                     GrabModeAsync,
                     GrabModeAsync);
        }
    }
}

static void grab_buttons(void) {
    XUngrabButton(wm.dpy, AnyButton, AnyModifier, wm.root);

    unsigned int mods[] = {MOD_MASK, MOD_MASK | LockMask, MOD_MASK | wm.numlock_mask, MOD_MASK | LockMask | wm.numlock_mask};
    for (size_t i = 0; i < sizeof(mods) / sizeof(mods[0]); i++) {
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

static void dispatch_action(Action action, int arg) {
    switch (action) {
        case ACTION_SPAWN_TERMINAL:
            spawn_terminal();
            break;
        case ACTION_SPAWN_MENU:
            spawn_menu();
            break;
        case ACTION_CLOSE_FOCUSED:
            client_close(wm.focused);
            break;
        case ACTION_FOCUS_PREV:
            focus_scroll_relative(-1);
            arrange();
            break;
        case ACTION_FOCUS_NEXT:
            focus_scroll_relative(1);
            arrange();
            break;
        case ACTION_MOVE_UP:
            client_move(wm.focused, 0, -MOVE_STEP);
            break;
        case ACTION_MOVE_DOWN:
            client_move(wm.focused, 0, MOVE_STEP);
            break;
        case ACTION_RESIZE_LEFT:
            client_resize(wm.focused, -RESIZE_STEP, 0);
            break;
        case ACTION_RESIZE_RIGHT:
            client_resize(wm.focused, RESIZE_STEP, 0);
            break;
        case ACTION_RESIZE_UP:
            client_resize(wm.focused, 0, -RESIZE_STEP);
            break;
        case ACTION_RESIZE_DOWN:
            client_resize(wm.focused, 0, RESIZE_STEP);
            break;
        case ACTION_SET_WORKSPACE:
            set_workspace(arg);
            break;
        case ACTION_MOVE_TO_WORKSPACE:
            move_client_workspace(wm.focused, arg);
            break;
        case ACTION_TOGGLE_FULLSCREEN:
            client_toggle_fullscreen(wm.focused);
            break;
        case ACTION_MINIMIZE:
            client_minimize(wm.focused);
            break;
        case ACTION_RESTORE_MINIMIZED:
            client_restore_last();
            break;
        case ACTION_TOGGLE_FLOATING:
            if (wm.focused && !wm.focused->is_transient) {
                if (wm.focused->is_floating) {
                    wm.focused->is_floating = false;
                    workspace_attach_tail(current_workspace_state(), wm.focused);
                    current_workspace_state()->scroll_focus = wm.focused;
                } else {
                    workspace_detach(wm.focused);
                    wm.focused->is_floating = true;
                }
                arrange();
            }
            break;
    }
}

static void keypress(XKeyEvent *e) {
    reload_runtime_config_if_changed();

    KeySym sym = XLookupKeysym(e, 0);
    unsigned int clean = clean_mod_mask(e->state);

    for (size_t i = 0; i < sizeof(keybindings) / sizeof(keybindings[0]); i++) {
        if (keybindings[i].sym == sym && keybindings[i].mod == clean) {
            dispatch_action(keybindings[i].action, keybindings[i].arg);
            return;
        }
    }
}

static void buttonpress(XButtonEvent *e) {
    if (clean_mod_mask(e->state) != MOD_MASK) {
        return;
    }

    Client *c = find_client(e->subwindow ? e->subwindow : e->window);
    if (!c || !is_visible(c) || !can_floating_move_resize(c)) {
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

    XGrabPointer(wm.dpy,
                 wm.root,
                 False,
                 PointerMotionMask | ButtonReleaseMask,
                 GrabModeAsync,
                 GrabModeAsync,
                 None,
                 None,
                 CurrentTime);
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
    XUngrabPointer(wm.dpy, CurrentTime);
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
        if (can_floating_move_resize(c)) {
            if (e->value_mask & CWX) c->x = e->x;
            if (e->value_mask & CWY) c->y = e->y;
            if (e->value_mask & CWWidth) c->w = e->width;
            if (e->value_mask & CWHeight) c->h = e->height;
            XConfigureWindow(wm.dpy, e->window, e->value_mask, &wc);
        } else {
            arrange();
        }
        return;
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
        set_input_focus(c);
        if (is_scroll_managed(c)) {
            arrange();
        }
    }
}

static void clientmessage(XClientMessageEvent *e) {
    if (e->message_type == wm.net_active_window) {
        long source = e->data.l[0];
        if (source != 2) {
            return;
        }

        Client *c = find_client(e->window);
        if (c) {
            set_workspace(c->workspace);
            set_input_focus(c);
            if (is_scroll_managed(c)) {
                arrange();
            }
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

    Atom supported[] = {wm.net_active_window,
                        wm.net_wm_state,
                        wm.net_wm_state_fullscreen,
                        wm.net_wm_desktop,
                        wm.net_current_desktop};
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
    detect_numlock_mask();
    load_runtime_config();
    grab_keys();
    grab_buttons();
    scan_existing_windows();

    XSync(wm.dpy, False);
}

void wm_run(void) {
    XEvent ev;

    while (wm.running && !XNextEvent(wm.dpy, &ev)) {
        reload_runtime_config_if_changed();

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
