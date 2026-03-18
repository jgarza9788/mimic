# Demo assets and troubleshooting guide

## Demo recordings/screenshots checklist

Use this list for release artifacts:

- Startup from display manager into ScrollWM session.
- Focus cycling and viewport movement (`Mod+j`, `Mod+k`).
- Workspace switching and move-to-workspace shortcuts.
- Runtime layout toggle (`Mod+Space`).
- Reorder and fullscreen behavior.
- Exit flow (`Mod+Shift+e`).

### Suggested capture tools

- Screen recording: `obs`, `wf-recorder` (XWayland), or `simplescreenrecorder`
- Animated clips: `peek`
- Still screenshots: `scrot`, `gnome-screenshot`, or desktop environment tooling

Store final assets under `docs/demo/` (or external release attachments) and link them in release notes.

## Troubleshooting

### ScrollWM does not appear in login session list

- Ensure `scrollwm.desktop` is installed into `/usr/share/xsessions`.
- Re-login or restart the display manager.
- Verify desktop file syntax:

```bash
desktop-file-validate /usr/share/xsessions/scrollwm.desktop
```

### Black screen or immediate session exit

- Start from a TTY and inspect stderr output:

```bash
scrollwm-session
```

- Confirm X11 dependencies are present (`xcb`, `xcb-keysyms`, `xcb-icccm`).

### Keybindings not working as expected

- Validate your config file schema:

```bash
./scripts/validate-config-schema.py ~/.config/scrollwm/config.toml
```

- Check modifier/key spelling in `[bindings]`.

### Compositor did not start

- Ensure config includes:

```toml
[autostart]
launch_picom = true
compositor = "picom --experimental-backends"
```

- Run the compositor command manually to verify it is installed and valid.
