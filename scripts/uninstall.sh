#!/usr/bin/env bash
set -euo pipefail

sudo rm -f /usr/local/bin/mimic /usr/local/bin/mimic-msg
sudo rm -rf /usr/local/share/mimic
sudo rm -rf /usr/local/share/doc/mimic

if [[ -n "${SUDO_USER:-}" ]]; then
    target_user="${SUDO_USER}"
else
    target_user="$(id -un)"
fi

target_home="$(eval echo "~${target_user}")"
rm -f "${target_home}/.config/mimic/conf.toml"
rmdir --ignore-fail-on-non-empty "${target_home}/.config/mimic" 2>/dev/null || true
