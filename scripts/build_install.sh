#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: build_install.sh [--build-dir DIR] [--prefix DIR] [--sysconfdir DIR] [--skip-tests]

Builds, tests, and installs Mimic for display-manager use.

Options:
  --build-dir DIR   CMake build directory (default: build)
  --prefix DIR      Install prefix for binaries/data (default: /usr/local)
  --sysconfdir DIR  System config root (default: /etc)
  --skip-tests      Skip ctest
  -h, --help        Show this help
USAGE
}

BUILD_DIR="build"
PREFIX="/usr/local"
SYSCONFDIR="/etc"
RUN_TESTS=1

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="${2:?missing value for --build-dir}"
      shift 2
      ;;
    --prefix)
      PREFIX="${2:?missing value for --prefix}"
      shift 2
      ;;
    --sysconfdir)
      SYSCONFDIR="${2:?missing value for --sysconfdir}"
      shift 2
      ;;
    --skip-tests)
      RUN_TESTS=0
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

printf '\n[0/4] Configuring and building Mimic...\n'
cmake -S . -B "$BUILD_DIR" -DCMAKE_INSTALL_PREFIX="$PREFIX"
cmake --build "$BUILD_DIR"

if [[ "$RUN_TESTS" -eq 1 ]]; then
  printf '\n[1/4] Running tests...\n'
  ctest --test-dir "$BUILD_DIR" --output-on-failure
else
  printf '\n[1/4] Skipping tests (--skip-tests).\n'
fi

printf '\n[2/4] Installing binaries, session file, and example config...\n'
cmake --install "$BUILD_DIR"

BIN_PATH="$PREFIX/bin/mimic"
XSESSIONS_DIR_PREFIX="$PREFIX/share/xsessions"
XSESSIONS_DIR_SYSTEM="/usr/share/xsessions"

if [[ -x "$BIN_PATH" ]]; then
  printf '\n[3/4] Installing display-manager session files (GDM/SDDM)...\n'
  TMP_DESKTOP="$(mktemp)"
  sed "s#^Exec=.*#Exec=$BIN_PATH#" sessions/mimic.desktop > "$TMP_DESKTOP"

  install -Dm644 "$TMP_DESKTOP" "$XSESSIONS_DIR_PREFIX/mimic.desktop"

  if [[ -w "$XSESSIONS_DIR_SYSTEM" || ! -d "$XSESSIONS_DIR_SYSTEM" && -w /usr/share ]]; then
    install -Dm644 "$TMP_DESKTOP" "$XSESSIONS_DIR_SYSTEM/mimic.desktop"
  else
    echo "Note: could not write $XSESSIONS_DIR_SYSTEM/mimic.desktop (need root)."
  fi

  rm -f "$TMP_DESKTOP"
else
  echo "Warning: expected binary not found at $BIN_PATH" >&2
fi

printf '\n[4/4] Installing default system config if missing...\n'
DEFAULT_CONFIG_DIR="$SYSCONFDIR/xdg/mimic"
DEFAULT_CONFIG_FILE="$DEFAULT_CONFIG_DIR/mimic.keys"

if [[ ! -f "$DEFAULT_CONFIG_FILE" ]]; then
  install -Dm644 config/mimic.keys "$DEFAULT_CONFIG_FILE"
  echo "Installed default config: $DEFAULT_CONFIG_FILE"
else
  echo "Kept existing config: $DEFAULT_CONFIG_FILE"
fi

if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "$XSESSIONS_DIR_PREFIX" || true
fi

echo
echo "Mimic install is ready."
echo "- Binary: $BIN_PATH"
echo "- Session files: $XSESSIONS_DIR_PREFIX/mimic.desktop and (if permitted) $XSESSIONS_DIR_SYSTEM/mimic.desktop"
echo "- Default config: $DEFAULT_CONFIG_FILE"
