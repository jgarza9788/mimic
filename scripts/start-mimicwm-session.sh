#!/usr/bin/env sh
set -eu

# Optional user config
USER_CONF="${XDG_CONFIG_HOME:-$HOME/.config}/mimicwm/config"
if [ -f "$USER_CONF" ]; then
    # shellcheck disable=SC1090
    . "$USER_CONF"
fi

PICOM_CFG="${PICOM_CONFIG:-${XDG_CONFIG_HOME:-$HOME/.config}/mimicwm/picom.conf}"
if command -v picom >/dev/null 2>&1; then
    if [ -f "$PICOM_CFG" ]; then
        picom --config "$PICOM_CFG" &
    else
        picom &
    fi
fi

exec mimicwm
