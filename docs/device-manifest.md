# Device Manifest Format

Device manifests describe a KNX device type. The tool reads them from the catalog directory. The firmware stack reads them at compile time to generate the correct memory layout.

## File Location

Place manifest files in:
- **Bundled catalog:** `catalog/devices/<device-id>.yaml` (in this repository)
- **User catalog:** `~/.config/OpenKNX/KNXOpenDeveloperTool/catalog/devices/`

## Schema

Full JSON-Schema: `schemas/device-manifest.schema.yaml`

## Example

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
    channel: ch1
    name:
      de: "Schalten"
      en: "Switch"
    dpt: "1.001"
    flags: [C, W, T, U]
parameters:
  - id: p_startup_delay
    name:
      de: "Einschaltverzögerung"
      en: "Startup delay"
    type: uint16
    unit: ms
    default: 0
    range: [0, 30000]
memoryLayout:
  baseAddress: 0x4000
```

## Fields

| Field | Required | Description |
|---|---|---|
| `id` | ✓ | Unique device identifier (lowercase, hyphens) |
| `version` | ✓ | Semantic version (MAJOR.MINOR.PATCH) |
| `manufacturer` | ✓ | Manufacturer name |
| `name.de` / `name.en` | ✓ | Display name (German + English) |
| `hardware.target` | ✓ | MCU identifier (e.g. STM32G0B1) |
| `hardware.transceiver` | ✓ | Bus transceiver (NCN5120, TP-UART, etc.) |
| `channels[]` | – | Optional channel grouping |
| `comObjects[]` | ✓ | Communication objects |
| `comObjects[].dpt` | ✓ | Data Point Type (e.g. "1.001") |
| `comObjects[].flags` | ✓ | C=Communicate, R=Read, W=Write, T=Transmit, U=Update |
| `parameters[]` | – | Configurable parameters |
| `parameters[].type` | ✓ | `bool`, `uint8`, `uint16`, `uint32`, `enum` |
| `parameters[].range` | – | [min, max] for numeric types |
| `parameters[].values` | – | For `enum`: list of {value, label} |
| `memoryLayout.baseAddress` | ✓ | Start address for parameter memory |

## DPT Reference (most common)

| DPT | Name | Size |
|---|---|---|
| 1.001 | Switch (on/off) | 1 bit |
| 1.008 | Up/Down | 1 bit |
| 3.007 | Dimming (relative) | 4 bit |
| 5.001 | Percentage (0–100%) | 1 byte |
| 5.010 | Counter value | 1 byte |
| 9.001 | Temperature (°C) | 2 bytes |
| 9.004 | Illuminance (lux) | 2 bytes |
| 14.019 | Electrical current (A) | 4 bytes |
