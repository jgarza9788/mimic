# Mimic first-wave architecture

The first wave introduces separable modules so scrolling behavior can be added incrementally without scattering logic.

## Modules

## `MimicCommandRegistry`
- Parses command-binding lines (`bind <combo> exec <command>`)
- Stores validated bindings and supports lookup by key combo
- Designed to become adapter-backed for TOML or legacy format parsers

## `MimicWorkspaceModel`
- Owns workspace list and active workspace index
- Encapsulates policy: remove empty workspace only if total workspaces > 1
- Guarantees startup invariant of at least one workspace

## `MimicLayoutEngine` and `MimicViewport`
- Holds ordered windows per workspace slice
- Provides next/previous focus navigation in order
- Tracks viewport offset (`offset_x`, `offset_y`) as basis for virtual scrolling strip

## `MimicOverviewController`
- Dedicated overview state machine (`Inactive`/`Active`)
- Builds simple grid tiles for visible windows
- Handles pick-to-focus and clean exit transition

## Integration status

Current `main.cpp` wires construction/bootstrap and config parsing with dry-mode behavior when no X server is available. Full X event-loop integration is intentionally left as a focused TODO.
