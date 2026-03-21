# mimic architecture

`mimic` is split into focused components:

- `XcbBackend`: X11 integration (event subscription, focus, placement).
- `WindowManager`: orchestrates lifecycle and dispatch.
- `LayoutEngine`: horizontal strip model with centered focused window.
- `WorkspaceManager` + `MonitorManager`: monitor-local dynamic workspace state.
- `ConfigLoader` + `ConfigWatcher`: layered TOML and hot reload polling.
- `IpcServer` + `mimic-msg`: scripting and query command channel.
- `CompositorBridge`: future extension seam for picom metadata.

The first working slice is intentionally conservative: robust scaffolding with
one monitor, one active layout strip, and Super+h/Super+l navigation.
