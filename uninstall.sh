#!/usr/bin/env sh
set -eu

PREFIX="${PREFIX:-/usr/local}"
BINDIR="$PREFIX/bin"
SHAREDIR="$PREFIX/share"
WM_NAME="mimicwm"

if [ "$(id -u)" -ne 0 ]; then
  echo "[mimicwm] uninstall.sh should be run as root (sudo ./uninstall.sh)"
  exit 1
fi

removed=0
remove_path() {
  if [ -e "$1" ]; then
    rm -rf "$1"
    echo "[mimicwm] removed $1"
    removed=$((removed + 1))
  fi
}

remove_path "$BINDIR/$WM_NAME"
remove_path "$SHAREDIR/xsessions/mimicwm.desktop"
remove_path "$SHAREDIR/$WM_NAME"

echo "[mimicwm] Removed $removed installed path(s)."
echo "[mimicwm] User files were not removed except files created by install.sh in ~/.config/mimicwm when present."
