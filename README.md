# mimic

`mimic` is a new X11/XLibre window manager project inspired by Niri's scrollable tiling mental model. It targets a daily-drivable direction while staying contributor-friendly and modular.

## Project overview

- **Type:** scrollable tiling WM for X11/XLibre only.
- **Core model:** each workspace is one continuous horizontal strip of tiled windows.
- **Focus behavior:** focus moves left/right while the viewport effectively scrolls around the centered focused window.
- **Compositor model:** designed to work with stock picom first; future hooks are planned for deeper compositor-aware metadata.

## Goals

- Build an XCB-based C++ WM with strong architecture boundaries.
- Keep dynamic tiling + floating as first-class modes.
- Add a scriptable IPC interface from day one (`mimic-msg`).
- Provide practical docs, tests, and CI so external contributors can iterate quickly.

## Non-goals (for this initial slice)

- Wayland support.
- Complete Niri parity in v0 scaffold.
- Full animation engine in WM core.

## Current architecture

- `src/xcb_backend.cpp`: X11 connection, event polling, focus, and placement operations.
- `src/window_manager.cpp`: lifecycle orchestration for map/destroy/key events.
- `src/layout_engine.cpp`: Niri-like horizontal strip insertion/focus/placement logic.
- `src/workspace_manager.cpp` and `src/monitor_manager.cpp`: monitor-local workspace scaffolding.
- `src/config.cpp` and `src/config_watcher.cpp`: layered TOML loading and reload polling.
- `src/ipc_server.cpp` and `src/mimic_msg.cpp`: local command transport (`mimic msg` equivalent).
- `src/compositor_bridge.cpp`: extension seam for future picom-aware metadata.

See `docs/architecture.md` for details.

## Build instructions

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
```

Dependencies (Ubuntu/Debian):

```bash
sudo apt-get install cmake g++ pkg-config libxcb1-dev libxcb-keysyms1-dev
```

## Test instructions

```bash
ctest --test-dir build --output-on-failure
```

## Install instructions

```bash
./scripts/install.sh
```

Or directly:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
sudo cmake --install build
```

The installer also places a desktop session entry at:

- `/usr/share/xsessions/mimic.desktop`

`cmake --install` also installs a session file under the active install prefix (for example `/usr/local/share/xsessions/mimic.desktop`).
This allows both **SDDM** and **GDM** to list **Mimic** as an X11 session option on the login screen.

## Uninstall instructions

```bash
./scripts/uninstall.sh
```

## Config locations

- System defaults: `<install-prefix>/share/mimic/config/default.toml` (for `./scripts/install.sh`, this is typically `/usr/local/share/mimic/config/default.toml`)
- User overrides: `~/.config/mimic/config.toml`

Precedence: user config overrides system defaults.

## Default keybindings

- `Super+h` → `focus left`
- `Super+l` → `focus right`
- `Super+o` → `toggle overview` (stubbed command in this slice)

## IPC quick usage

```bash
mimic-msg query focused
mimic-msg query windows
mimic-msg focus left
mimic-msg focus right
```

## Development roadmap

Short-term milestones:

1. Harden EWMH/ICCCM handling and startup checks.
2. Add real XRandR monitor management.
3. Implement floating/fullscreen/rules expansion.
4. Build overview mode model + rendering policy.
5. Add richer IPC schema and event subscriptions.

See `docs/roadmap.md`.

## Contributor notes

- Favor small classes with single responsibilities.
- Keep comments explicit; this codebase optimizes readability over cleverness.
- Add tests for layout and behavior changes, especially simulation-friendly logic.
- Capture deferred ideas with `TODO` markers close to relevant code.

## Picom integration notes

`mimic` currently integrates with picom by staying standards-friendly and exposing a dedicated `CompositorBridge` abstraction for future atom/metadata publication. This avoids coupling early WM implementation to compositor forks while keeping a clear extension path.

## First working slice status

Implemented now:

- Launches and connects to X server via XCB.
- Subscribes to map/destroy/key events.
- Manages discovered + newly mapped windows into a horizontal strip.
- Tracks focus and supports left/right movement.
- Recomputes centered-layout placements after focus/lifecycle changes.
- Includes simulation-friendly unit tests for layout/workspace/config behavior.

Stubbed next:

- True floating layer interactions and fullscreen bypass restoration.
- Real monitor hotplug and multi-monitor workspace reassignment.
- Overview mode rendering/state transitions.
- Full rule matching, swallowing, and richer IPC events.
