# Memory Optimization Report - nordic-wifi-audio

## Document Information

| Field | Value |
|---|---|
| Project | Nordic Wi-Fi Audio Demo |
| Version | 2026-06-26-10-00 |
| PRD Version | 2026-06-26-09-55 |
| NCS Version | v3.3.0 |
| Target Board(s) | nRF5340 Audio DK + nRF7002EK (P0); nRF7002DK (P1, gateway only); nRF54LM20DK + nRF7002EB2 (P1, gateway only) |
| Method | Build output flash summary; ZView watermark measurements pending (Phase 4.2) |
| Status | Draft |

> `Version` = this doc's own latest edit time (`date +%Y-%m-%d-%H-%M`); bump on every edit.
> `PRD Version` = the PRD revision this report's targets and budgets were reconciled against.

---

## Changelog

| Version | Summary of changes |
|---|---|
| 2026-06-26-10-00 | Updated to PRD v2026-06-26-09-55: USB audio UAC1→UAC2 (USBD-next stack). Flash/RAM delta vs legacy `USB_DEVICE_STACK` is **TBD — pending Phase 4.2** build comparison; the next stack drops the legacy USB device support in favour of UDC + `usbd_uac2`. No change to audio FIFO sizing (1 ms framing retained). |
| 2026-06-25-13-35 | Updated to PRD v2026-06-25-13-30: picolibc (−~15 KB flash/−~14 KB RAM); removed CMSIS-DSP/LC3 tone path (square-wave test tone), SD card + power-measurement modules, src/debug; memonitor ZView-only; NET_MAX_CONN/NET_MAX_CONTEXTS=8 for dual-mode socket budget; dual-mode flash figures |
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

Current figures (nRF5340 Audio DK, **picolibc** C library, slot0 = 1016 KB):

| Config | Board | Flash used | Flash avail | Flash util |
|---|---|---|---|---|
| dual-mode gateway (P2P + STA + mDNS, wifi-p2p snippet) | nRF5340 Audio DK | ≈ 1006 KB | 1016 KB | ≈ 99.07 % |
| dual-mode headset (P2P + STA + mDNS, wifi-p2p snippet) | nRF5340 Audio DK | ≈ 1001 KB | 1016 KB | ≈ 98.57 % |
| STA-only (no wifi-p2p snippet) | nRF5340 Audio DK | ≈ 640 KB | 1016 KB | ≈ 63 % |

> The single dual-mode firmware (P2P + STA + mDNS) only fits in the 1016 KB slot0 after switching the
> C library to **picolibc** (`CONFIG_PICOLIBC=y`, `CONFIG_MINIMAL_LIBC=n`, `CONFIG_NEWLIB_LIBC=n` in
> `prj.conf`, overriding the `NEWLIB` choice from `Kconfig.defaults`). picolibc saves ~15 KB flash and
> ~14 KB RAM vs newlib-nano. Flash was further reclaimed by dropping the CMSIS-DSP / LC3 `tone` test-tone
> path, the SD card and power-measurement modules, and `src/debug/` (see Summary of Changes Applied).
> At ≈ 99 % the dual-mode build exceeds the NFR-002 ≤ 85 % headroom target — flagged in Open Issues.

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
| ~~`sd_card_playback_thread`~~ | ~~`CONFIG_SD_CARD_PLAYBACK_STACK_SIZE`~~ | n/a | n/a | n/a | — | — | — | — (no longer built — SD card module removed) |
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

Footprint-reduction changes applied to make the single dual-mode firmware fit slot0 (see Flash & RAM
Budget). Thread-stack and heap watermark tuning is still pending Phase 4.2 measurements.

| Kconfig | Old | New | Δ | Reason |
|---|---|---|---|---|
| `CONFIG_LOG_BUFFER_SIZE` | 1024 | 4096 | +3072 B | Boot banner silently lost on nRF5340 Audio DK (overflow); see board conf |
| `CONFIG_PICOLIBC` / `CONFIG_NEWLIB_LIBC` / `CONFIG_MINIMAL_LIBC` | newlib-nano (`Kconfig.defaults` `NEWLIB` choice) | `PICOLIBC=y`, `NEWLIB_LIBC=n`, `MINIMAL_LIBC=n` (in `prj.conf`) | −~15 KB flash / −~14 KB RAM | Switch C library to picolibc; what made the dual-mode firmware fit |
| `CONFIG_CMSIS_DSP`, `CMSIS_DSP_FASTMATH`, `FP16`, `FLOAT16` | enabled (pulled in by `nrf/lib/tone`) | dropped | flash reclaimed | Test-tone path replaced: LC3/`nrf/lib/tone` `tone_gen()` (used `arm_sin_f32`) → local static `square_tone_gen()`. The tone lib was the only consumer; `CONFIG_AUDIO_TEST_TONE` no longer `select TONE` |
| `CONFIG_NRF5340_AUDIO_SD_CARD_MODULE` | enabled | removed (`boards/nrf5340_audio_dk_nrf5340_cpuapp.conf`) | flash reclaimed | FAT/SDHC stack no longer linked; `sd_card_playback_thread` no longer built |
| `CONFIG_NRF5340_AUDIO_POWER_MEASUREMENT` | enabled | removed (`boards/nrf5340_audio_dk_nrf5340_cpuapp.conf`) | flash reclaimed | Power-measurement module no longer linked |
| `CONFIG_HEAPS_MONITOR` (`src/debug/` heaps_monitor) | enabled | removed (`src/debug/` deleted) | flash/RAM reclaimed | Replaced by ZView-based memonitor |
| `CONFIG_ZEGO_MEMONITOR` / `CONFIG_ZEGO_MEMONITOR_ZVIEW` | firmware periodic sampler | `ZEGO_MEMONITOR=y`, `ZEGO_MEMONITOR_ZVIEW=y` (ZView-only; sampler `INTERVAL_MS` line removed) | RAM/CPU reclaimed | Watermarks read live over SWD via ZView instead of an on-device sampling thread |
| `CONFIG_NET_MAX_CONN` | 4 | 8 | +4 conns | Dual-mode pool exhaustion: mDNS + hostap control + app socket left no slot for the P2P_GO DHCP server's port-67 bind → failed with `-ENOENT` (`prj.conf`) |
| `CONFIG_NET_MAX_CONTEXTS` | 6 | 8 | +2 contexts | Each bound socket consumes one conn + one context, so raised together with `NET_MAX_CONN` (`prj.conf`) |

---

## Open Issues

| # | Description | Owner | Target |
|---|---|---|---|
| 1 | Measure all thread stack watermarks via ZView after P2P+PCM boot (steady-state with audio streaming) | — | Phase 4.2 |
| 2 | Measure WPA supplicant and nRF Wi-Fi heap peaks for P2P vs STA | — | Phase 4.2 |
| 3 | Dual-mode firmware sits at ≈ 99 % of slot0, exceeding the NFR-002 ≤ 85 % flash headroom target; identify further flash savings or revisit the partition/target | — | Phase 4.2 |
