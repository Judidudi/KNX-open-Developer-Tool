# Device Manifest Format

Device manifests describe a KNX device type. The tool reads them from the catalog
directory. The OpenKNX firmware stack reads them at compile time to generate the
correct memory layout on the device.

**The manifest is the shared contract between tool and firmware** — both sides
must use the same `memoryLayout` addresses and `parameters[].memoryOffset` values.

## File Location

- **Bundled catalog:** `catalog/devices/<device-id>.yaml` (in this repository)
- **User catalog:** `~/.config/OpenKNX/KNXOpenDeveloperTool/catalog/devices/`
- **Override (CI/test):** Set env `KNXODT_CATALOG_PATH`

## Full Schema

`schemas/device-manifest.schema.yaml` — formal JSON-Schema definition.

## Example (switch-actuator-1ch.yaml)

```yaml
id: "switch-actuator-1ch"
version: "1.0.0"
manufacturer: "OpenKNX"
name:
  de: "Schaltaktor 1-fach"
  en: "Switch Actuator 1-channel"
hardware:
  target: "STM32G0B1"
  transceiver: "NCN5120"
channels:
  - id: ch1
    name:
      de: "Kanal 1"
      en: "Channel 1"
comObjects:
  - id: co_switch_ch1
    number: 0          # ComObject table index (must match firmware)
    channel: ch1
    name:
      de: "Schalten"
      en: "Switch"
    dpt: "1.001"
    flags: [C, W, T, U]
  - id: co_status_ch1
    number: 1
    channel: ch1
    name:
      de: "Status"
      en: "Status"
    dpt: "1.001"
    flags: [C, R, T]
parameters:
  - id: p_startup_delay
    name:
      de: "Einschaltverzögerung"
      en: "Startup delay"
    type: uint16
    unit: ms
    default: 0
    range: [0, 30000]
    memoryOffset: 0    # byte offset within parameter block
    size: 2            # number of bytes (auto-derived from type if omitted)
  - id: p_relay_mode
    name:
      de: "Relaismodus"
      en: "Relay mode"
    type: uint8
    default: 0
    memoryOffset: 2
    size: 1
memoryLayout:
  addressTable:     "0x4000"   # base address for KNX address table
  associationTable: "0x4100"   # base address for KNX association table
  comObjectTable:   "0x4200"   # base address for ComObject table
  parameterBase:    "0x4400"   # start of parameter block
  parameterSize:    4          # total byte size of parameter block
```

## Fields Reference

### Top-level

| Field | Required | Description |
|---|---|---|
| `id` | ✓ | Unique device identifier (lowercase, hyphens only) |
| `version` | ✓ | Semantic version (`MAJOR.MINOR.PATCH`) |
| `manufacturer` | ✓ | Manufacturer display name |
| `name.de` / `name.en` | ✓ | Human-readable name |
| `hardware.target` | ✓ | MCU identifier (e.g. `STM32G0B1`) |
| `hardware.transceiver` | ✓ | Bus transceiver IC (e.g. `NCN5120`) |
| `channels[]` | – | Optional grouping of ComObjects |
| `comObjects[]` | ✓ | Communication object definitions |
| `parameters[]` | – | Configurable parameter definitions |
| `memoryLayout` | ✓ | KNX memory table base addresses |

### comObjects[]

| Field | Required | Description |
|---|---|---|
| `id` | ✓ | Unique identifier within manifest |
| `number` | ✓ | ComObject table index — must match firmware |
| `channel` | – | Channel reference (`id` from `channels[]`) |
| `name.de/.en` | ✓ | Display name |
| `dpt` | ✓ | Data Point Type (e.g. `"1.001"`, `"9.001"`) |
| `flags` | ✓ | Communication flags: `C`=Communicate, `R`=Read, `W`=Write, `T`=Transmit, `U`=Update |

### parameters[]

| Field | Required | Description |
|---|---|---|
| `id` | ✓ | Unique identifier within manifest |
| `name.de/.en` | ✓ | Display name |
| `type` | ✓ | `bool`, `uint8`, `uint16`, `uint32`, `enum` |
| `unit` | – | Display unit (e.g. `ms`, `%`, `°C`) |
| `default` | – | Default value (number or string for enum) |
| `range` | – | `[min, max]` for numeric types |
| `values` | – | For `enum`: list of `{value: N, label: "…"}` |
| `memoryOffset` | ✓ | Byte offset within the parameter block |
| `size` | – | Byte count; auto-derived from type if omitted |

Auto-derived sizes: `bool`/`uint8`/`enum` → 1 byte, `uint16` → 2 bytes, `uint32` → 4 bytes.

### memoryLayout

| Field | Required | Default | Description |
|---|---|---|---|
| `addressTable` | – | `0x4000` | KNX address table base address |
| `associationTable` | – | `0x4100` | KNX association table base address |
| `comObjectTable` | – | `0x4200` | ComObject table base address |
| `parameterBase` | – | `0x4400` | Parameter block start address |
| `parameterSize` | – | auto | Total byte size of parameter block |

Addresses can be decimal integers or hex strings (`"0x4000"`).

If `parameterSize` is omitted, it is auto-computed as
`max(memoryOffset + size)` over all parameters.

## DPT Reference (most common)

| DPT | Name | Size |
|---|---|---|
| 1.001 | Switch (on/off) | 1 bit |
| 1.008 | Up/Down | 1 bit |
| 1.009 | Open/Close | 1 bit |
| 3.007 | Dimming (relative) | 4 bit |
| 5.001 | Percentage (0–100%) | 1 byte |
| 5.010 | Counter value (0–255) | 1 byte |
| 7.001 | 2-byte unsigned counter | 2 bytes |
| 9.001 | Temperature (°C) | 2 bytes |
| 9.004 | Illuminance (lux) | 2 bytes |
| 14.019 | Electrical current (A) | 4 bytes |
| 17.001 | Scene number | 1 byte |

## Adding a New Device

1. Create `catalog/devices/<your-device-id>.yaml` using the example above.
2. Set `comObjects[].number` to match the ComObject indices in the firmware.
3. Set `parameters[].memoryOffset` to match the firmware's `KNX_PARAM_*` offsets.
4. Set `memoryLayout.parameterBase` to match the firmware's `KNX_PARAM_BASE`.
5. No code changes required — the tool picks up new manifests automatically.
