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
src/core/     Data model: Project, Topology, GroupAddresses, KnxprodCatalog, serializers
src/ui/       Qt Widgets: MainWindow, ProjectTree, Catalog, DeviceEditor, BusMonitor
src/knxip/    KNXnet/IP client (tunneling, discovery, CEMI)
src/usb/      USB KNX interface + KnxdManager (knxd subprocess backend)
catalog/      .knxprod device files (KNX standard format, ETS 6 compatible)
tests/        QtTest unit tests
```

## Build

Requirements: Qt 6.5+, CMake 3.21+, C++20 compiler

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build
```

## Key Concepts

**`.knxprod` catalog** (`KnxprodCatalog`): Scans directories for `.knxprod` files
(KNX standard ZIP+XML format, ETS 6 compatible). Drop any `.knxprod` file into
`catalog/devices/` and it appears in the Catalog tab. Only `.knxprod` is supported;
YAML authoring format has been removed.

**KNX Application Program** (`KnxApplicationProgram`): In-memory representation of a device's
application program, loaded from `.knxprod`. Contains `KnxParameterType`, `KnxParameter`,
`KnxComObject`, `KnxMemoryLayout`. Shared between the catalog, device editor, and programmer.

**Project file** (`*.knxproj`): KNX standard ZIP+XML format, ETS 6 compatible.
Contains topology (areas/lines/devices), group addresses (3-level: main/middle/sub),
parameter values, and ComObject↔GA links. Written/read by `KnxprojSerializer`.

**IKnxInterface**: Abstract base for both `KnxIpTunnelingClient` and `UsbKnxInterface`.
All bus access goes through this interface.

**KnxdManager** (`src/usb/KnxdManager`): Detects, starts, and stops the `knxd` subprocess
as a USB transport backend. When `knxd` is installed, it is started automatically on USB
device plug-in and serves KNXnet/IP tunneling on localhost:3671. The existing
`KnxIpTunnelingClient` connects to this port — no protocol changes needed. When `knxd` is
not installed, `UsbKnxInterface` is used directly as a fallback.

**Group address format**: 3-level `main/middle/sub` (5/3/8 bit), e.g. `0/0/1`.

## Code Style

- C++20, Qt6
- `tr()` for every user-visible string – no hardcoded UI text
- No raw pointers for ownership; use `std::unique_ptr` / `std::shared_ptr`
- Qt signal/slot for async communication between layers
- No global state

## Adding a New Device Type

Drop a standard-format `.knxprod` file (e.g. exported from ETS or generated with
KNX manufacturer tools) into `catalog/devices/`. It loads automatically on next
tool launch without any code changes.

## Running Tests

```bash
ctest --test-dir build --output-on-failure
```
