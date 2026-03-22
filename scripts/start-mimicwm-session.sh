#!/usr/bin/env sh
set -eu

CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/mimicwm"
USER_TOML="$CONFIG_DIR/config.toml"
SYSTEM_TOML="/usr/local/share/mimicwm/config.toml"

pick_toml() {
    if [ -f "$USER_TOML" ]; then
        printf '%s\n' "$USER_TOML"
    elif [ -f "$SYSTEM_TOML" ]; then
        printf '%s\n' "$SYSTEM_TOML"
    else
        printf '%s\n' ""
    fi
}

read_toml_value() {
    file="$1"
    section="$2"
    key="$3"

    awk -v section="$section" -v key="$key" '
        BEGIN { in_section = 0 }
        /^[[:space:]]*\[/ {
            in_section = ($0 ~ "^[[:space:]]*\\[" section "\\][[:space:]]*$")
            next
        }
        in_section && $0 ~ "^[[:space:]]*" key "[[:space:]]*=" {
            line = $0
            sub(/^[^=]*=[[:space:]]*/, "", line)
            gsub(/["[:space:]]+$/, "", line)
            gsub(/^["[:space:]]+/, "", line)
            print line
            exit
        }
    ' "$file"
}

expand_path() {
    value="$1"
    case "$value" in
        ~/*)
            printf '%s\n' "$HOME/${value#~/}"
            ;;
        *)
            printf '%s\n' "$value"
            ;;
    esac
}

TOML_FILE="$(pick_toml)"
TERMINAL=""
MENU=""
PICOM_BACKEND="auto"
PICOM_CFG="$CONFIG_DIR/picom.conf"

if [ -n "$TOML_FILE" ]; then
    TERMINAL="$(read_toml_value "$TOML_FILE" "commands" "terminal" || true)"
    MENU="$(read_toml_value "$TOML_FILE" "commands" "menu" || true)"
    backend_value="$(read_toml_value "$TOML_FILE" "picom" "backend" || true)"
    cfg_value="$(read_toml_value "$TOML_FILE" "picom" "config" || true)"

    if [ -n "$backend_value" ]; then
        PICOM_BACKEND="$backend_value"
    fi
    if [ -n "$cfg_value" ]; then
        PICOM_CFG="$(expand_path "$cfg_value")"
    fi
fi

if command -v picom >/dev/null 2>&1; then
    if [ "$PICOM_BACKEND" = "auto" ]; then
        if [ -f "$PICOM_CFG" ]; then
            picom -b --backend glx --config "$PICOM_CFG" >/dev/null 2>&1 ||                 picom -b --backend xrender --config "$PICOM_CFG" >/dev/null 2>&1 || true
        else
            picom -b --backend glx >/dev/null 2>&1 ||                 picom -b --backend xrender >/dev/null 2>&1 || true
        fi
    else
        if [ -f "$PICOM_CFG" ]; then
            picom -b --backend "$PICOM_BACKEND" --config "$PICOM_CFG" >/dev/null 2>&1 || true
        else
            picom -b --backend "$PICOM_BACKEND" >/dev/null 2>&1 || true
        fi
    fi
fi

if [ -n "$TERMINAL" ]; then
    export MIMICWM_TERMINAL="$TERMINAL"
fi
if [ -n "$MENU" ]; then
    export MIMICWM_MENU="$MENU"
fi

exec mimicwm
