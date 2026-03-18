# ScrollWM Milestones

## M0 - Vertical slice (current)
- [x] Meson + Ninja project setup
- [x] X11 WM ownership and event loop
- [x] Basic client lifecycle (map/unmap/destroy)
- [x] Scrollable linear tiling layout
- [x] Focus next/previous keyboard navigation
- [x] Workspace switching (4 default workspaces)
- [x] TOML-like config loading from `~/.config/scrollwm/config.toml`
- [x] Session integration for display managers via `xsessions` desktop entry
- [x] Session wrapper that can optionally start picom
- [x] Basic unit tests for config/layout/workspace logic

## M1 - Desktop usability hardening
- [x] Improve ICCCM/EWMH coverage (`_NET_SUPPORTED`, `_NET_WM_STATE`, desktop names)
- [x] Better handling for transient/dialog/floating windows
- [x] Respect size hints and minimum sizes
- [x] Implement move window to workspace shortcuts
- [x] Configurable keybinding parser from TOML

## M2 - Layout and multi-monitor evolution
- [x] Per-monitor workspace/view model
- [x] Vertical + horizontal scrolling modes with runtime toggle
- [x] Window reorder shortcuts
- [x] Fullscreen semantics and restore behavior
- [x] Better focus history and urgent window handling

## M3 - Packaging and ops
- [ ] man page (`scrollwm(1)`)
- [ ] Dist package scaffolding
- [ ] CI build + static analysis
- [ ] Config schema validation and migration notes
- [ ] Demo recordings/screenshots and troubleshooting guide
