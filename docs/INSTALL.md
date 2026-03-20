# Installing Mimic

## Dependencies
- CMake >= 3.16
- C++17 compiler
- X11 development libraries (`libX11`)

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Test

```bash
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

## One-command system install

To build and install Mimic with display-manager/session setup and default config provisioning:

```bash
sudo ./scripts/build_install.sh --prefix /usr/local --sysconfdir /etc
```

What this command does:
- configures + builds the app
- runs tests (unless `--skip-tests`)
- installs `mimic` into `<prefix>/bin`
- installs `mimic.desktop` for display managers in `<prefix>/share/xsessions` and also `/usr/share/xsessions` when writable
- creates `/etc/xdg/mimic/mimic.keys` if it does not exist
