# MimicWM

MimicWM is a small floating-first window manager written in C using Xlib.
It targets **X11 / XLibre** sessions (not Wayland), is designed for easy compilation on common Linux distros, and delegates compositing to **picom**.

## Current status

This is a minimal but usable first version with:

- floating window management as the default mode
- optional basic tiling toggle
- workspaces (4 by default)
- keyboard and mouse move/resize operations
- focused border highlighting
- fullscreen toggling
- minimize/restore support
- simple EWMH/ICCCM basics (`_NET_ACTIVE_WINDOW`, desktop tracking, `WM_DELETE_WINDOW`)
- SDDM-compatible X session entry

## Features

- Manages normal app windows and tracks map/unmap/destroy lifecycle.
- Handles transients/dialogs as floating windows.
- Ignores dock windows (`_NET_WM_WINDOW_TYPE_DOCK`) so panels can coexist.
- Focus follows both keyboard navigation and mouse enter.
- Mouse drag support:
  - `Super + Left Mouse`: move window
  - `Super + Right Mouse`: resize window
- Works with picom via a session script (`scripts/start-mimicwm-session.sh`).

## Limitations (intentional for v1)

- No built-in status bar.
- No advanced tiling tree; tiling mode is a basic master/stack layout.
- No dynamic keybinding parser yet (bindings are compile-time in `include/config.h` + WM source).
- No multi-monitor placement logic yet.

## Repository layout

- `src/` – C source files
- `include/` – headers and default constants
- `config/` – default configs (`picom.conf`, launcher config)
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

### Distro package hints

#### Fedora

```bash
sudo dnf install gcc make libX11-devel libXext-devel picom xterm dmenu
```

#### Arch / CachyOS

```bash
sudo pacman -S --needed base-devel libx11 libxext picom xterm dmenu
```

#### Debian / Ubuntu

```bash
sudo apt install build-essential libx11-dev libxext-dev picom xterm suckless-tools
```

## Build

Release build:

```bash
make
```

Debug-friendly build:

```bash
make debug
```

Manual clean:

```bash
make clean
```

## Install

System-wide install (default prefix `/usr/local`):

```bash
sudo ./install.sh
```

Or direct Make install:

```bash
sudo make install PREFIX=/usr/local
```

Installed files include:

- binary: `/usr/local/bin/mimicwm`
- session desktop: `/usr/local/share/xsessions/mimicwm.desktop`
- session launcher: `/usr/local/share/mimicwm/start-mimicwm-session.sh`
- picom sample config: `/usr/local/share/mimicwm/picom.conf`

## Uninstall

```bash
sudo ./uninstall.sh
```

Or:

```bash
sudo make uninstall PREFIX=/usr/local
```

## SDDM usage

1. Install MimicWM (`sudo ./install.sh`).
2. Log out to SDDM.
3. Select **MimicWM** from the session chooser.
4. Log in.

The session script starts picom (if installed) and then executes `mimicwm`.

## Testing in Xephyr

You can test without logging out of your current desktop:

```bash
Xephyr :2 -screen 1280x720 &
DISPLAY=:2 picom --config ./config/picom.conf &
DISPLAY=:2 ./mimicwm
```

Then run apps inside that display:

```bash
DISPLAY=:2 xterm
```

## Default keybindings

- `Super+Enter` → launch terminal (`xterm`)
- `Super+R` → launch app menu (`dmenu_run`)
- `Super+Q` → close focused window
- `Super+Left / Super+Right` → focus previous/next window
- `Super+Up / Super+Down` → move focused window vertically
- `Super+Shift+Left/Right/Up/Down` → resize focused window
- `Super+1..4` → switch workspace
- `Super+Shift+1..4` → move focused window to workspace
- `Super+F` → toggle fullscreen
- `Super+M` → minimize focused window
- `Super+Shift+M` → restore a minimized window in current workspace
- `Super+Space` → toggle simple tiling mode

## Configuration

- Compile-time defaults: `include/config.h`
  - border width
  - color values
  - workspace count
  - mod key
  - default terminal/menu commands
- Session script runtime config:
  - `~/.config/mimicwm/config` (optional shell vars)
  - `~/.config/mimicwm/picom.conf`

## How picom is used

MimicWM does not implement compositing internally.

- The script `scripts/start-mimicwm-session.sh` checks for `picom`.
- If found, it starts picom with user config (`~/.config/mimicwm/picom.conf`) when present.
- The WM remains independent and still runs without picom.

## Troubleshooting

- **Black screen on login**:
  - switch to TTY (`Ctrl+Alt+F3`), inspect `~/.xsession-errors` or SDDM logs.
  - verify `/usr/local/bin/mimicwm` exists and is executable.
- **Could not acquire WM control**:
  - another WM is already running on that display.
- **No terminal on `Super+Enter`**:
  - install `xterm` or rebuild with a different `TERMINAL_CMD` in `include/config.h`.
- **No transparency/shadows**:
  - ensure picom is installed and running.

## License

See [LICENSE](./LICENSE).
