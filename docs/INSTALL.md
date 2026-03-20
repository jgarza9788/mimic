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
