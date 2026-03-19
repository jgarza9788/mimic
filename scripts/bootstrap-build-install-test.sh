#!/usr/bin/env bash
set -euo pipefail

PREFIX="${PREFIX:-/usr/local}"
BUILD_DIR="${BUILD_DIR:-build}"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

log() {
  printf '[scrollwm-bootstrap] %s\n' "$*"
}

run_as_root() {
  if [[ "$(id -u)" -eq 0 ]]; then
    "$@"
  elif command -v sudo >/dev/null 2>&1; then
    sudo "$@"
  else
    log "ERROR: need root privileges for package installation."
    exit 1
  fi
}

install_requirements() {
  log 'installing build/runtime requirements'

  if command -v apt-get >/dev/null 2>&1; then
    run_as_root apt-get update
    run_as_root apt-get install -y \
      build-essential \
      meson \
      ninja-build \
      pkg-config \
      libxcb1-dev \
      libxcb-util-dev \
      libxcb-keysyms1-dev \
      libxcb-icccm4-dev \
      libx11-dev \
      xterm \
      picom \
      xinit
    return
  fi

  if command -v dnf >/dev/null 2>&1; then
    run_as_root dnf install -y \
      gcc-c++ \
      meson \
      ninja-build \
      pkgconf-pkg-config \
      libxcb-devel \
      xcb-util-devel \
      xcb-util-keysyms-devel \
      xcb-util-wm-devel \
      libX11-devel \
      xterm \
      picom \
      xorg-x11-xinit
    return
  fi

  if command -v pacman >/dev/null 2>&1; then
    run_as_root pacman -Syu --noconfirm \
      base-devel \
      meson \
      ninja \
      pkgconf \
      libxcb \
      xcb-util \
      xcb-util-keysyms \
      xcb-util-wm \
      xorg-server \
      libx11 \
      xterm \
      picom \
      xorg-xinit
    return
  fi

  if command -v zypper >/dev/null 2>&1; then
    run_as_root zypper --non-interactive install \
      gcc-c++ \
      meson \
      ninja \
      pkgconf-pkg-config \
      libxcb-devel \
      xcb-util-keysyms-devel \
      xcb-util-wm-devel \
      libX11-devel \
      xterm \
      picom \
      xinit
    return
  fi

  log 'ERROR: unsupported package manager; install dependencies manually.'
  exit 1
}

build_project() {
  log 'configuring and building project'
  cd "$PROJECT_ROOT"
  meson setup "$BUILD_DIR" --prefix "$PREFIX" --buildtype debugoptimized --reconfigure
  meson compile -C "$BUILD_DIR"
}

install_project() {
  log 'installing project files'
  cd "$PROJECT_ROOT"
  run_as_root meson install -C "$BUILD_DIR"
}

run_tests() {
  log 'running unit tests'
  cd "$PROJECT_ROOT"
  meson test -C "$BUILD_DIR" --print-errorlogs

  log 'running binary smoke checks'
  "$BUILD_DIR/scrollwm" --help >/dev/null
  "$BUILD_DIR/scrollwm" --version >/dev/null
}

main() {
  install_requirements
  build_project
  install_project
  run_tests
  log 'done: dependencies installed, project built/installed, tests passed'
}

main "$@"
