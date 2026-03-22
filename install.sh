#!/usr/bin/env sh
set -eu

PREFIX="${PREFIX:-/usr/local}"
BINDIR="$PREFIX/bin"
SHAREDIR="$PREFIX/share"
WM_NAME="mimicwm"

if [ "$(id -u)" -ne 0 ]; then
  echo "[mimicwm] install.sh should be run as root (sudo ./install.sh)"
  exit 1
fi

echo "[mimicwm] Building..."
make clean
make

echo "[mimicwm] Installing binary and session assets under $PREFIX"
install -d "$BINDIR" "$SHAREDIR/xsessions" "$SHAREDIR/$WM_NAME"
install -m 755 "$WM_NAME" "$BINDIR/$WM_NAME"
install -m 755 scripts/start-mimicwm-session.sh "$SHAREDIR/$WM_NAME/start-mimicwm-session.sh"
install -m 644 assets/mimicwm.desktop "$SHAREDIR/xsessions/mimicwm.desktop"
install -m 644 config/picom.conf "$SHAREDIR/$WM_NAME/picom.conf"
install -m 644 config/config.toml "$SHAREDIR/$WM_NAME/config.toml"

echo "[mimicwm] Installing default user config skeleton (if missing)..."
for home in /home/*; do
  [ -d "$home" ] || continue
  user_cfg_dir="$home/.config/mimicwm"
  if [ ! -e "$user_cfg_dir/picom.conf" ]; then
    install -d -m 755 "$user_cfg_dir"
    install -m 644 config/picom.conf "$user_cfg_dir/picom.conf"
  fi
  if [ ! -e "$user_cfg_dir/config.toml" ]; then
    install -d -m 755 "$user_cfg_dir"
    install -m 644 config/config.toml "$user_cfg_dir/config.toml"
  fi
done

echo
cat <<MSG
[mimicwm] Install complete.

Next steps:
1) Make sure dependencies are installed (xorg, libX11, picom, xterm or another terminal).
2) Log out.
3) In SDDM, choose "MimicWM" from the session list and log in.
4) Optional: edit ~/.config/mimicwm/config.toml and ~/.config/mimicwm/picom.conf.

Default key examples:
- Super+Enter: terminal
- Super+Q: close focused window
- Super+1..4: switch workspaces
- Super+Shift+1..4: move focused window to workspace
- Super+F: fullscreen
MSG
