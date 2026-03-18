#!/usr/bin/env python3
"""Validate ScrollWM config.toml schema."""

from __future__ import annotations

import argparse
import pathlib
import sys

if sys.version_info >= (3, 11):
    import tomllib
else:
    import tomli as tomllib

ALLOWED_SCHEMA: dict[str, dict[str, type | tuple[type, ...]]] = {
    "schema": {"version": int},
    "general": {
        "mod_key": str,
        "workspace_count": int,
        "focus_follows_mouse": bool,
        "terminal": str,
    },
    "layout": {"layout_direction": str},
    "appearance": {"gap": int, "border_width": int, "outer_padding": int},
    "autostart": {"launch_picom": bool, "compositor": str},
    "bindings": {
        "focus_next": str,
        "focus_prev": str,
        "spawn_terminal": str,
        "close_window": str,
        "exit_wm": str,
        "workspace_1": str,
        "workspace_2": str,
        "workspace_3": str,
        "workspace_4": str,
        "move_to_workspace_1": str,
        "move_to_workspace_2": str,
        "move_to_workspace_3": str,
        "move_to_workspace_4": str,
        "toggle_layout_direction": str,
        "reorder_next": str,
        "reorder_prev": str,
        "toggle_fullscreen": str,
    },
}


def _fail(message: str) -> None:
    print(f"schema validation failed: {message}", file=sys.stderr)
    raise SystemExit(1)


def validate(path: pathlib.Path) -> None:
    data = tomllib.loads(path.read_text(encoding="utf-8"))

    for section_name, section_data in data.items():
        if section_name not in ALLOWED_SCHEMA:
            _fail(f"unknown section [{section_name}]")
        if not isinstance(section_data, dict):
            _fail(f"section [{section_name}] must be a table")

        allowed_keys = ALLOWED_SCHEMA[section_name]
        for key, value in section_data.items():
            if key not in allowed_keys:
                _fail(f"unknown key {section_name}.{key}")
            expected_type = allowed_keys[key]
            if not isinstance(value, expected_type):
                _fail(
                    f"key {section_name}.{key} expected {expected_type} but got {type(value)}"
                )

    layout_direction = data.get("layout", {}).get("layout_direction")
    if layout_direction is not None and layout_direction not in {"horizontal", "vertical"}:
        _fail("layout.layout_direction must be 'horizontal' or 'vertical'")

    schema_version = data.get("schema", {}).get("version", 1)
    if schema_version != 1:
        _fail("schema.version must be 1")



def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", type=pathlib.Path, help="Path to config.toml file")
    args = parser.parse_args()
    validate(args.path)
    print(f"schema validation passed: {args.path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
