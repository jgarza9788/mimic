# MimicWM

MimicWM is a small floating-first window manager written in C using Xlib.
It targets **X11 / XLibre** sessions (not Wayland), is designed for easy compilation on common Linux distros, and delegates compositing to **picom**.

## Current status

This version keeps floating support, workspaces, and mouse operations, while introducing a **scrollable workspace layout** for normal windows.

- scrollable horizontal strip layout per workspace (focused window centered)
- floating/transient window support (dialogs remain floating)
- workspaces (4 by default)
- keyboard and mouse move/resize operations for floating windows
- focused border highlighting
- fullscreen toggling
- minimize/restore support
- simple EWMH/ICCCM basics (`_NET_ACTIVE_WINDOW`, desktop tracking, `WM_DELETE_WINDOW`)
- SDDM-compatible X session entry

## Scroll layout behavior

Each workspace now tracks its own ordered list of scroll-managed windows and focused entry.

- `Super+Left / Super+Right`: move backward/forward through the workspace order
- focused window is laid out prominently near the center
- neighboring windows remain positioned left/right to preserve a scroll-strip mental model
- order is stable unless you explicitly move a window to another workspace
- minimized windows are excluded from the strip
- fullscreen temporarily bypasses strip placement and restores normal layout when toggled off

`Super+Space` now toggles floating mode for the focused non-transient window.

## Repository layout

- `src/` – C source files
- `include/` – headers and default constants
- `config/` – default configs (`picom.conf`, `config.toml`)
- `scripts/` – session startup helpers
- `assets/` – desktop session entry
- `Makefile` – build/install rules
- `install.sh` – system install helper
- `uninstall.sh` – removal helper

## Dependencies

### Runtime

- Xorg/XLibre session
- `picom` (recommended)
- terminal emulator (`xterm` default)
- optional launcher (`dmenu_run` by default)

### Build

- C compiler (`gcc`/`clang`)
- `make`
- `libX11` + development headers

## Build

```bash
make
```

Debug build:

```bash
make debug
```

Clean:

```bash
make clean
```

## Install / Uninstall

```bash
sudo ./install.sh
sudo ./uninstall.sh
```

Or use:

```bash
sudo make install PREFIX=/usr/local
sudo make uninstall PREFIX=/usr/local
```

## Testing in Xephyr

```bash
Xephyr :2 -screen 1280x720 &
DISPLAY=:2 picom --config ./config/picom.conf &
DISPLAY=:2 ./mimicwm
```

Launch test apps:

```bash
DISPLAY=:2 xterm
```

## Default keybindings

- `Super+Enter` → launch terminal
- `Super+R` → launch app menu
- `Super+Q` → close focused window
- `Super+Left / Super+Right` → scroll focus previous/next window
- `Super+Up / Super+Down` → move focused floating window
- `Super+Shift+Left/Right/Up/Down` → resize focused floating window
- `Super+1..4` → switch workspace
- `Super+Shift+1..4` → move focused window to workspace
- `Super+F` → toggle fullscreen
- `Super+M` → minimize focused window
- `Super+Shift+M` → restore a minimized window in current workspace
- `Super+Space` → toggle floating for focused non-transient window

Caps Lock and Num Lock are ignored for keybinding matching.

## Runtime configuration

Runtime config path resolution:

1. `~/.config/mimicwm/config.toml` (canonical)
2. `~/.config/mimicwm/config.TOML` (compat fallback)
3. `/usr/local/share/mimicwm/config.toml`

Supported keys:

- `[commands] terminal = "..."`
- `[commands] menu = "..."`
- `[picom] backend = "..."` (stored for session compatibility)

The WM checks config changes on keypress and throughout the event loop. Parse errors are logged to stderr and the previous known-good runtime config is kept.

## Manual test checklist

- [ ] Start MimicWM and verify no crash with **0 windows**.
- [ ] Open one terminal and verify centered scroll layout.
- [ ] Open 3–6 normal windows and verify `Super+Left/Right` scroll focus and recenter.
- [ ] Verify `Super+Enter`, `Super+R`, `Super+Q`.
- [ ] Verify `Super+F` enter/exit fullscreen.
- [ ] Verify `Super+M` minimize and `Super+Shift+M` restore.
- [ ] Verify `Super+1..4` workspace switch preserves per-workspace order/focus.
- [ ] Verify `Super+Shift+1..4` moves a window and that destination workspace order is stable.
- [ ] Verify transient/dialog remains floating over its parent.
- [ ] Verify mixed floating + scroll-managed windows.
- [ ] Edit `~/.config/mimicwm/config.toml`, then trigger a keybinding and verify updated command usage without restart.
- [ ] Repeat in Xephyr (`DISPLAY=:2`) before system session use.

## License

See [LICENSE](./LICENSE).
