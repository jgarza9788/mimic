#!/usr/bin/env python3
"""Validate ScrollWM config.toml schema."""

from __future__ import annotations

import argparse
import pathlib
import re
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
        "toggle_layout_direction": str,
        "reorder_next": str,
        "reorder_prev": str,
        "toggle_fullscreen": str,
    },
}

DYNAMIC_BINDING_PATTERNS = (
    re.compile(r"^workspace_[1-9]\d*$"),
    re.compile(r"^move_to_workspace_[1-9]\d*$"),
)


def _fail(message: str) -> None:
    print(f"schema validation failed: {message}", file=sys.stderr)
    raise SystemExit(1)


def validate(path: pathlib.Path) -> None:
    data = tomllib.loads(path.read_text(encoding="utf-8"))

    for section_name, section_data in data.items():
        if section_name not in ALLOWED_SCHEMA:
            if section_name != "exec":
                _fail(f"unknown section [{section_name}]")
            if not isinstance(section_data, list):
                _fail("section [[exec]] must be an array of tables")
            for i, entry in enumerate(section_data, start=1):
                if not isinstance(entry, dict):
                    _fail(f"entry [[exec]] #{i} must be a table")
                if set(entry.keys()) != {"key", "command"}:
                    _fail(f"entry [[exec]] #{i} must only include key and command")
                if not isinstance(entry["key"], str):
                    _fail(f"entry [[exec]] #{i} key must be a string")
                if not isinstance(entry["command"], str):
                    _fail(f"entry [[exec]] #{i} command must be a string")
            continue
        if not isinstance(section_data, dict):
            _fail(f"section [{section_name}] must be a table")

        allowed_keys = ALLOWED_SCHEMA[section_name]
        for key, value in section_data.items():
            if key not in allowed_keys:
                if section_name == "bindings" and any(
                    pattern.fullmatch(key) for pattern in DYNAMIC_BINDING_PATTERNS
                ):
                    if not isinstance(value, str):
                        _fail(f"key {section_name}.{key} expected {str} but got {type(value)}")
                    continue
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
