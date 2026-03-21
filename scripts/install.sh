#!/usr/bin/env bash
set -euo pipefail

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
sudo cmake --install build

if [[ -n "${SUDO_USER:-}" ]]; then
    target_user="${SUDO_USER}"
else
    target_user="$(id -un)"
fi

target_home="$(eval echo "~${target_user}")"
target_config_dir="${target_home}/.config/mimic"
target_config_path="${target_config_dir}/config.toml"

mkdir -p "${target_config_dir}"
if [[ ! -f "${target_config_path}" ]]; then
    cp examples/conf.toml "${target_config_path}"
fi

if [[ "$(id -un)" == "root" ]]; then
    chown -R "${target_user}:${target_user}" "${target_config_dir}"
fi
