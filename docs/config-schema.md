# Config schema and migration notes

ScrollWM now defines a documented config schema with an optional version table:

```toml
[schema]
version = 1
```

If omitted, ScrollWM assumes `version = 1` for backward compatibility.

## Schema v1 sections

- `[schema]`: `version` (int, must be `1`)
- `[general]`: `mod_key` (string), `workspace_count` (int, initial workspace count), `focus_follows_mouse` (bool), `terminal` (string)
- `[layout]`: `layout_direction` (`"horizontal"` or `"vertical"`)
- `[appearance]`: `gap` (int), `border_width` (int), `outer_padding` (int)
- `[autostart]`: `launch_picom` (bool), `compositor` (string)
- `[bindings]`: keybinding strings for all WM actions, including dynamic `workspace_N` and `move_to_workspace_N` keys
- `[[exec]]`: array-of-tables entries with `key` (string) and `command` (string) for arbitrary executable keybindings

## Validation

Validate files with:

```bash
./scripts/validate-config-schema.py path/to/config.toml
```

## Migration notes

### Legacy configs (pre-schema table)

No migration is required. Existing config files without `[schema]` remain valid and are interpreted as schema v1.

### Future schema changes

When introducing schema v2+:

1. Add parser compatibility shims for prior versions.
2. Extend `scripts/validate-config-schema.py` to accept/transform known legacy keys.
3. Add a `docs/migrations/vX-to-vY.md` note describing exact key renames/default changes.
4. Include at least one unit test fixture per supported schema version.
