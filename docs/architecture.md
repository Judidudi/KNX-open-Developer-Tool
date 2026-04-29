# Architecture

## Overview

```
┌────────────────────────────────────────────────────────────────────────┐
│  KNX open Developer Tool (Qt6/C++)                                     │
│                                                                        │
│  app/main.cpp  (QApplication + global stylesheet)                      │
│       │                                                                │
│  src/ui/MainWindow ──────────────────────────────────────────────────┐ │
│       ├── ProjectTreeWidget  (3-tab navigation panel, left)          │ │
│       │     ├── Tab "Topologie"  – QTreeView (Areas/Lines/Devices)   │ │
│       │     ├── Tab "Gruppen"    – QTreeView (Main/Mid/GroupAddress)  │ │
│       │     └── Tab "Katalog"    – CatalogWidget (.knxprod products) │ │
│       ├── QStackedWidget  (center)                                    │ │
│       │     ├── DeviceEditorWidget (Parameter + ComObject tabs)      │ │
│       │     └── BusMonitorWidget  (live telegram log, table model)   │ │
│       ├── PropertiesPanel  (QDockWidget, right)                      │ │
│       │     ├── Device view  – phys. address editor                  │ │
│       │     └── GroupAddress view – name + DPT editor                │ │
│       └── ProgramDialog  (step-by-step download, state machine)      │ │
│                                                                 │    │ │
│  src/core/  ────────────────────────────────────────────────── │    │ │
│       Project                                                   │    │ │
│       ├── TopologyNode  (Area → Line → DeviceInstance)          │    │ │
│       ├── GroupAddress  (3-level: main/middle/sub, DPT)         │    │ │
│       ├── DeviceInstance → KnxApplicationProgram                │    │ │
│       ├── KnxprodCatalog (scans dirs, loads .knxprod files)     │    │ │
│       ├── YamlToKnxprod  (YAML manifest → .knxprod ZIP)         │    │ │
│       └── KnxprojSerializer (*.knxproj ZIP+XML read/write)      │    │ │
│                                                                 │    │ │
│  src/knxip/ ────────────────────────────────────────────────── │    │ │
│       IKnxInterface (abstract)                                  │    │ │
│       ├── KnxIpTunnelingClient  (UDP KNXnet/IP tunneling)       │    │ │
│       ├── KnxIpDiscovery        (SEARCH_REQUEST multicast)      │    │ │
│       ├── CemiFrame             (encode/decode L_Data)          │    │ │
│       ├── TableBuilder          (address/assoc/param blocks)    │    │ │
│       ├── DeviceProgrammer      (KNX programming state machine) │    │ │
│       └── InterfaceManager      (owns active IKnxInterface)     │    │ │
│                                                                 │    │ │
│  src/usb/ ─────────────────────────────────────────────────── │    │ │
│       UsbKnxInterface  (IKnxInterface via Serial/HID)           │    │ │
│       └── ConnectInterfaceFactory (creates the right impl.)     │    │ │
└─────────────────────────────────────────────────────────────────┴────┴─┘
                              │
               KNXnet/IP (UDP/TCP)  or  USB (Serial CDC / HID)
                              │
             ┌────────────────┴──────────────────┐
             │  KNX TP Bus                        │
             │    STM32 Device (OpenKNX firmware) │
             └────────────────────────────────────┘
```

## UI Layout

```
┌──────────────────────────────────────────────────────────────────────────┐
│  Datei | Projekt | Bus | Hilfe                   [Toolbar]               │
├───────────────────┬──────────────────────────────┬───────────────────────┤
│ [Topologie|Gruppen│                              │ Eigenschaften         │
│  |Katalog]        │  Center (gestapelt)          │ ──────────────────    │
│                   │                              │ Gerät / Gruppenadresse│
│  ▼ Bereich 1      │  A: Parameter-Editor         │ Typ: switch-actuator  │
│    ▼ Linie 1.1    │     oder                     │ Phys. Adresse: [1.1.1]│
│      Gerät 1.1.1  │  B: Busmonitor-Tabelle       │                       │
│                   │                              │ ─ oder ─              │
│ (Gruppen-Tab)     │                              │ Adresse: 0/0/1        │
│  ▼ 0 Beleuchtung  │                              │ Name: [Wohnzimmer]    │
│    ▼ 0 EG         │                              │ DPT:  [1.001]         │
│      0/0/1        │                              │                       │
├───────────────────┴──────────────────────────────┴───────────────────────┤
│  ● Verbunden (grün) | Bereit                    [Status-Bar, dunkel]     │
└──────────────────────────────────────────────────────────────────────────┘
```

## Project File Format (`*.knxproj`)

KNX standard ZIP archive, compatible with ETS 6. Written/read by `KnxprojSerializer`.

```
projekt.knxproj  (ZIP)
├── 0.xml                    ← Project list + metadata (project name, ID)
└── P-XXXXXXXX/0.xml         ← Main project data
    ├── Topology
    │   └── Area[]           (Address, Name)
    │       └── Line[]       (Address, Name, MediumTypeRefId)
    │           └── DeviceInstance[]  (Address, ProductRefId, AppProgramRefId)
    │               ├── ParameterInstanceRef[]  (RefId, Value)
    │               └── ComObjectInstanceRef[]
    │                   └── Connectors → Send/Receive → GroupAddressRefId
    └── GroupAddresses
        └── GroupRanges
            └── GroupRange[]    (main groups)
                └── GroupRange[]  (middle groups)
                    └── GroupAddress[]  (Address, Name, DPTs)
```

Namespace: `http://knx.org/xml/project/21`

## Device Catalog Format (`.knxprod`)

KNX standard ZIP archive, compatible with ETS 6. Read by `KnxprodCatalog`.

```
device.knxprod  (ZIP)
└── M-XXXX/
    ├── M-XXXX_H-{hash}_HP-{hash}.xml     ← Hardware XML
    └── M-XXXX_A-{hash}-{ver}.xml         ← ApplicationProgram XML
```

OpenKNX devices use manufacturer ID `M-00FA`. IDs are derived deterministically
from the YAML manifest ID using a polynomial hash (see `YamlToKnxprod`).

## YAML → .knxprod Conversion Flow

```
catalog/devices/*.yaml
         │
         │  loadManifest()         (Manifest struct, yaml-cpp)
         ▼
   YamlToKnxprod::writeFile()
         │
         │  buildHardwareXml()     Hardware ZIP entry
         │  buildApplicationXml()  ApplicationProgram ZIP entry
         ▼
catalog/devices/*.knxprod
         │
         │  KnxprodCatalog::reload()
         ▼
KnxHardwareProduct → KnxApplicationProgram
         │
         │  shown in Catalog tab
         ▼
DeviceInstance::setAppProgram()
```

## KNX Memory Layout

The programming flow writes three memory regions to each device:

```
Address Table (default base 0x4000):
  [count_hi][count_lo]  ← 1 + number of group addresses
  [PA_hi][PA_lo]        ← physical address of the device
  [GA1_hi][GA1_lo]      ← first group address
  ...

Association Table (default base 0x4100):
  [count_hi][count_lo]  ← number of entries
  [ga_idx][co_number]   ← index into address table (1-based) + ComObject number
  ...

Parameter Block (base from KnxMemoryLayout.parameterBase):
  raw bytes at offsets defined per-parameter in KnxParameter.offset
```

`TableBuilder` computes these byte arrays from a `DeviceInstance + KnxApplicationProgram`.

## Programming Sequence

```
Tool                         KNX Router            Device
 │                               │                    │
 │  ── CONNECT_REQUEST ─────────►│                    │
 │  ◄─ CONNECT_RESPONSE ─────────│                    │
 │                               │                    │
 │  [User presses prog. button on device]             │
 │  ── A_IndividualAddress_Write ►│ ─────────────────►│
 │  (400 ms gap)                  │                    │
 │  ── A_Memory_Write (addrTable)►│ ─────────────────►│
 │  ── A_Memory_Write (assocTable)►│────────────────►│
 │  ── A_Memory_Write (params) ──►│ ─────────────────►│
 │  (1000 ms gap)                 │                    │
 │  ── A_Restart ────────────────►│ ─────────────────►│
 │                               │                    │
 │  ✓ done                        │                    │
```

## Bus Interface Transports

| Transport | Class | Notes |
|---|---|---|
| KNXnet/IP Tunneling | `KnxIpTunnelingClient` | UDP, port 3671. Heartbeat via CONNECTIONSTATE_REQUEST every 60 s. |
| KNXnet/IP Discovery | `KnxIpDiscovery` | SEARCH_REQUEST to multicast 224.0.23.12:3671. |
| USB Serial (CDC-ACM) | `UsbKnxInterface(Serial)` | Qt6::SerialPort, 115200 8N1. Frame: `[0xAB][len_hi][len_lo][cemi…]` |
| USB HID | `UsbKnxInterface(HID)` | Linux `/dev/hidrawN`, KNX spec 07_01_01. 64-byte reports: `[0x01][0x13][len][cemi…]` |

## Bus Monitor Data Flow

```
Device ── L_Data.Ind ──► Router ── TUNNEL_REQUEST ──► KnxIpTunnelingClient
                                                              │
                                                   cemiFrameReceived()
                                                              │
                                                    InterfaceManager
                                                              │
                                                   BusMonitorWidget
                                                    (BusMonitorModel)
```

## Library Dependency Graph

```
knxodt_core   (Project, KnxApplicationProgram, GroupAddress, Topology,
               KnxprodCatalog, YamlToKnxprod, KnxprojSerializer, Manifest)
     ▲
knxodt_knxip  (IKnxInterface, CemiFrame, TableBuilder, DeviceProgrammer, …)
     ▲
knxodt_usb    (UsbKnxInterface, ConnectInterfaceFactory)
     ▲
knxodt_ui     (MainWindow, Widgets, Dialogs, PropertiesPanel)
     ▲
KNXOpenDeveloperTool  (main.cpp)
```
