# wifiscan

A Linux terminal app that scans for nearby WiFi networks and displays them in a spectrum & list view.

<img width="1706" height="1172" alt="image" src="https://github.com/user-attachments/assets/091a97f0-271a-4a0c-8e68-c18423bc4172" />

## How to use:

```bash
# Running as an unpriviledged user will show just the networks that are on the same channel.
# To be able to scan channels, the program needs to have `CAP_NET_ADMIN` / root permissions:
sudo wifiscan

# By default the program uses the first wifi interface. To specify an interface explicitly:
sudo wifiscan wlan0

# Non-interactive mode performs a single scan and dumps the network list to a terminal:
sudo wifiscan --non-interactive

# Graphical mode opens a Dear ImGui window (requires a display; use sudo -E to keep $DISPLAY):
sudo -E wifiscan --gui
```

## Requirements

- GCC 12+ (C++20)
- CMake 3.22+
- libnl: `sudo apt install libnl-3-dev libnl-genl-3-dev`
- Root / `CAP_NET_ADMIN` to read scan results
- For the optional `--gui` mode (built by default): GLFW + OpenGL — `sudo apt install libglfw3-dev`. Disable with `-DWIFISCAN_ENABLE_GUI=OFF` for a terminal-only build with no graphics dependencies.

## Install

```bash
# Clone with submodules (includes FTXUI, fmt and Dear ImGui)
git clone --recurse-submodules https://github.com/lukas-hosek/wifiscan.git
cd wifiscan

# Build (Release) and install to /usr/local/bin
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build
```

## Development build

```bash
# If you already cloned without --recurse-submodules
git submodule update --init --recursive

# Configure with debug symbols and compile commands for tooling
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
cmake --build build -j$(nproc)

# Run directly from the build directory
sudo ./build/wifiscan
```

## License

MIT — see [LICENSE](LICENSE).

