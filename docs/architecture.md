# Architecture

## Overview

```
┌──────────────────────────────────────────────────────────────────┐
│  KNX open Developer Tool (Qt6/C++)                               │
│                                                                  │
│  app/main.cpp                                                    │
│       │                                                          │
│  src/ui/MainWindow ──────────────────────────────────────────┐   │
│       ├── ProjectTreeWidget  (QAbstractItemModel)            │   │
│       ├── CatalogWidget      (loads DeviceCatalog)           │   │
│       ├── DeviceEditorWidget (Parameter + ComObject tabs)    │   │
│       ├── BusMonitorWidget   (live telegram log)             │   │
│       └── ProgramDialog      (step-by-step download)         │   │
│                                                              │   │
│  src/core/  ──────────────────────────────────────────────── │   │
│       Project                                                │   │
│       ├── TopologyNode  (Area → Line → DeviceInstance)       │   │
│       ├── GroupAddress  (3-level: main/middle/sub)           │   │
│       └── DeviceInstance → Manifest (from DeviceCatalog)     │   │
│                                                              │   │
│  src/knxip/ ──────────────────────────────────────────────── │   │
│       IKnxInterface ◄──── KnxIpTunnelingClient               │   │
│                    ◄──── UsbKnxInterface  (Phase 6)          │   │
└──────────────────────────────────────────────────────────────┴───┘
                              │
                    KNXnet/IP (UDP/TCP)  or  USB HID
                              │
             ┌────────────────┴──────────────────┐
             │  KNX Bus (TP or IP)               │
             │    STM32 Device (Firmware Stack)  │
             └───────────────────────────────────┘
```

## Project File Format (*.kodtproj)

XML file. Structure:

```
KodtProject
├── ProjectInfo (Name, Created)
├── Topology
│   └── Area[]
│       └── Line[]
│           └── Device[] (physAddr, catalogRef, Parameters, ComObjectLinks)
└── GroupAddresses
    └── MainGroup[]
        └── MiddleGroup[]
            └── GroupAddress[] (id, name, dpt)
```

## Device Manifest Format (YAML)

`catalog/devices/*.yaml` – one file per device type. See `docs/device-manifest.md`.

## Programming Sequence

```
Tool                         KNX-IP-Router         Device
 │                                │                   │
 │──CONNECT_REQUEST──────────────►│                   │
 │◄─CONNECT_RESPONSE─────────────│                   │
 │                                │                   │
 │  [Button press on device → programming mode]       │
 │──A_IndividualAddress_Write────►│──────────────────►│
 │──A_Restart────────────────────►│──────────────────►│
 │  (wait 3s)                     │                   │
 │──A_Memory_Write (params)───────►│──────────────────►│
 │──A_Restart────────────────────►│──────────────────►│
 │                                │                   │
```

## Bus Monitor Data Flow

```
Device──L_Data.Ind──►Router──TUNNEL_REQUEST──►KnxIpTunnelingClient
                                                      │
                                              cemiFrameReceived()
                                                      │
                                           BusMonitorWidget (table row)
```
