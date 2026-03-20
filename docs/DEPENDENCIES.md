# Mimic Dependencies

This file lists required and optional dependencies, plus install commands for CachyOS/Arch and Fedora.

## Required build tools

- `cmake` (>= 3.16)
- C++17 compiler (`gcc`/`clang`)
- `make` or `ninja` (CMake generator backend)
- `pkg-config` (for explicit dependency diagnostics)

### CachyOS / Arch

```bash
sudo pacman -S --needed base-devel cmake pkgconf
```

### Fedora

```bash
sudo dnf install @development-tools cmake pkgconf-pkg-config
```

## Required development libraries

- `libX11` headers and linker library
- pkg-config module: `x11`

### CachyOS / Arch

```bash
sudo pacman -S --needed libx11
```

### Fedora

```bash
sudo dnf install libX11-devel
```

## Runtime dependencies

- Xorg/X11 server (for real WM mode)
- A login/session launcher such as `startx` or a display manager (GDM/SDDM/etc.)
- Shell: `/bin/sh`

### CachyOS / Arch

```bash
sudo pacman -S --needed xorg-server xorg-xinit
```

### Fedora

```bash
sudo dnf install xorg-x11-server-Xorg xorg-x11-xinit
```

## Optional runtime tools

These are not required to build/start Mimic, but are useful for default key bindings:

- `xterm`
- `dmenu` (`dmenu_run`)
- `i3lock`

### CachyOS / Arch

```bash
sudo pacman -S --needed xterm dmenu i3lock
```

### Fedora

```bash
sudo dnf install xterm dmenu i3lock
```

## Dependency check command

```bash
bash scripts/check_dependencies.sh
```
