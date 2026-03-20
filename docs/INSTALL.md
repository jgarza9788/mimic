# Installing Mimic

## Dependencies
- CMake >= 3.16
- C++17 compiler
- X11 development libraries (`libX11`)

## Build + test

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Install

```bash
cmake --install build --prefix /usr/local
```

Installed artifacts:
- `/usr/local/bin/mimic`
- `/usr/local/share/xsessions/mimic.desktop`
- `/usr/local/share/mimic/examples/mimic.keys`

## Start from xinit

Use `~/.xinitrc`:

```bash
exec mimic
```

## Start from display manager

1. Ensure `mimic.desktop` is installed under an active xsessions path.
2. Log out.
3. Select the **Mimic** session.
4. Log in.

`mimic.desktop` currently uses:
- `Name=Mimic`
- `Exec=mimic`

## Runtime logs

Mimic logs to stderr and also to:
- `$XDG_RUNTIME_DIR/mimic.log` when available, else
- `/tmp/mimic.log`

## Config path search order

1. CLI argument path
2. `$XDG_CONFIG_HOME/mimic/mimic.keys`
3. `~/.config/mimic/mimic.keys`
4. `/etc/xdg/mimic/mimic.keys`
5. `/usr/local/share/mimic/examples/mimic.keys`
6. `/usr/share/mimic/examples/mimic.keys`

Mimic does not fail startup when config is missing; it logs and continues.
