#!/usr/bin/env bash
set -euo pipefail

print_status() {
  local name="$1"
  local cmd="$2"
  if command -v "$cmd" >/dev/null 2>&1; then
    printf '  [OK]      %-20s %s\n' "$name" "$(command -v "$cmd")"
  else
    printf '  [MISSING] %-20s\n' "$name"
  fi
}

print_pkgcfg() {
  local module="$1"
  if pkg-config --exists "$module" 2>/dev/null; then
    printf '  [OK]      %-20s %s\n' "$module" "$(pkg-config --modversion "$module")"
  else
    printf '  [MISSING] %-20s\n' "$module"
  fi
}

echo "== Mimic dependency report =="
echo
echo "Required build tools"
print_status "cmake" cmake
print_status "c++" c++
print_status "make" make
print_status "ninja (optional gen)" ninja
print_status "pkg-config" pkg-config

echo
echo "Required dev libraries"
if command -v pkg-config >/dev/null 2>&1; then
  print_pkgcfg "x11"
else
  echo "  [MISSING] x11 (pkg-config unavailable)"
fi

echo
echo "Runtime dependencies"
print_status "Xorg server (Xorg)" Xorg
print_status "xinit/startx" xinit
print_status "shell (/bin/sh)" sh

echo
echo "Optional tools"
print_status "xterm" xterm
print_status "dmenu_run" dmenu_run
print_status "i3lock" i3lock
print_status "xvfb-run (headless)" xvfb-run
