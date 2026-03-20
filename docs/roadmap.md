# Mimic roadmap

## Milestone 1 (completed)
- Mimic identity/bootstrap
- Core architecture modules (`WorkspaceModel`, `LayoutEngine`, `OverviewController`, `CommandRegistry`)
- Config parser for `bind ... exec ...`
- Live X11 WM runtime with root claim + event loop + basic client management

## Milestone 2 (next recommended)
- Multi-workspace switching commands + key bindings
- EWMH basics (`_NET_ACTIVE_WINDOW`, `_NET_CLIENT_LIST`, etc.)
- Better focus model and pointer/enter interactions
- More robust client state transitions and edge-case handling
- Early integration points for Fluxbox-derived behavior

## Milestone 3
- Reparent/decorations where needed
- Scrolling strip workspace UX and smooth viewport motion
- Visual overview rendering and interaction
- Stronger runtime/integration tests (Xephyr/Xvfb-based)
