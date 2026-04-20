# KNX open Developer Tool

Open-source alternative to the KNX-certified ETS (Engineering Tool Software).  
Configure, parametrize and program your self-built KNX-compatible devices — without vendor lock-in.

**License:** GPLv3 | **Platform:** Linux (Windows/macOS portability planned) | **UI:** Qt6/C++

---

## What it does

- **Topology editor** – define areas, lines and devices in your KNX installation
- **Group address editor** – 3-level structure (main/middle/sub), DPT assignment
- **Parameter & ComObject editor** – configure device parameters and link communication objects to group addresses
- **Programmer** – download configuration to devices via KNXnet/IP or USB interface
- **Bus monitor** – live telegram log with DPT decoding and filtering

## What it is NOT

- A replacement for knxd or any KNX IP-router firmware
- A KNX IP-router itself — it connects to an existing IP-router or USB interface
- A certified KNX tool — it targets open devices only

## Related repositories

| Repository | Role |
|---|---|
| **KNX-open-Developer-Tool** (this) | Qt6 configuration tool |
| KNX-open-Firmware-Stack *(planned)* | STM32 Arduino-like KNX firmware platform |

## Quick Start

### Ubuntu 24.04

```bash
sudo apt install cmake g++ qt6-base-dev qt6-tools-dev qt6-tools-dev-tools libqt6serialport6-dev
git clone https://github.com/judidudi/knx-open-developer-tool.git
cd knx-open-developer-tool
cmake -S . -B build
cmake --build build -j$(nproc)
./build/app/KNXOpenDeveloperTool
```

See [docs/development-setup.md](docs/development-setup.md) for full details.

## Architecture

See [docs/architecture.md](docs/architecture.md).

## Device Manifests

New device types are added by placing a YAML manifest in `catalog/devices/`.  
Format: [docs/device-manifest.md](docs/device-manifest.md)

## Contributing

Issues and PRs welcome. Please read [CLAUDE.md](CLAUDE.md) first — it explains the codebase structure, conventions and how to build.

## License

Copyright © 2026 OpenKNX contributors.  
This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
