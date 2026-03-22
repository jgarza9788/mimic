# Xlib → XCB Migration Notes (V003)

This document captures a practical, low-risk migration plan for porting `mimicwm` from Xlib to XCB while preserving existing behavior.

## Current Xlib Dependencies

Primary Xlib touchpoints live in:

- `src/wm.c` (event loop, atoms, window ops, grabs, focus, ICCCM interactions)
- `include/wm.h` (`Display*`, `Window`, `Atom`)
- `include/config.h` (`Mod4Mask`)
- `Makefile` (`-lX11 -lXext`)

## Incremental Migration Strategy

1. **Type migration layer**
   - Move WM core types to XCB (`xcb_connection_t*`, `xcb_window_t`, `xcb_atom_t`, `xcb_generic_event_t*`).
   - Keep layout/workspace/client algorithms unchanged.

2. **Atom + property abstraction**
   - Add helper functions for:
     - intern atom
     - set/get CARDINAL/WINDOW/ATOM props
     - delete props
   - Replace all `XChangeProperty`, `XGetWindowProperty`, `XDeleteProperty` calls with helpers.

3. **Event loop conversion**
   - Replace `XNextEvent` with `xcb_wait_for_event`.
   - Convert Xlib events to XCB structs:
     - `XKeyEvent` → `xcb_key_press_event_t`
     - `XMapRequestEvent` → `xcb_map_request_event_t`
     - `XConfigureRequestEvent` → `xcb_configure_request_event_t`
     - etc.

4. **Window operations**
   - Replace map/unmap/configure/move/resize/focus/raise with `xcb_*` calls.
   - Normalize through helper wrappers to avoid repeated value-mask boilerplate.

5. **Keyboard handling**
   - Replace `XLookupKeysym` and `XKeysymToKeycode` with `xcb-keysyms` equivalents.
   - Preserve existing keybinding behavior and numlock/lock-mask cleaning.

6. **ICCCM/EWMH**
   - Replace WM hints and protocol handling with `xcb-icccm` + raw client messages.
   - Keep support for `_NET_ACTIVE_WINDOW`, `_NET_WM_STATE_FULLSCREEN`, desktop properties.

7. **Build system update**
   - Replace Xlib linker flags with:
     - `-lxcb`
     - `-lxcb-keysyms`
     - `-lxcb-icccm`
     - `-lxcb-ewmh` (optional helper lib)

8. **Validation checklist**
   - launch, map/unmap, focus changes
   - move/resize via keyboard + mouse drag
   - fullscreen toggle
   - minimize/restore
   - workspace switching and send-to-workspace
   - floating/tiled toggle
   - picom external compatibility

## Suggested Execution Order

- First pass: compile with mixed wrappers + XCB connection/event loop.
- Second pass: remove all Xlib headers/types.
- Third pass: remove remaining Xlib function shims and run regression checks.
