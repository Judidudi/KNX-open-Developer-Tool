# Development Setup

## Requirements

| Tool | Min. Version | Install |
|---|---|---|
| CMake | 3.21 | `apt install cmake` |
| Qt6 | 6.5 | `apt install qt6-base-dev qt6-tools-dev qt6-tools-dev-tools` |
| C++ Compiler | GCC 12 / Clang 15 | `apt install g++` |
| Git | any | `apt install git` |
| yaml-cpp | fetched automatically | – |

On Ubuntu 24.04:
```bash
sudo apt update
sudo apt install cmake g++ qt6-base-dev qt6-tools-dev qt6-tools-dev-tools \
                 libqt6serialport6-dev
```

## Build (Debug)

```bash
git clone https://github.com/judidudi/knx-open-developer-tool.git
cd knx-open-developer-tool
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

## Run

```bash
./build/app/KNXOpenDeveloperTool
```

## Tests

```bash
ctest --test-dir build --output-on-failure
```

## Generate/Update Translations

```bash
# Extract strings from source (generates/updates .ts files)
cd build && cmake --build . --target update_translations

# Compile .ts → .qm (done automatically during build)
cmake --build . --target release_translations
```

## Project File Location

Default catalog directory: `~/.config/OpenKNX/KNXOpenDeveloperTool/catalog/`  
Override with env var: `KNXODT_CATALOG_PATH`

## Architecture Notes

See [CLAUDE.md](../CLAUDE.md) for a concise overview.  
See [architecture.md](architecture.md) for detailed sequence diagrams.
