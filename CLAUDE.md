# KNX open Developer Tool – Codebase Guide

## Project Overview

Open-source alternative to the KNX-certified ETS (Engineering Tool Software). Lets users configure and program KNX-compatible devices that run the open firmware stack (separate repository).

**License:** GPLv3  
**Primary OS:** Linux (Windows/macOS portability planned)

## Architecture

Two repositories:
- **This repo** – Qt6/C++ configuration tool (ETS replacement)
- **KNX-open-Firmware-Stack** (separate) – STM32 firmware platform

```
app/          Entry point (main.cpp), translations
src/core/     Data model: Project, Topology, GroupAddresses, Manifests, XML serializer
src/ui/       Qt Widgets: MainWindow, ProjectTree, Catalog, DeviceEditor, BusMonitor
src/knxip/    KNXnet/IP client (tunneling, discovery, CEMI)
src/usb/      USB KNX interface (Phase 6)
catalog/      YAML device manifests (one file per device type)
schemas/      JSON-Schema for manifest validation
tests/        QtTest unit tests
```

## Build

Requirements: Qt 6.5+, CMake 3.21+, C++20 compiler, internet (first build fetches yaml-cpp)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build
```

## Key Concepts

**Device Manifest** (`catalog/devices/*.yaml`): Describes a device type – channels, communication objects (ComObjects), parameters, DPT types, memory layout. Shared contract between this tool and the firmware stack.

**Project file** (`*.kodtproj`): XML file describing the installation – topology (areas/lines/devices), group addresses (3-level: main/middle/sub), parameter values, ComObject↔GA links.

**IKnxInterface**: Abstract base for both `KnxIpTunnelingClient` and `UsbKnxInterface`. All bus access goes through this interface.

**Group address format**: 3-level `main/middle/sub` (5/3/8 bit), e.g. `0/0/1`.

## Code Style

- C++20, Qt6
- `tr()` for every user-visible string – no hardcoded UI text
- No raw pointers for ownership; use `std::unique_ptr` / `std::shared_ptr`
- Qt signal/slot for async communication between layers
- No global state

## Adding a New Device Type

Create `catalog/devices/<your-device>.yaml` following the schema in `schemas/device-manifest.schema.yaml`. No code changes needed for the tool to pick it up.

## Running Tests

```bash
ctest --test-dir build --output-on-failure
```
