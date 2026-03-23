#include <X11/keysym.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xcb/xcb.h>

#include "wm.h"

WM wm = {0};

typedef uint32_t xcb_keysym_t;


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
    xcb_keysym_t sym;
    uint16_t mod;
    Action action;
    int arg;
    xcb_keycode_t keycode;
} KeyBinding;

static KeyBinding keybindings[] = {
    {XK_Return, MOD_MASK, ACTION_SPAWN_TERMINAL, 0, 0},
    {XK_r, MOD_MASK, ACTION_SPAWN_MENU, 0, 0},
    {XK_q, MOD_MASK, ACTION_CLOSE_FOCUSED, 0, 0},
    {XK_Left, MOD_MASK, ACTION_FOCUS_PREV, 0, 0},
    {XK_Right, MOD_MASK, ACTION_FOCUS_NEXT, 0, 0},
    {XK_Up, MOD_MASK, ACTION_MOVE_UP, 0, 0},
    {XK_Down, MOD_MASK, ACTION_MOVE_DOWN, 0, 0},
    {XK_Left, MOD_MASK | XCB_MOD_MASK_SHIFT, ACTION_RESIZE_LEFT, 0, 0},
    {XK_Right, MOD_MASK | XCB_MOD_MASK_SHIFT, ACTION_RESIZE_RIGHT, 0, 0},
    {XK_Up, MOD_MASK | XCB_MOD_MASK_SHIFT, ACTION_RESIZE_UP, 0, 0},
    {XK_Down, MOD_MASK | XCB_MOD_MASK_SHIFT, ACTION_RESIZE_DOWN, 0, 0},
    {XK_f, MOD_MASK, ACTION_TOGGLE_FULLSCREEN, 0, 0},
    {XK_m, MOD_MASK, ACTION_MINIMIZE, 0, 0},
    {XK_m, MOD_MASK | XCB_MOD_MASK_SHIFT, ACTION_RESTORE_MINIMIZED, 0, 0},
    {XK_space, MOD_MASK, ACTION_TOGGLE_FLOATING, 0, 0},
    {XK_1, MOD_MASK, ACTION_SET_WORKSPACE, 0, 0},
    {XK_2, MOD_MASK, ACTION_SET_WORKSPACE, 1, 0},
    {XK_3, MOD_MASK, ACTION_SET_WORKSPACE, 2, 0},
    {XK_4, MOD_MASK, ACTION_SET_WORKSPACE, 3, 0},
    {XK_1, MOD_MASK | XCB_MOD_MASK_SHIFT, ACTION_MOVE_TO_WORKSPACE, 0, 0},
    {XK_2, MOD_MASK | XCB_MOD_MASK_SHIFT, ACTION_MOVE_TO_WORKSPACE, 1, 0},
    {XK_3, MOD_MASK | XCB_MOD_MASK_SHIFT, ACTION_MOVE_TO_WORKSPACE, 2, 0},
    {XK_4, MOD_MASK | XCB_MOD_MASK_SHIFT, ACTION_MOVE_TO_WORKSPACE, 3, 0},
};

static xcb_atom_t net_number_of_desktops = XCB_ATOM_NONE;
static xcb_atom_t wm_transient_for = XCB_ATOM_NONE;
static xcb_atom_t wm_hints = XCB_ATOM_NONE;
static xcb_atom_t wm_state = XCB_ATOM_NONE;

static xcb_keysym_t *keysyms = NULL;
static uint8_t keycode_min = 0;
static uint8_t keycode_max = 0;
static int keysyms_per_keycode = 0;

static void die(const char *msg) {
    fprintf(stderr, "mimicwm: %s\n", msg);
    exit(EXIT_FAILURE);
}

static bool xcb_check_ok(xcb_void_cookie_t ck) {
    xcb_generic_error_t *err = xcb_request_check(wm.dpy, ck);
    if (!err) return true;
    if (err->error_code == XCB_ACCESS || err->error_code == XCB_WINDOW) {
        free(err);
        return false;
    }
    fprintf(stderr, "mimicwm: xcb error code=%u major=%u minor=%u\n", err->error_code, err->major_code, err->minor_code);
    free(err);
    return false;
}

static xcb_atom_t atom(const char *name) {
    xcb_intern_atom_cookie_t ck = xcb_intern_atom(wm.dpy, 0, (uint16_t)strlen(name), name);
    xcb_intern_atom_reply_t *rp = xcb_intern_atom_reply(wm.dpy, ck, NULL);
    if (!rp) return XCB_ATOM_NONE;
    xcb_atom_t a = rp->atom;
    free(rp);
    return a;
}

static uint32_t color_from_hex(uint32_t rgb) {
    uint16_t red = (uint16_t)(((rgb >> 16) & 0xff) * 257);
    uint16_t green = (uint16_t)(((rgb >> 8) & 0xff) * 257);
    uint16_t blue = (uint16_t)((rgb & 0xff) * 257);
    xcb_alloc_color_cookie_t ck = xcb_alloc_color(wm.dpy, wm.screen->default_colormap, red, green, blue);
    xcb_alloc_color_reply_t *rp = xcb_alloc_color_reply(wm.dpy, ck, NULL);
    if (!rp) return wm.screen->black_pixel;
    uint32_t pixel = rp->pixel;
    free(rp);
    return pixel;
}

static void set_property32(xcb_window_t win, xcb_atom_t prop, xcb_atom_t type, const uint32_t *vals, uint32_t n) {
    xcb_change_property(wm.dpy, XCB_PROP_MODE_REPLACE, win, prop, type, 32, n, vals);
}

static void delete_property(xcb_window_t win, xcb_atom_t prop) {
    xcb_delete_property(wm.dpy, win, prop);
}

static bool has_path_separator(const char *cmd) { return cmd && strchr(cmd, '/') != NULL; }
static void trim_leading_spaces(const char **p) { while (**p && isspace((unsigned char)**p)) (*p)++; }
static void trim_trailing_whitespace(char *s) { size_t len = strlen(s); while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0'; }

static bool copy_config_value(char *dest, size_t dest_size, const char *src) {
    if (!dest || dest_size == 0) return false;
    dest[0] = '\0';
    if (!src) return false;
    const char *start = src;
    trim_leading_spaces(&start);
    size_t len = strlen(start);
    while (len > 0 && isspace((unsigned char)start[len - 1])) len--;
    if (len >= 2 && start[0] == '"' && start[len - 1] == '"') {
        start++;
        len -= 2;
    } else if (len > 0 && (start[0] == '"' || start[len - 1] == '"')) {
        return false;
    }
    if (len >= dest_size) len = dest_size - 1;
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
        if (first_existing_path(out, out_size, user_candidates)) return;
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
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return;

    FILE *fp = fopen(path, "r");
    if (!fp) return;

    bool parse_error = false;
    bool in_commands = false;
    bool in_picom = false;
    char line[2048] = {0};
    while (fgets(line, sizeof(line), fp)) {
        trim_trailing_whitespace(line);
        const char *p = line;
        trim_leading_spaces(&p);
        if (!*p || *p == '#') continue;
        if (*p == '[') {
            in_commands = strncmp(p, "[commands]", 10) == 0;
            in_picom = strncmp(p, "[picom]", 7) == 0;
            continue;
        }
        const char *eq = strchr(p, '=');
        if ((in_commands || in_picom) && !eq) {
            parse_error = true;
            continue;
        }
        if (in_commands && strncmp(p, "terminal", 8) == 0) {
            if (!copy_config_value(candidate.terminal, sizeof(candidate.terminal), eq + 1)) parse_error = true;
        } else if (in_commands && strncmp(p, "menu", 4) == 0) {
            if (!copy_config_value(candidate.menu, sizeof(candidate.menu), eq + 1)) parse_error = true;
        } else if (in_picom && strncmp(p, "backend", 7) == 0) {
            if (!copy_config_value(candidate.picom_backend, sizeof(candidate.picom_backend), eq + 1)) parse_error = true;
        }
    }
    fclose(fp);
    if (parse_error) return;
    wm.runtime_config = candidate;
    snprintf(wm.runtime_toml_path, sizeof(wm.runtime_toml_path), "%s", path);
    wm.runtime_toml_mtime_sec = st.st_mtime;
}

static void reload_runtime_config_if_changed(void) {
    char path[sizeof(wm.runtime_toml_path)] = {0};
    resolve_runtime_config_path(path, sizeof(path));
    if (!path[0]) {
        if (wm.runtime_toml_path[0]) load_runtime_config();
        return;
    }
    struct stat st = {0};
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        if (wm.runtime_toml_path[0]) load_runtime_config();
        return;
    }
    if (strncmp(path, wm.runtime_toml_path, sizeof(wm.runtime_toml_path)) != 0 || st.st_mtime != wm.runtime_toml_mtime_sec) load_runtime_config();
}

static bool command_exists(const char *cmd) {
    if (!cmd || !*cmd) return false;
    const char *p = cmd;
    trim_leading_spaces(&p);
    if (!*p) return false;
    size_t len = 0;
    while (p[len] && !isspace((unsigned char)p[len])) len++;
    if (len == 0 || len >= 512) return false;
    char token[512] = {0};
    memcpy(token, p, len);
    if (has_path_separator(token)) return access(token, X_OK) == 0;
    const char *path = getenv("PATH");
    if (!path || !*path) path = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
    for (const char *seg = path; *seg;) {
        const char *end = strchr(seg, ':');
        size_t dir_len = end ? (size_t)(end - seg) : strlen(seg);
        char full[1024] = {0};
        if (dir_len == 0) snprintf(full, sizeof(full), "%s", token);
        else snprintf(full, sizeof(full), "%.*s/%s", (int)dir_len, seg, token);
        if (access(full, X_OK) == 0) return true;
        if (!end) break;
        seg = end + 1;
    }
    return false;
}

static void spawn(const char *cmd) {
    if (!command_exists(cmd)) return;
    pid_t pid = fork();
    if (pid == 0) {
        if (wm.dpy) close(xcb_get_file_descriptor(wm.dpy));
        setsid();
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
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
}

static void spawn_terminal(void) {
    static const char *const fallbacks[] = {"xterm", "x-terminal-emulator", "alacritty", "kitty", "wezterm", "gnome-terminal", "konsole", "xfce4-terminal", NULL};
    const char *preferred = getenv("MIMICWM_TERMINAL");
    if (!preferred || !*preferred) preferred = wm.runtime_config.terminal[0] ? wm.runtime_config.terminal : TERMINAL_CMD;
    spawn_first_available(preferred, fallbacks);
}

static void spawn_menu(void) {
    static const char *const fallbacks[] = {"dmenu_run", "rofi -show drun", "wofi --show drun", NULL};
    const char *preferred = getenv("MIMICWM_MENU");
    if (!preferred || !*preferred) preferred = wm.runtime_config.menu[0] ? wm.runtime_config.menu : MENU_CMD;
    spawn_first_available(preferred, fallbacks);
}

static WorkspaceState *current_workspace_state(void) { return &wm.workspaces[wm.current_workspace]; }
static Client *find_client(xcb_window_t w) { for (Client *c = wm.clients; c; c = c->next) if (c->win == w) return c; return NULL; }
static bool is_visible(Client *c) { return c && !c->is_minimized && c->workspace == wm.current_workspace; }
static bool is_scroll_managed(Client *c) { return c && !c->is_floating && !c->is_transient; }

static void workspace_attach_tail(WorkspaceState *ws, Client *c) {
    c->ws_prev = NULL; c->ws_next = NULL;
    if (!ws->scroll_head) { ws->scroll_head = c; return; }
    Client *tail = ws->scroll_head;
    while (tail->ws_next) tail = tail->ws_next;
    tail->ws_next = c; c->ws_prev = tail;
}

static void workspace_detach(Client *c) {
    WorkspaceState *ws = &wm.workspaces[c->workspace];
    if (ws->scroll_head == c) ws->scroll_head = c->ws_next;
    if (ws->scroll_focus == c) ws->scroll_focus = c->ws_next ? c->ws_next : c->ws_prev;
    if (ws->min_restore_cursor == c) ws->min_restore_cursor = NULL;
    if (c->ws_prev) c->ws_prev->ws_next = c->ws_next;
    if (c->ws_next) c->ws_next->ws_prev = c->ws_prev;
    c->ws_prev = NULL; c->ws_next = NULL;
}

static void update_current_desktop(void) { uint32_t v = (uint32_t)wm.current_workspace; set_property32(wm.root, wm.net_current_desktop, XCB_ATOM_CARDINAL, &v, 1); }
static void set_client_desktop(Client *c) { uint32_t v = (uint32_t)c->workspace; set_property32(c->win, wm.net_wm_desktop, XCB_ATOM_CARDINAL, &v, 1); }

static void set_border(xcb_window_t win, uint32_t color) {
    uint32_t vals[] = {color};
    xcb_configure_window(wm.dpy, win, XCB_CONFIG_WINDOW_BORDER_WIDTH, (uint32_t[]){BORDER_WIDTH});
    xcb_change_window_attributes(wm.dpy, win, XCB_CW_BORDER_PIXEL, vals);
}

static void raise_window(xcb_window_t win) { xcb_configure_window(wm.dpy, win, XCB_CONFIG_WINDOW_STACK_MODE, (uint32_t[]){XCB_STACK_MODE_ABOVE}); }

static void set_input_focus(Client *c) {
    if (!c || !is_visible(c)) {
        xcb_set_input_focus(wm.dpy, XCB_INPUT_FOCUS_POINTER_ROOT, wm.root, XCB_CURRENT_TIME);
        wm.focused = NULL;
        delete_property(wm.root, wm.net_active_window);
        return;
    }
    if (wm.focused && wm.focused != c) set_border(wm.focused->win, wm.border_normal);
    wm.focused = c;
    set_border(c->win, wm.border_focus);
    xcb_set_input_focus(wm.dpy, XCB_INPUT_FOCUS_POINTER_ROOT, c->win, XCB_CURRENT_TIME);
    raise_window(c->win);
    WorkspaceState *ws = &wm.workspaces[c->workspace];
    if (is_scroll_managed(c)) ws->scroll_focus = c;
    uint32_t active = c->win;
    set_property32(wm.root, wm.net_active_window, XCB_ATOM_WINDOW, &active, 1);
}

static Client *workspace_find_first_focusable(WorkspaceState *ws) {
    for (Client *c = ws->scroll_head; c; c = c->ws_next) if (is_visible(c) && is_scroll_managed(c) && !c->is_fullscreen) return c;
    return NULL;
}

static void focus_scroll_relative(int direction) {
    WorkspaceState *ws = current_workspace_state();
    Client *start = ws->scroll_focus;
    if (!start || !is_visible(start) || !is_scroll_managed(start) || start->is_fullscreen) start = workspace_find_first_focusable(ws);
    if (!start) return;
    Client *c = start;
    for (;;) {
        c = (direction > 0) ? c->ws_next : c->ws_prev;
        if (!c) {
            c = (direction > 0) ? ws->scroll_head : ws->scroll_head;
            if (direction < 0 && c) while (c->ws_next) c = c->ws_next;
        }
        if (!c || c == start) break;
        if (is_visible(c) && is_scroll_managed(c) && !c->is_fullscreen) { set_input_focus(c); return; }
    }
    set_input_focus(start);
}

static void update_visibility(void) {
    for (Client *c = wm.clients; c; c = c->next) {
        if (is_visible(c)) xcb_map_window(wm.dpy, c->win);
        else xcb_unmap_window(wm.dpy, c->win);
    }
}

static void move_resize(xcb_window_t win, int x, int y, int w, int h) {
    uint32_t vals[] = {(uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h};
    xcb_configure_window(wm.dpy, win, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, vals);
}

static Client *workspace_fullscreen_client(WorkspaceState *ws) {
    for (Client *c = ws->scroll_head; c; c = c->ws_next) if (is_visible(c) && c->is_fullscreen) return c;
    for (Client *c = wm.clients; c; c = c->next) if (is_visible(c) && c->is_floating && c->is_fullscreen) return c;
    return NULL;
}

static void arrange_scroll_workspace(WorkspaceState *ws) {
    Client *focused = ws->scroll_focus;
    if (!focused || !is_visible(focused) || !is_scroll_managed(focused) || focused->is_fullscreen) { focused = workspace_find_first_focusable(ws); ws->scroll_focus = focused; }
    if (!focused) return;
    const int gap = 24, ypad = 24;
    const int main_w = (int)(wm.sw * 0.78), side_w = (int)(wm.sw * 0.58);
    const int main_h = (int)wm.sh - (2 * ypad) - 2 * BORDER_WIDTH;
    const int side_h = (int)wm.sh - (2 * ypad) - 2 * BORDER_WIDTH;
    const int center_x = ((int)wm.sw - main_w) / 2;
    focused->x = center_x; focused->y = ypad; focused->w = main_w - 2 * BORDER_WIDTH; focused->h = main_h;
    move_resize(focused->win, focused->x, focused->y, focused->w, focused->h);
    int left_x = center_x - gap - side_w;
    for (Client *c = focused->ws_prev; c; c = c->ws_prev) {
        if (!is_visible(c) || !is_scroll_managed(c) || c->is_fullscreen) continue;
        c->x = left_x; c->y = ypad; c->w = side_w - 2 * BORDER_WIDTH; c->h = side_h;
        move_resize(c->win, c->x, c->y, c->w, c->h);
        left_x -= side_w + gap;
    }
    int right_x = center_x + main_w + gap;
    for (Client *c = focused->ws_next; c; c = c->ws_next) {
        if (!is_visible(c) || !is_scroll_managed(c) || c->is_fullscreen) continue;
        c->x = right_x; c->y = ypad; c->w = side_w - 2 * BORDER_WIDTH; c->h = side_h;
        move_resize(c->win, c->x, c->y, c->w, c->h);
        right_x += side_w + gap;
    }
}

static void arrange(void) {
    update_visibility();
    WorkspaceState *ws = current_workspace_state();
    Client *fullscreen = workspace_fullscreen_client(ws);
    if (fullscreen) {
        fullscreen->x = 0; fullscreen->y = 0; fullscreen->w = (int)wm.sw; fullscreen->h = (int)wm.sh;
        move_resize(fullscreen->win, fullscreen->x, fullscreen->y, fullscreen->w, fullscreen->h);
        raise_window(fullscreen->win);
        set_input_focus(fullscreen);
    } else {
        arrange_scroll_workspace(ws);
        for (Client *c = wm.clients; c; c = c->next) {
            if (!is_visible(c) || !c->is_floating || c->is_minimized || c->is_fullscreen) continue;
            move_resize(c->win, c->x, c->y, c->w, c->h);
            raise_window(c->win);
        }
        if (!wm.focused || !is_visible(wm.focused)) {
            if (ws->scroll_focus && is_visible(ws->scroll_focus)) set_input_focus(ws->scroll_focus);
            else for (Client *c = wm.clients; c; c = c->next) if (is_visible(c)) { set_input_focus(c); break; }
        }
    }
    xcb_flush(wm.dpy);
}

static bool is_dock_window(xcb_window_t w) {
    xcb_get_property_cookie_t ck = xcb_get_property(wm.dpy, 0, w, wm.net_wm_window_type, XCB_ATOM_ATOM, 0, 8);
    xcb_get_property_reply_t *rp = xcb_get_property_reply(wm.dpy, ck, NULL);
    if (!rp) return false;
    bool dock = false;
    int len = xcb_get_property_value_length(rp) / (int)sizeof(xcb_atom_t);
    xcb_atom_t *vals = (xcb_atom_t *)xcb_get_property_value(rp);
    for (int i = 0; i < len; i++) if (vals[i] == wm.net_wm_window_type_dock) { dock = true; break; }
    free(rp);
    return dock;
}

static bool window_is_unmapped(xcb_window_t w, bool *override_redirect) {
    xcb_get_window_attributes_reply_t *ar = xcb_get_window_attributes_reply(wm.dpy, xcb_get_window_attributes(wm.dpy, w), NULL);
    if (!ar) return true;
    if (override_redirect) *override_redirect = ar->override_redirect;
    bool unmapped = ar->map_state == XCB_MAP_STATE_UNMAPPED;
    free(ar);
    return unmapped;
}

static bool is_iconic(xcb_window_t w) {
    xcb_get_property_cookie_t ck = xcb_get_property(wm.dpy, 0, w, wm_hints, wm_hints, 0, 9);
    xcb_get_property_reply_t *rp = xcb_get_property_reply(wm.dpy, ck, NULL);
    if (!rp) return false;
    bool iconic = false;
    int n = xcb_get_property_value_length(rp) / 4;
    uint32_t *v = (uint32_t *)xcb_get_property_value(rp);
    if (n >= 3) {
        uint32_t flags = v[0], initial_state = v[2];
        if ((flags & (1u << 1)) && initial_state == 3) iconic = true;
    }
    free(rp);
    return iconic;
}

static bool is_transient_window(xcb_window_t w) {
    xcb_get_property_cookie_t ck = xcb_get_property(wm.dpy, 0, w, wm_transient_for, XCB_ATOM_WINDOW, 0, 1);
    xcb_get_property_reply_t *rp = xcb_get_property_reply(wm.dpy, ck, NULL);
    bool t = rp && xcb_get_property_value_length(rp) >= 4;
    free(rp);
    return t;
}

static Client *manage(xcb_window_t w) {
    if (find_client(w)) return NULL;
    bool override_redirect = false;
    if (window_is_unmapped(w, &override_redirect) || override_redirect || is_dock_window(w)) return NULL;

    xcb_get_geometry_reply_t *gr = xcb_get_geometry_reply(wm.dpy, xcb_get_geometry(wm.dpy, w), NULL);
    if (!gr) return NULL;

    Client *c = calloc(1, sizeof(*c));
    if (!c) die("out of memory");
    c->win = w; c->x = gr->x; c->y = gr->y; c->w = gr->width; c->h = gr->height; c->workspace = wm.current_workspace;
    free(gr);

    c->is_minimized = is_iconic(w);
    if (is_transient_window(w)) { c->is_transient = true; c->is_floating = true; }

    uint32_t events = XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes(wm.dpy, w, XCB_CW_EVENT_MASK, &events);
    set_border(w, wm.border_normal);

    c->next = wm.clients;
    wm.clients = c;
    WorkspaceState *ws = current_workspace_state();
    if (!c->is_floating) { workspace_attach_tail(ws, c); if (!ws->scroll_focus) ws->scroll_focus = c; }
    set_client_desktop(c);
    if (!c->is_minimized) xcb_map_window(wm.dpy, c->win);
    set_input_focus(c);
    return c;
}

static void unmanage(Client *c) {
    if (!c) return;
    if (!c->is_floating) workspace_detach(c);
    if (wm.focused == c) wm.focused = NULL;
    Client **pp = &wm.clients;
    while (*pp && *pp != c) pp = &(*pp)->next;
    if (*pp) *pp = c->next;
    xcb_configure_window(wm.dpy, c->win, XCB_CONFIG_WINDOW_BORDER_WIDTH, (uint32_t[]){0});
    free(c);
}

static void client_close(Client *c) {
    if (!c) return;
    xcb_get_property_reply_t *rp = xcb_get_property_reply(wm.dpy, xcb_get_property(wm.dpy, 0, c->win, wm.wm_protocols, XCB_ATOM_ATOM, 0, 16), NULL);
    if (rp) {
        int n = xcb_get_property_value_length(rp) / (int)sizeof(xcb_atom_t);
        xcb_atom_t *p = (xcb_atom_t *)xcb_get_property_value(rp);
        for (int i = 0; i < n; i++) {
            if (p[i] == wm.wm_delete) {
                xcb_client_message_event_t ev = {0};
                ev.response_type = XCB_CLIENT_MESSAGE;
                ev.window = c->win;
                ev.type = wm.wm_protocols;
                ev.format = 32;
                ev.data.data32[0] = wm.wm_delete;
                ev.data.data32[1] = XCB_CURRENT_TIME;
                xcb_send_event(wm.dpy, 0, c->win, XCB_EVENT_MASK_NO_EVENT, (const char *)&ev);
                free(rp);
                return;
            }
        }
        free(rp);
    }
    xcb_kill_client(wm.dpy, c->win);
}

static void client_toggle_fullscreen(Client *c) {
    if (!c) return;
    c->is_fullscreen = !c->is_fullscreen;
    if (c->is_fullscreen) { c->oldx = c->x; c->oldy = c->y; c->oldw = c->w; c->oldh = c->h; }
    else { c->x = c->oldx; c->y = c->oldy; c->w = c->oldw; c->h = c->oldh; }
    if (c->is_fullscreen) {
        uint32_t fs = wm.net_wm_state_fullscreen;
        set_property32(c->win, wm.net_wm_state, XCB_ATOM_ATOM, &fs, 1);
    } else {
        delete_property(c->win, wm.net_wm_state);
    }
    arrange();
}

static bool can_floating_move_resize(Client *c) { return c && !c->is_fullscreen && (c->is_floating || c->is_transient); }

static void client_move(Client *c, int dx, int dy) { if (!can_floating_move_resize(c)) return; c->x += dx; c->y += dy; xcb_configure_window(wm.dpy, c->win, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, (uint32_t[]){(uint32_t)c->x, (uint32_t)c->y}); }
static void client_resize(Client *c, int dw, int dh) { if (!can_floating_move_resize(c)) return; c->w += dw; c->h += dh; if (c->w < 120) c->w = 120; if (c->h < 80) c->h = 80; xcb_configure_window(wm.dpy, c->win, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, (uint32_t[]){(uint32_t)c->w, (uint32_t)c->h}); }

static void client_minimize(Client *c) {
    if (!c) return;
    c->is_minimized = true;
    xcb_unmap_window(wm.dpy, c->win);
    if (wm.focused == c) wm.focused = NULL;
    arrange();
}

static void client_restore_last(void) {
    WorkspaceState *ws = current_workspace_state();
    Client *start = ws->min_restore_cursor ? ws->min_restore_cursor : wm.clients;
    Client *candidate = NULL;
    for (Client *c = start; c; c = c->next) if (c->workspace == wm.current_workspace && c->is_minimized) { candidate = c; break; }
    if (!candidate) for (Client *c = wm.clients; c && c != start; c = c->next) if (c->workspace == wm.current_workspace && c->is_minimized) { candidate = c; break; }
    if (!candidate) return;
    candidate->is_minimized = false;
    ws->min_restore_cursor = candidate->next;
    xcb_map_window(wm.dpy, candidate->win);
    raise_window(candidate->win);
    set_input_focus(candidate);
    arrange();
}

static void set_workspace(int ws) { if (ws < 0 || ws >= WORKSPACE_COUNT || ws == wm.current_workspace) return; wm.current_workspace = ws; update_current_desktop(); wm.focused = NULL; arrange(); }

static void move_client_workspace(Client *c, int ws) {
    if (!c || ws < 0 || ws >= WORKSPACE_COUNT) return;
    if (!c->is_floating) workspace_detach(c);
    c->workspace = ws; c->is_minimized = false;
    if (!c->is_floating) {
        WorkspaceState *target = &wm.workspaces[ws];
        workspace_attach_tail(target, c);
        if (!target->scroll_focus) target->scroll_focus = c;
    }
    set_client_desktop(c);
    if (ws != wm.current_workspace) {
        xcb_unmap_window(wm.dpy, c->win);
        if (wm.focused == c) wm.focused = NULL;
    }
    arrange();
}

static uint16_t clean_mod_mask(uint16_t state) { return (uint16_t)(state & ~(XCB_MOD_MASK_LOCK | wm.numlock_mask)); }

static void refresh_keyboard_map(void) {
    const xcb_setup_t *setup = xcb_get_setup(wm.dpy);
    keycode_min = setup->min_keycode;
    keycode_max = setup->max_keycode;
    uint8_t count = (uint8_t)(keycode_max - keycode_min + 1);
    xcb_get_keyboard_mapping_reply_t *rp = xcb_get_keyboard_mapping_reply(wm.dpy, xcb_get_keyboard_mapping(wm.dpy, keycode_min, count), NULL);
    if (!rp) return;
    free(keysyms);
    keysyms_per_keycode = rp->keysyms_per_keycode;
    int total = count * keysyms_per_keycode;
    keysyms = calloc((size_t)total, sizeof(xcb_keysym_t));
    if (keysyms) memcpy(keysyms, xcb_get_keyboard_mapping_keysyms(rp), (size_t)total * sizeof(xcb_keysym_t));
    free(rp);
}

static xcb_keycode_t keycode_from_keysym(xcb_keysym_t sym) {
    if (!keysyms || keysyms_per_keycode <= 0) return XCB_NO_SYMBOL;
    int count = keycode_max - keycode_min + 1;
    for (int i = 0; i < count; i++) {
        for (int col = 0; col < keysyms_per_keycode; col++) {
            if (keysyms[i * keysyms_per_keycode + col] == sym) return (xcb_keycode_t)(keycode_min + i);
        }
    }
    return XCB_NO_SYMBOL;
}

static void detect_numlock_mask(void) {
    wm.numlock_mask = 0;
    refresh_keyboard_map();
    xcb_get_modifier_mapping_reply_t *rp = xcb_get_modifier_mapping_reply(wm.dpy, xcb_get_modifier_mapping(wm.dpy), NULL);
    if (!rp) return;
    int kpm = rp->keycodes_per_modifier;
    xcb_keycode_t *mods = xcb_get_modifier_mapping_keycodes(rp);
    xcb_keycode_t nl = keycode_from_keysym(XK_Num_Lock);
    for (int mod = 0; mod < 8; mod++) {
        for (int k = 0; k < kpm; k++) {
            if (mods[mod * kpm + k] == nl && nl != XCB_NO_SYMBOL) wm.numlock_mask = (1u << mod);
        }
    }
    free(rp);
}

static void init_keybinding_keycodes(void) {
    refresh_keyboard_map();
    for (size_t i = 0; i < sizeof(keybindings) / sizeof(keybindings[0]); i++) {
        keybindings[i].keycode = keycode_from_keysym(keybindings[i].sym);
    }
}

static void grab_key_with_lock_variants(xcb_keycode_t code, uint16_t mod) {
    if (!code || code == XCB_NO_SYMBOL) return;
    uint16_t modifiers[] = {0, XCB_MOD_MASK_LOCK, (uint16_t)wm.numlock_mask, (uint16_t)(XCB_MOD_MASK_LOCK | wm.numlock_mask)};
    for (size_t i = 0; i < sizeof(modifiers) / sizeof(modifiers[0]); i++) {
        xcb_grab_key(wm.dpy, 1, wm.root, (uint16_t)(mod | modifiers[i]), code, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }
}

static void grab_all_keys(void) {
    xcb_ungrab_key(wm.dpy, XCB_GRAB_ANY, wm.root, XCB_MOD_MASK_ANY);
    for (size_t i = 0; i < sizeof(keybindings) / sizeof(keybindings[0]); i++) {
        grab_key_with_lock_variants(keybindings[i].keycode, keybindings[i].mod);
    }
}


static void grab_buttons(void) {
    xcb_ungrab_button(wm.dpy, XCB_BUTTON_INDEX_ANY, wm.root, XCB_MOD_MASK_ANY);
    uint16_t mods[] = {MOD_MASK, (uint16_t)(MOD_MASK | XCB_MOD_MASK_LOCK), (uint16_t)(MOD_MASK | wm.numlock_mask), (uint16_t)(MOD_MASK | XCB_MOD_MASK_LOCK | wm.numlock_mask)};
    for (size_t i = 0; i < sizeof(mods) / sizeof(mods[0]); i++) {
        xcb_grab_button(wm.dpy, 1, wm.root, XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION,
                        XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_WINDOW_NONE, XCB_CURSOR_NONE, XCB_BUTTON_INDEX_1, mods[i]);
        xcb_grab_button(wm.dpy, 1, wm.root, XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION,
                        XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_WINDOW_NONE, XCB_CURSOR_NONE, XCB_BUTTON_INDEX_3, mods[i]);
    }
}

static void dispatch_action(Action action, int arg) {
    switch (action) {
        case ACTION_SPAWN_TERMINAL: spawn_terminal(); break;
        case ACTION_SPAWN_MENU: spawn_menu(); break;
        case ACTION_CLOSE_FOCUSED: client_close(wm.focused); break;
        case ACTION_FOCUS_PREV: focus_scroll_relative(-1); arrange(); break;
        case ACTION_FOCUS_NEXT: focus_scroll_relative(1); arrange(); break;
        case ACTION_MOVE_UP: client_move(wm.focused, 0, -MOVE_STEP); break;
        case ACTION_MOVE_DOWN: client_move(wm.focused, 0, MOVE_STEP); break;
        case ACTION_RESIZE_LEFT: client_resize(wm.focused, -RESIZE_STEP, 0); break;
        case ACTION_RESIZE_RIGHT: client_resize(wm.focused, RESIZE_STEP, 0); break;
        case ACTION_RESIZE_UP: client_resize(wm.focused, 0, -RESIZE_STEP); break;
        case ACTION_RESIZE_DOWN: client_resize(wm.focused, 0, RESIZE_STEP); break;
        case ACTION_SET_WORKSPACE: set_workspace(arg); break;
        case ACTION_MOVE_TO_WORKSPACE: move_client_workspace(wm.focused, arg); break;
        case ACTION_TOGGLE_FULLSCREEN: client_toggle_fullscreen(wm.focused); break;
        case ACTION_MINIMIZE: client_minimize(wm.focused); break;
        case ACTION_RESTORE_MINIMIZED: client_restore_last(); break;
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

static void handle_keypress(xcb_key_press_event_t *e) {
    reload_runtime_config_if_changed();
    uint16_t clean = clean_mod_mask(e->state);
    for (size_t i = 0; i < sizeof(keybindings) / sizeof(keybindings[0]); i++) {
        if (keybindings[i].keycode == e->detail && keybindings[i].mod == clean) {
            dispatch_action(keybindings[i].action, keybindings[i].arg);
            return;
        }
    }
}

static void buttonpress(xcb_button_press_event_t *e) {
    if (clean_mod_mask(e->state) != MOD_MASK) return;
    Client *c = find_client(e->child ? e->child : e->event);
    if (!c || !is_visible(c) || !can_floating_move_resize(c)) return;
    set_input_focus(c);
    wm.drag_active = true;
    wm.drag_resize = (e->detail == XCB_BUTTON_INDEX_3);
    wm.drag_start_x = e->root_x; wm.drag_start_y = e->root_y;
    wm.drag_win_x = c->x; wm.drag_win_y = c->y; wm.drag_win_w = c->w; wm.drag_win_h = c->h;
    wm.drag_client = c;
    xcb_grab_pointer(wm.dpy, 0, wm.root, XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
                     XCB_NONE, XCB_NONE, XCB_CURRENT_TIME);
}

static void motionnotify(xcb_motion_notify_event_t *e) {
    if (!wm.drag_active || !wm.drag_client) return;
    Client *c = wm.drag_client;
    int dx = e->root_x - wm.drag_start_x;
    int dy = e->root_y - wm.drag_start_y;
    if (wm.drag_resize) {
        c->w = wm.drag_win_w + dx; c->h = wm.drag_win_h + dy;
        if (c->w < 120) c->w = 120;
        if (c->h < 80) c->h = 80;
        xcb_configure_window(wm.dpy, c->win, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, (uint32_t[]){(uint32_t)c->w, (uint32_t)c->h});
    } else {
        c->x = wm.drag_win_x + dx; c->y = wm.drag_win_y + dy;
        xcb_configure_window(wm.dpy, c->win, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, (uint32_t[]){(uint32_t)c->x, (uint32_t)c->y});
    }
}

static void buttonrelease(xcb_button_release_event_t *e) {
    (void)e;
    wm.drag_active = false;
    wm.drag_client = NULL;
    xcb_ungrab_pointer(wm.dpy, XCB_CURRENT_TIME);
}

static void maprequest(xcb_map_request_event_t *e) {
    Client *c = manage(e->window);
    if (!c) { xcb_map_window(wm.dpy, e->window); return; }
    arrange();
}

static void configurerequest(xcb_configure_request_event_t *e) {
    uint16_t mask = 0;
    uint32_t vals[7];
    int n = 0;
    if (e->value_mask & XCB_CONFIG_WINDOW_X) { mask |= XCB_CONFIG_WINDOW_X; vals[n++] = e->x; }
    if (e->value_mask & XCB_CONFIG_WINDOW_Y) { mask |= XCB_CONFIG_WINDOW_Y; vals[n++] = e->y; }
    if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH) { mask |= XCB_CONFIG_WINDOW_WIDTH; vals[n++] = e->width; }
    if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT) { mask |= XCB_CONFIG_WINDOW_HEIGHT; vals[n++] = e->height; }
    if (e->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) { mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH; vals[n++] = e->border_width; }
    if (e->value_mask & XCB_CONFIG_WINDOW_SIBLING) { mask |= XCB_CONFIG_WINDOW_SIBLING; vals[n++] = e->sibling; }
    if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) { mask |= XCB_CONFIG_WINDOW_STACK_MODE; vals[n++] = e->stack_mode; }

    Client *c = find_client(e->window);
    if (c) {
        if (can_floating_move_resize(c)) {
            if (e->value_mask & XCB_CONFIG_WINDOW_X) c->x = e->x;
            if (e->value_mask & XCB_CONFIG_WINDOW_Y) c->y = e->y;
            if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH) c->w = e->width;
            if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT) c->h = e->height;
            if (mask) xcb_configure_window(wm.dpy, e->window, mask, vals);
        } else arrange();
        return;
    }
    if (mask) xcb_configure_window(wm.dpy, e->window, mask, vals);
}

static void destroynotify(xcb_destroy_notify_event_t *e) { Client *c = find_client(e->window); if (c) { unmanage(c); arrange(); } }

static void unmapnotify(xcb_unmap_notify_event_t *e) {
    Client *c = find_client(e->window);
    if (!c) return;
    if (e->from_configure) c->is_minimized = true;
    else unmanage(c);
    arrange();
}

static void enternotify(xcb_enter_notify_event_t *e) {
    Client *c = find_client(e->event);
    if (c && c != wm.focused && is_visible(c)) {
        set_input_focus(c);
        if (is_scroll_managed(c)) arrange();
    }
}

static void clientmessage(xcb_client_message_event_t *e) {
    if (e->type == wm.net_active_window) {
        long source = (long)e->data.data32[0];
        if (source != 2) return;
        Client *c = find_client(e->window);
        if (c) { set_workspace(c->workspace); set_input_focus(c); if (is_scroll_managed(c)) arrange(); }
        return;
    }
    if (e->type == wm.net_current_desktop) set_workspace((int)e->data.data32[0]);
}

static void scan_existing_windows(void) {
    xcb_query_tree_reply_t *qr = xcb_query_tree_reply(wm.dpy, xcb_query_tree(wm.dpy, wm.root), NULL);
    if (!qr) return;
    int len = xcb_query_tree_children_length(qr);
    xcb_window_t *children = xcb_query_tree_children(qr);
    for (int i = 0; i < len; i++) {
        bool override_redirect = false;
        if (window_is_unmapped(children[i], &override_redirect) || override_redirect) continue;
        manage(children[i]);
    }
    free(qr);
    arrange();
}

static void setup_atoms(void) {
    wm.wm_protocols = atom("WM_PROTOCOLS");
    wm.wm_delete = atom("WM_DELETE_WINDOW");
    wm.net_active_window = atom("_NET_ACTIVE_WINDOW");
    wm.net_wm_state = atom("_NET_WM_STATE");
    wm.net_wm_state_fullscreen = atom("_NET_WM_STATE_FULLSCREEN");
    wm.net_supported = atom("_NET_SUPPORTED");
    wm.net_wm_desktop = atom("_NET_WM_DESKTOP");
    wm.net_current_desktop = atom("_NET_CURRENT_DESKTOP");
    wm.net_wm_window_type = atom("_NET_WM_WINDOW_TYPE");
    wm.net_wm_window_type_dock = atom("_NET_WM_WINDOW_TYPE_DOCK");
    net_number_of_desktops = atom("_NET_NUMBER_OF_DESKTOPS");
    wm_transient_for = atom("WM_TRANSIENT_FOR");
    wm_hints = atom("WM_HINTS");
    wm_state = atom("WM_STATE");
    (void)wm_state;

    uint32_t supported[] = {wm.net_active_window, wm.net_wm_state, wm.net_wm_state_fullscreen, wm.net_wm_desktop, wm.net_current_desktop};
    set_property32(wm.root, wm.net_supported, XCB_ATOM_ATOM, supported, (uint32_t)(sizeof(supported) / sizeof(supported[0])));
    uint32_t workspaces = WORKSPACE_COUNT;
    set_property32(wm.root, net_number_of_desktops, XCB_ATOM_CARDINAL, &workspaces, 1);
    update_current_desktop();
}

void wm_init(void) {
    signal(SIGCHLD, SIG_IGN);
    int screen_idx = 0;
    wm.dpy = xcb_connect(NULL, &screen_idx);
    if (!wm.dpy || xcb_connection_has_error(wm.dpy)) die("cannot connect to X server");

    const xcb_setup_t *setup = xcb_get_setup(wm.dpy);
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    for (int i = 0; i < screen_idx; i++) xcb_screen_next(&it);
    wm.screen = it.data;
    if (!wm.screen) die("cannot get X screen");

    wm.root = wm.screen->root;
    wm.sw = wm.screen->width_in_pixels;
    wm.sh = wm.screen->height_in_pixels;
    wm.border_focus = color_from_hex(COLOR_BORDER_FOCUS);
    wm.border_normal = color_from_hex(COLOR_BORDER_NORMAL);
    wm.current_workspace = 0;
    wm.running = true;

    uint32_t root_mask = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_BUTTON_PRESS |
                         XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_ENTER_WINDOW |
                         XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_KEY_PRESS;
    xcb_check_ok(xcb_change_window_attributes_checked(wm.dpy, wm.root, XCB_CW_EVENT_MASK, &root_mask));

    setup_atoms();
    load_runtime_config();
    detect_numlock_mask();
    init_keybinding_keycodes();
    grab_all_keys();
    grab_buttons();
    scan_existing_windows();

    xcb_flush(wm.dpy);
}

void wm_run(void) {
    while (wm.running) {
        xcb_generic_event_t *ev = xcb_wait_for_event(wm.dpy);
        if (!ev) break;
        reload_runtime_config_if_changed();
        uint8_t type = ev->response_type & ~0x80;
        switch (type) {
            case XCB_KEY_PRESS: handle_keypress((xcb_key_press_event_t *)ev); break;
            case XCB_BUTTON_PRESS: buttonpress((xcb_button_press_event_t *)ev); break;
            case XCB_BUTTON_RELEASE: buttonrelease((xcb_button_release_event_t *)ev); break;
            case XCB_MOTION_NOTIFY: motionnotify((xcb_motion_notify_event_t *)ev); break;
            case XCB_MAP_REQUEST: maprequest((xcb_map_request_event_t *)ev); break;
            case XCB_CONFIGURE_REQUEST: configurerequest((xcb_configure_request_event_t *)ev); break;
            case XCB_DESTROY_NOTIFY: destroynotify((xcb_destroy_notify_event_t *)ev); break;
            case XCB_UNMAP_NOTIFY: unmapnotify((xcb_unmap_notify_event_t *)ev); break;
            case XCB_ENTER_NOTIFY: enternotify((xcb_enter_notify_event_t *)ev); break;
            case XCB_CLIENT_MESSAGE: clientmessage((xcb_client_message_event_t *)ev); break;
            default: break;
        }
        free(ev);
    }
}

void wm_cleanup(void) {
    while (wm.clients) {
        Client *next = wm.clients->next;
        free(wm.clients);
        wm.clients = next;
    }
    free(keysyms);
    if (wm.dpy) xcb_disconnect(wm.dpy);
}
