# wifiscan

A C++ terminal app that scans for nearby WiFi networks and displays them in a spectrum & list view.

<img width="1706" height="1172" alt="image" src="https://github.com/user-attachments/assets/091a97f0-271a-4a0c-8e68-c18423bc4172" />

## How to use:

```bash
# Running as an unpriviledged user will show just the networks that are on the same channel.
# To be able to scan channels, the program needs to have `CAP_NET_ADMIN` / root permissions:
sudo ./wifiscan

# By default the program uses the first wifi interface. To specify an interface explicitly:
sudo ./wifiscan wlan0
```


## Requirements

- GCC 14+ (for C++23 `std::print`, `std::format`, `std::jthread`)
- CMake 3.28+
- libnl: `sudo apt install libnl-3-dev libnl-genl-3-dev`
- Root / `CAP_NET_ADMIN` to read scan results

## Clone and build

```bash
# Clone with submodules (includes FTXUI)
git clone --recurse-submodules https://github.com/lukas-hosek/wifiscan.git
cd wifiscan

# If you already cloned without --recurse-submodules
git submodule update --init --recursive

# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build -j$(nproc)
```

## License

MIT — see [LICENSE](LICENSE).

