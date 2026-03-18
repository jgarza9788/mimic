# ScrollWM

ScrollWM is an experimental scrollable tiling window manager for X11/XLibre, inspired by Niri's scrolling workspace idea while staying realistic to X11 constraints.

## Features in this prototype

- Modern C++20 codebase with Meson + Ninja
- X11 WM skeleton with ownership check (`SubstructureRedirectMask`)
- Core event handling: map/unmap/destroy/configure/key/focus
- Scrollable linear layout (horizontal or vertical)
- Focus navigation (`Mod+j` / `Mod+k`) with viewport shifting
- 4 workspaces by default (`Mod+1..4`)
- Simple TOML config loader (`~/.config/scrollwm/config.toml`)
- XSessions integration for display managers
- Optional picom startup via session wrapper and config

## Build requirements

- C++20 compiler (gcc/clang)
- Meson + Ninja
- `xcb`
- `xcb-keysyms`

Example packages (Debian/Ubuntu style):

```bash
sudo apt install build-essential meson ninja-build libxcb1-dev libxcb-keysyms1-dev
```

## Build

```bash
meson setup build
meson compile -C build
```

## One-shot bootstrap (install deps, build, install, test)

A helper script is provided for common Linux distributions:

```bash
./scripts/bootstrap-build-install-test.sh
```

Environment overrides:

- `PREFIX` (default `/usr/local`)
- `BUILD_DIR` (default `build`)

## Install

```bash
meson install -C build
```

This installs:

- `scrollwm` binary to `${prefix}/bin`
- `scrollwm-session` wrapper to `${prefix}/bin`
- `scrollwm.desktop` to `${datadir}/xsessions`
- docs and sample config under `${datadir}/doc/scrollwm`

> For display-manager visibility (GDM/SDDM), install with a system prefix such as `/usr` so the desktop file lands in `/usr/share/xsessions`.

## Session startup and display managers

The installed desktop entry points to:

```text
Exec=scrollwm-session
```

`scrollwm-session` can read `~/.config/scrollwm/config.toml` and optionally launch picom when:

```toml
[autostart]
launch_picom = true
compositor = "picom --experimental-backends"
```

Then it execs `scrollwm`.

## startx usage

In `~/.xinitrc`:

```sh
exec scrollwm-session
```

## Default keybindings

- `Mod+Enter`: launch terminal
- `Mod+j`: focus next
- `Mod+k`: focus previous
- `Mod+1..4`: switch workspace
- `Mod+q`: close focused window (WM_DELETE_WINDOW)
- `Mod+Shift+e`: exit ScrollWM
- `Mod+Space`: toggle layout direction (horizontal/vertical) at runtime
- `Mod+Shift+j` / `Mod+Shift+k`: reorder focused window in scroll order
- `Mod+f`: toggle fullscreen on focused tiled window (with restore)

## Config

Copy the sample config:

```bash
mkdir -p ~/.config/scrollwm
cp /usr/share/doc/scrollwm/config.toml.example ~/.config/scrollwm/config.toml
```

Configuration sections currently parsed:

- `[general]`: `mod_key`, `workspace_count`, `focus_follows_mouse`, `terminal`
- `[layout]`: `layout_direction`
- `[appearance]`: `gap`, `border_width`, `outer_padding`
- `[autostart]`: `launch_picom`, `compositor`

See [`config/config.toml.example`](config/config.toml.example).

## Testing

Run unit tests after building:

```bash
meson test -C build --print-errorlogs
```

## Development roadmap

See [docs/milestones.md](docs/milestones.md).

## License

MIT (see [LICENSE](LICENSE)).
