# Mimic

Mimic is now a **minimally working X11 window manager runtime** built with Xlib and C++.

It is still early-stage, but V001 now runs as a real WM process: it connects to X, claims the WM role, enters an event loop, manages top-level windows, applies a deterministic layout, and dispatches `exec` key bindings.

## What works now

- Real X11 startup path (`XOpenDisplay`, root acquisition, WM conflict detection).
- Root `SubstructureRedirectMask` claim with clean error handling when another WM is active.
- Stable event loop with handling for:
  - `MapRequest`
  - `ConfigureRequest`
  - `DestroyNotify`
  - `UnmapNotify`
  - `KeyPress`
- Basic top-level client management (no full reparenting yet).
- Deterministic layout applied to managed windows on the active workspace.
- Existing Mimic architecture is wired into runtime:
  - `MimicWorkspaceModel`
  - `MimicLayoutEngine` + `MimicViewport`
  - `MimicOverviewController`
  - `MimicCommandRegistry`
- Config search path logic with graceful fallback and parse diagnostics.
- Foreground stderr logs + runtime log file (`$XDG_RUNTIME_DIR/mimic.log` or `/tmp/mimic.log`).

## What is still incomplete

- No full Fluxbox-level reparenting/decorations.
- No advanced workspace navigation commands yet.
- Overview controller is stateful and connected, but not yet rendered visually.
- Scrolling-strip UX remains future work.

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Install

```bash
cmake --install build --prefix /usr/local
```

Installed artifacts:
- `/usr/local/bin/mimic`
- `/usr/local/share/xsessions/mimic.desktop`
- `/usr/local/share/mimic/examples/mimic.keys`

## Run from TTY (`xinit`)

Use `~/.xinitrc`:

```bash
exec mimic
```

Then start X:

```bash
startx
```

## Session entry for display managers

Install and then select **Mimic** from GDM/SDDM/etc. The desktop file is:
- `sessions/mimic.desktop` (`Exec=mimic`, `Name=Mimic`)

## Config file search order

Mimic checks, in order:
1. explicit CLI path (`mimic /path/to/mimic.keys`)
2. `$XDG_CONFIG_HOME/mimic/mimic.keys`
3. `~/.config/mimic/mimic.keys`
4. `/etc/xdg/mimic/mimic.keys`
5. `/usr/local/share/mimic/examples/mimic.keys`
6. `/usr/share/mimic/examples/mimic.keys`

Syntax:

```txt
bind Mod4+Return exec xterm
bind Mod4+d exec dmenu_run
```

## Logging and diagnostics

Startup logs include:
- X connection status
- WM root claim status
- config path used
- loaded exec binding count
- dry mode vs real WM mode

If no X server exists, Mimic runs dry mode and logs bootstrap state.

## Troubleshooting

### "Mimic initialized with 0 exec bindings"
That old scaffold behavior has been replaced. If you still have zero bindings, Mimic now logs where it searched and whether it found a config.

### "Another window manager is already running"
Mimic detected `BadAccess` while trying to claim `SubstructureRedirectMask` on the root window. Exit the existing WM or start Mimic in a fresh X session.

### "Mimic exits immediately"
Check stderr and `mimic.log` for:
- failed X connection
- WM conflict on root
- fatal X protocol errors

### "No session entry appears in GDM/SDDM"
Verify installation of `mimic.desktop` into your active `xsessions` directory (commonly `/usr/share/xsessions` or `/usr/local/share/xsessions`).

### "No config file found"
Mimic continues with defaults. Create `~/.config/mimic/mimic.keys` and add `bind ... exec ...` lines.

## Documentation

- `docs/INSTALL.md`
- `docs/architecture.md`
- `docs/roadmap.md`
- `docs/forking-from-fluxbox.md`
