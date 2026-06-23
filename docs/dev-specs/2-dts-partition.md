# Flash Memory Layout - nordic-wifi-audio

## Document Information

| Field | Value |
|---|---|
| Project | Nordic Wi-Fi Audio Demo |
| Version | 2026-06-23-14-48 |
| PRD Version | 2026-06-23-14-27 |
| NCS Version | v3.3.0 |
| Target Board(s) | nRF5340 Audio DK + nRF7002EK (P0); nRF7002DK (P1, gateway only); nRF54LM20DK + nRF7002EB2 (P1, gateway only) |
| Status | In Review |

> `Version` = this spec's own latest edit time (`date +%Y-%m-%d-%H-%M`); bump on every edit.
> `PRD Version` = the PRD Changelog timestamp this spec tracks.

---

## Changelog

| Version | Summary of changes |
|---|---|
| 2026-06-23-14-48 | Initial partition layout extracted from 1-architecture.md; actual DTS overlay values confirmed |

---

## Overview

This project uses **DTS-based fixed-partitions** (no Partition Manager, no MCUboot, no OTA).
`SB_CONFIG_PARTITION_MANAGER=n` is set in `sysbuild.conf`.

Each board overlay (`boards/<board>.overlay`) defines `slot0_partition` (application image)
and `storage_partition` (NVS/settings) directly in DeviceTree. No secondary OTA slot or
external flash is used.

---

## Flash Memory Layout

### nRF5340 Audio DK + nRF7002EK (nRF5340 — 1 MB internal flash)

DTS node: `&flash0`

| Address | Partition | Size | Purpose |
|---|---|---|---|
| `0x000000` | `slot0_partition` | 1008 KB (0xFC000) | Application image |
| `0x0FC000` | `storage_partition` | 16 KB (0x4000) | NVS — Wi-Fi mode, channel assignment |

Source: `boards/nrf5340_audio_dk_nrf5340_cpuapp.overlay`

---

### nRF7002DK (nRF5340 — 1 MB internal flash)

DTS node: `&flash0`

| Address | Partition | Size | Purpose |
|---|---|---|---|
| `0x000000` | `slot0_partition` | 1008 KB (0xFC000) | Application image |
| `0x0FC000` | `storage_partition` | 16 KB (0x4000) | NVS — Wi-Fi mode |

Source: `boards/nrf7002dk_nrf5340_cpuapp.overlay`

---

### nRF54LM20DK + nRF7002EB2 (nRF54LM20A — 2036 KB RRAM, 1940 KB available)

DTS node: `&cpuapp_rram` (last 96 KB reserved for FLPR core)

| Address | Partition | Size | Purpose |
|---|---|---|---|
| `0x000000` | `slot0_partition` | 1908 KB (0x1DD000) | Application image |
| `0x1DD000` | `storage_partition` | 32 KB (0x8000) | NVS — Wi-Fi mode |

Source: `boards/nrf54lm20dk_nrf54lm20a_cpuapp.overlay`

---

## Storage Partition Capacity Notes

The `storage_partition` is used exclusively by the Zephyr settings subsystem for NVS.

| Consumer | Estimated size | Notes |
|---|---|---|
| Wi-Fi mode selection | ~16 B | Single enum value via zego/wifi brick |
| Channel assignment (L/R/GW) | ~16 B | UICR-backed on nRF53; NVS fallback on nRF54L |
| **Total** | **~32 B** | Well within 16 KB (nRF53) / 32 KB (nRF54L) |

> This project does **not** store Wi-Fi credentials in NVS. STA mode credentials are provided
> via `overlay-sta.conf` or `overlay-wifi-cred-static.conf` at build time.

---

## DTS Overlay Checklist

- [x] `boards/nrf5340_audio_dk_nrf5340_cpuapp.overlay` — `slot0_partition` + `storage_partition` defined
- [x] `boards/nrf7002dk_nrf5340_cpuapp.overlay` — `slot0_partition` + `storage_partition` defined
- [x] `boards/nrf54lm20dk_nrf54lm20a_cpuapp.overlay` — `slot0_partition` + `storage_partition` defined
- [x] `sysbuild.conf` contains `SB_CONFIG_PARTITION_MANAGER=n`
- [x] No MCUboot overlay needed (single-image, no bootloader)
- [x] No OTA slot (`slot1_partition`) — OTA not in scope for this project

---

## Related Specs

- [1-architecture.md](1-architecture.md) — SoC selection and build matrix
- [3-memopt.md](3-memopt.md) — RAM budget and headroom tracking
