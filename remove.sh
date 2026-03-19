#!/usr/bin/env bash
set -euo pipefail

PREFIX="${PREFIX:-/usr/local}"
BUILD_DIR="${BUILD_DIR:-build}"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

log() {
  printf '[scrollwm-remove] %s\n' "$*"
}

run_as_root() {
  if [[ "$(id -u)" -eq 0 ]]; then
    "$@"
  elif command -v sudo >/dev/null 2>&1; then
    sudo "$@"
  else
    log 'ERROR: need root privileges to remove installed files.'
    exit 1
  fi
}

ensure_build_dir() {
  if [[ ! -d "$PROJECT_ROOT/$BUILD_DIR" ]]; then
    log "ERROR: build directory '$BUILD_DIR' does not exist."
    log 'Run meson setup/build first so installed files can be discovered.'
    exit 1
  fi
}

uninstall_project_files() {
  local installed_json
  mapfile -t installed_files < <(
    cd "$PROJECT_ROOT"
    meson introspect "$BUILD_DIR" --installed | python3 -c '
import json, sys
paths = json.load(sys.stdin)
for path in paths.values():
    print(path)
'
  )

  if [[ "${#installed_files[@]}" -eq 0 ]]; then
    log 'No installed files were reported by Meson. Nothing to remove.'
    return
  fi

  log "removing ${#installed_files[@]} installed file(s)"
  local path
  for path in "${installed_files[@]}"; do
    if [[ -e "$path" || -L "$path" ]]; then
      run_as_root rm -f "$path"
      log "removed $path"
    else
      log "skipped missing $path"
    fi
  done
}

cleanup_empty_dirs() {
  local dirs=(
    "$PREFIX/bin"
    "$PREFIX/share/xsessions"
    "$PREFIX/share/doc/scrollwm"
  )

  log 'cleaning up empty directories created during install'
  local dir
  for dir in "${dirs[@]}"; do
    if [[ -d "$dir" ]]; then
      run_as_root rmdir --ignore-fail-on-non-empty -p "$dir" 2>/dev/null || true
    fi
  done
}

main() {
  ensure_build_dir
  uninstall_project_files
  cleanup_empty_dirs
  log 'done: removed files installed by meson install for this project'
  log 'note: distro packages/dependencies installed by bootstrap are not removed'
}

main "$@"
