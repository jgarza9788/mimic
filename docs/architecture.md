# Mimic architecture

## Runtime overview

V001 now includes a minimal live X11 WM lifecycle:

1. Startup parses command bindings from config search paths.
2. Connects to X with `XOpenDisplay`.
3. Attempts to claim WM control on root (`SubstructureRedirectMask`).
4. Registers key grabs from `MimicCommandRegistry`.
5. Scans existing mapped windows and manages them.
6. Runs a persistent X event loop.

## Core modules in live path

## `MimicCommandRegistry`
- Parses `bind <combo> exec <command>`
- Stores bindings used to register root key grabs
- Dispatches commands when matching `KeyPress` arrives

## `MimicWorkspaceModel`
- Maintains workspace list + active workspace index
- Preserves invariant: at least one workspace always exists
- Tracks per-workspace window counts when clients map/unmap

## `MimicLayoutEngine` / `MimicViewport`
- Tracks ordered windows for active workspace
- Maintains focus iteration state
- Applies deterministic tiling-like layout in runtime
- Viewport offsets are now part of real layout application path

## `MimicOverviewController`
- Keeps overview state machine and tile metadata model
- Receives live managed-window metadata when active
- Ready for future visible overview rendering path

## X11 event handling scope

Current handled events:
- `MapRequest`
- `ConfigureRequest`
- `DestroyNotify`
- `UnmapNotify`
- `KeyPress`

This is intentionally minimal but forms a real WM session base for future Fluxbox-derived behavior.
