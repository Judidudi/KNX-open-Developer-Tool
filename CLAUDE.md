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
src/usb/      USB KNX interface (Phase 6)
catalog/      YAML device manifests (authoring format; auto-converted to .knxprod)
schemas/      JSON-Schema for YAML manifest validation
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

**Device Manifest** (`catalog/devices/*.yaml`): YAML authoring format for firmware developers.
Describes channels, ComObjects, parameters, DPT types, and memory layout.
On first tool launch, each `.yaml` without a matching `.knxprod` is auto-converted by
`YamlToKnxprod`. The `.yaml` is the shared contract between this tool and the firmware stack.

**KNX Application Program** (`KnxApplicationProgram`): In-memory representation of a device's
application program, loaded from `.knxprod`. Contains `KnxParameterType`, `KnxParameter`,
`KnxComObject`, `KnxMemoryLayout`. Shared between the catalog, device editor, and programmer.

**`.knxprod` catalog** (`KnxprodCatalog`): Scans directories for `.knxprod` files (KNX standard
ZIP+XML format, ETS 6 compatible). The Catalog tab lists all loaded products.

**Project file** (`*.knxproj`): KNX standard ZIP+XML format, ETS 6 compatible.
Contains topology (areas/lines/devices), group addresses (3-level: main/middle/sub),
parameter values, and ComObject↔GA links. Written/read by `KnxprojSerializer`.

**IKnxInterface**: Abstract base for both `KnxIpTunnelingClient` and `UsbKnxInterface`.
All bus access goes through this interface.

**Group address format**: 3-level `main/middle/sub` (5/3/8 bit), e.g. `0/0/1`.

## Code Style

- C++20, Qt6
- `tr()` for every user-visible string – no hardcoded UI text
- No raw pointers for ownership; use `std::unique_ptr` / `std::shared_ptr`
- Qt signal/slot for async communication between layers
- No global state

## Adding a New Device Type

Create `catalog/devices/<your-device>.yaml` following the schema in
`schemas/device-manifest.schema.yaml`. On next tool launch it will be auto-converted
to `<your-device>.knxprod` in the same directory and appear in the Catalog tab.
No code changes required.

Alternatively, drop any standard-format `.knxprod` file directly into the catalog
directory — it loads without YAML conversion.

## Running Tests

```bash
ctest --test-dir build --output-on-failure
```
