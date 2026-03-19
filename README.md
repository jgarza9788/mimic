# ScrollWM

ScrollWM is an experimental scrollable tiling window manager for X11/Xorg.

## Dependency matrix (build + runtime)

### Build-time requirements

- C++20 compiler (`gcc`/`clang`)
- Meson
- Ninja
- pkg-config
- XCB development libraries:
  - `xcb`
  - `xcb-keysyms` (`xcb-util-keysyms`)
  - `xcb-icccm` (`xcb-util-wm`)
  - `xcb-util`

### Runtime requirements

- Xorg/X11 server (for example: `xorg`, `xorg-xinit`, display manager stack)
- `scrollwm` binary and `scrollwm-session` script installed into your prefix
- At least one terminal emulator (ScrollWM now uses fallback chain on `Mod+Enter`):
  - `xterm` -> `kitty` -> `alacritty` -> `foot` -> `x-terminal-emulator`

### Optional runtime components

- `picom` (only if you enable `[autostart].launch_picom = true` in config)
- Display manager package (GDM/SDDM/LightDM) if you want graphical session selection

## Distro package names

### Arch / CachyOS

```bash
sudo pacman -S --needed \
  base-devel meson ninja pkgconf \
  libxcb xcb-util xcb-util-keysyms xcb-util-wm \
  xorg-server xorg-xinit xterm
```

Optional:

```bash
sudo pacman -S --needed picom kitty alacritty foot
```

### Fedora

```bash
sudo dnf install \
  gcc-c++ meson ninja-build pkgconf-pkg-config \
  libxcb-devel xcb-util-devel xcb-util-keysyms-devel xcb-util-wm-devel \
  xorg-x11-server-Xorg xorg-x11-xinit xterm
```

Optional:

```bash
sudo dnf install picom kitty alacritty foot
```

### Ubuntu / Debian

```bash
sudo apt update
sudo apt install \
  build-essential meson ninja-build pkg-config \
  libxcb1-dev libxcb-util-dev libxcb-keysyms1-dev libxcb-icccm4-dev \
  xorg xinit xterm
```

Optional:

```bash
sudo apt install picom kitty alacritty foot x-terminal-emulator
```

## Build

```bash
meson setup build
meson compile -C build
```

## Install

```bash
meson install -C build
```

By default Meson installs to `/usr/local`. For display managers, `/usr` is usually safer:

```bash
meson setup build --prefix=/usr
meson compile -C build
sudo meson install -C build
```

## Launch methods

### 1) `startx`

In `~/.xinitrc`:

```sh
exec scrollwm-session
```

Then run:

```bash
startx
```

### 2) Display managers (GDM/SDDM/LightDM)

- Install ScrollWM system-wide.
- Confirm desktop entry exists in `/usr/share/xsessions/scrollwm.desktop` (or your prefix equivalent).
- Select **ScrollWM** from the session chooser and log in.

The installed desktop entry now uses an absolute `Exec=` path matching your Meson `bindir`, so `/usr/local` installs are no longer hidden by a minimal DM `PATH`.

## Configuration

Default config path:

```text
~/.config/scrollwm/config.toml
```

Copy sample:

```bash
mkdir -p ~/.config/scrollwm
cp /usr/share/doc/scrollwm/config.toml.example ~/.config/scrollwm/config.toml
```

If no config exists, ScrollWM starts with built-in defaults and logs a warning.

## Troubleshooting

### Black screen / immediate return to greeter

- Check session logs (display manager journal, `~/.xsession-errors`, etc.).
- Verify Xorg is available and `DISPLAY` is set in session.
- Run `scrollwm` from an X terminal to inspect startup logs directly.

### `could not acquire WM ownership`

Another WM is still running on the same X display. Use:

```bash
scrollwm-session --wait-for-wm=10
```

The session wrapper retries and prints progress while waiting.

### Missing terminal on `Mod+Enter`

ScrollWM logs an actionable error if no terminal is found. Install one of:
`xterm`, `kitty`, `alacritty`, `foot`, or `x-terminal-emulator`.

### Missing session entry in login screen

- Verify installation prefix and desktop file location.
- Reinstall with `--prefix=/usr` if your DM does not include `/usr/local` session paths.
- Confirm desktop file references a valid `Exec` target.

### Dependency mismatch / build failure

Use pkg-config checks:

```bash
pkg-config --modversion xcb xcb-keysyms xcb-icccm xcb-util
```

If any module is missing, install the corresponding `-dev`/`-devel` package from your distro matrix above.

## Verification commands

After install, run:

```bash
# Binary linkage
ldd "$(command -v scrollwm)"

# Session entry installed where expected
ls -l /usr/share/xsessions/scrollwm.desktop

# Validate desktop entry Exec path
grep '^Exec=' /usr/share/xsessions/scrollwm.desktop

# Ensure X11 environment exists in current shell
printf 'DISPLAY=%s\n' "${DISPLAY:-<unset>}"

# Detect likely running WM owners/processes
xprop -root _NET_SUPPORTING_WM_CHECK _NET_WM_NAME
ps -ef | grep -E 'i3|bspwm|openbox|xfwm4|kwin_x11|mutter|scrollwm' | grep -v grep
```

## Tests and validation

```bash
meson test -C build --print-errorlogs
./scripts/validate-config-schema.py config/config.toml.example
```

## One-shot bootstrap

```bash
./scripts/bootstrap-build-install-test.sh
```

Environment overrides:

- `PREFIX` (default `/usr/local`)
- `BUILD_DIR` (default `build`)

## License

MIT (see [LICENSE](LICENSE)).
