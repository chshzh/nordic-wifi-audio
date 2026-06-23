# Memory Optimization Report - nordic-wifi-audio

## Document Information

| Field | Value |
|---|---|
| Project | Nordic Wi-Fi Audio Demo |
| Version | 2026-06-23-14-48 |
| NCS Version | v3.3.0 |
| Target Board(s) | nRF5340 Audio DK + nRF7002EK (P0); nRF7002DK (P1, gateway only); nRF54LM20DK + nRF7002EB2 (P1, gateway only) |
| Method | Build output flash summary; ZView watermark measurements pending (Phase 4.2) |
| Status | Draft |

> `Version` = this doc's own latest edit time (`date +%Y-%m-%d-%H-%M`); bump on every edit.
> No `PRD Version` field — this doc tracks code metrics, not product requirements.

---

## Changelog

| Version | Summary of changes |
|---|---|
| 2026-06-23-14-48 | Initial report: pre-refactor flash baselines from 1-architecture.md; stack and heap measurements marked TBD pending Phase 4.2 ZView pass |

---

## Sizing Rules

| Resource | Formula | Headroom |
|---|---|---|
| Thread stacks (watermark < 5120 B) | `ceil(watermark / 0.8)` | 20 % |
| Thread stacks (watermark ≥ 5120 B) | `ceil(watermark / 0.9)` | 10 % |
| Heaps | `ceil(peak / 0.8)` | 20 % |

`NET_RX_STACK_SIZE` and `NET_TX_STACK_SIZE` are kept at the Zephyr default (2048 B)
to absorb network burst spikes.

---

## Headroom Targets

| Resource | Minimum headroom |
|---|---|
| nRF5340 internal flash | > 15 % (NFR-002: ≤ 85 % utilisation) |
| nRF54LM20A RRAM | > 30 % (NFR-002: ≤ 70 % utilisation) |
| RAM (total) | > 5 % of SoC RAM |
| `storage_partition` (16 KB / 32 KB NVS) | > 10 KB free |

---

## Flash & RAM Budget

Pre-refactor baselines measured with Opus overlay (worst-case flash):

| Config | Board | Flash used | Flash avail | Flash headroom |
|---|---|---|---|---|
| gateway + opus | nRF5340 Audio DK | 776 KB | 1024 KB | 24 % |
| headset + opus | nRF5340 Audio DK | 802 KB | 1024 KB | 22 % |
| gateway + opus | nRF7002DK | 748 KB | 1024 KB | 27 % |
| gateway + opus | nRF54LM20DK | 722 KB | 1940 KB | 63 % |

> ⚠️ These are **pre-refactor** baselines from NCS v3.2 era. Post-refactor (zego brick architecture,
> NCS v3.3.0) measurements are pending Phase 4.2 validation.
> P2P + PCM (default build, no Opus) is expected to be smaller than the Opus baseline.

RAM budget measurements: **TBD — pending Phase 4.2 ZView pass.**

---

## Thread Stack Analysis

> To be filled after Phase 4.2 hardware validation using ZView live watermark measurements.
> One row per thread; use worst-case value across all boards.

| Thread / WQ | Kconfig | nRF5340 watermark (B) | nRF54LM20DK watermark (B) | Worst-case | Rule | New size | Old size | Δ (B) |
|---|---|---|---|---|---|---|---|---|
| `encoder_thread` | `CONFIG_ENCODER_STACK_SIZE` | — | — | — | ÷0.8 | — | — | — |
| `audio_datapath_thread` | `CONFIG_AUDIO_DATAPATH_STACK_SIZE` | — | — | — | ÷0.8 | — | — | — |
| `socket_utils_thread` | `CONFIG_SOCKET_STACK_SIZE` | — | — | — | ÷0.8 | — | — | — |
| `cs47l63_thread` | `CONFIG_CS47L63_STACK_SIZE` | — | — | — | ÷0.8 | — | — | — |
| `button_msg_sub_thread` | `CONFIG_BUTTON_MSG_SUB_STACK_SIZE` | — | — | — | ÷0.8 | — | — | — |
| `le_audio_msg_sub_thread` | `CONFIG_LE_AUDIO_MSG_SUB_STACK_SIZE` | — | — | — | ÷0.8 | — | — | — |
| `volume_msg_sub_thread` | `CONFIG_VOLUME_MSG_SUB_STACK_SIZE` | — | — | — | ÷0.8 | — | — | — |
| `sd_card_playback_thread` | `CONFIG_SD_CARD_PLAYBACK_STACK_SIZE` | — | — | — | ÷0.8 | — | — | — |
| `sysworkq` | `SYSTEM_WORKQUEUE_STACK_SIZE` | — | — | — | ÷0.8 | — | — | — |
| `rx_q` | `NET_RX_STACK_SIZE` | — | — | — | kept | 2048 | 2048 | 0 |
| `tx_q` | `NET_TX_STACK_SIZE` | — | — | — | kept | 2048 | 2048 | 0 |

---

## Heap Analysis

> To be filled after Phase 4.2 ZView pass.

| Heap | ZView pool name | nRF5340 watermark (B) | nRF54LM20DK watermark (B) | Worst-case | New size | Old size | Δ (B) |
|---|---|---|---|---|---|---|---|
| System heap | `_system_heap` | — | — | — | — | — | — |
| WPA supplicant heap | `WPA_SUPPLICANT_*` | — | — | — | — | — | — |
| nRF Wi-Fi heap | `NRF_WIFI_*` | — | — | — | — | — | — |

---

## ISR Stack

| Board | ISR usage (B) | Allocated (B) | Utilization |
|---|---|---|---|
| nRF5340 Audio DK | — | 2048 | — |
| nRF7002DK | — | 2048 | — |
| nRF54LM20DK | — | 2048 | — |

---

## Summary of Changes Applied

No stack or heap optimizations applied yet. Baseline measurements pending.

| Kconfig | Old | New | Δ (B) | Reason |
|---|---|---|---|---|
| `CONFIG_LOG_BUFFER_SIZE` | 1024 | 4096 | +3072 | Boot banner silently lost on nRF5340 Audio DK (overflow); see board conf |

---

## Open Issues

| # | Description | Owner | Target |
|---|---|---|---|
| 1 | Measure all thread stack watermarks via ZView after P2P+PCM boot (steady-state with audio streaming) | — | Phase 4.2 |
| 2 | Measure WPA supplicant and nRF Wi-Fi heap peaks for P2P vs STA | — | Phase 4.2 |
| 3 | Confirm post-refactor flash totals for default (PCM+P2P) and Opus overlay builds | — | Phase 4.2 |
