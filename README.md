# Mimic

Mimic is an X11/Xlib C++ window manager project that is **intended to be a Fluxbox-derived fork** and evolve toward a Niri-style scrolling UX on X11.

> Note: in this environment, direct upstream cloning from GitHub was blocked, so this repository currently contains a clean Mimic-first bootstrap scaffold and architecture slices for the first wave.

## Current status (first wave)

Implemented now:
- `mimic` binary target and `Mimic` X session file (`sessions/mimic.desktop`)
- first-wave architecture modules:
  - `MimicViewport`
  - `MimicWorkspaceModel`
  - `MimicLayoutEngine`
  - `MimicOverviewController`
  - `MimicCommandRegistry`
- exec command binding parser/registry (`bind <key> exec <command>`)
- dynamic workspace policy with invariant: at least one workspace always remains
- overview mode state machine skeleton with grid preview tiling metadata
- unit tests for parser, workspace policy, and overview transitions

Experimental / not complete yet:
- full Fluxbox event loop integration
- real window reparenting/management behavior
- full scrolling-strip layout + animations
- TOML configuration parser

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Run

```bash
./build/mimic [optional/path/to/mimic.keys]
```

If no X server is available, Mimic runs in dry mode and prints registry/workspace bootstrap status.

## Config location and example

For now, a sample key file is included at:
- `config/mimic.keys`

Example safe commands:
- `bind Mod4+Return exec xterm`
- `bind Mod4+d exec dmenu_run`

## Running under xinit / display managers

- Install the project (`cmake --install build`) to place:
  - `mimic` in `bin`
  - `mimic.desktop` in `share/xsessions`
- Then select **Mimic** from your DM session chooser.

For `xinit`, you can use:

```bash
exec mimic
```

## Documentation

- `docs/forking-from-fluxbox.md`
- `docs/architecture.md`
- `docs/roadmap.md`
- `docs/INSTALL.md`
