# Board Init Module Spec - nordic-wifi-audio

## Document Information

| Field | Value |
|---|---|
| Project | Nordic Wi-Fi Audio Demo |
| Version | 2026-06-26-11-29 |
| PRD Version | 2026-06-26-09-55 |
| NCS Version | v3.3.0 |
| Target Board(s) | nRF5340 Audio DK + nRF7002EK (P0); nRF7002DK (P1, gateway only); nRF54LM20DK + nRF7002EB2 (P1, gateway only) |
| Status | In Review |

## Changelog

| Version | Summary of changes |
|---|---|
| 2026-06-26-11-29 | HW validation outcome: nRF54LM20DK forced to **Full-Speed** (`CONFIG_UDC_DRIVER_HIGH_SPEED_SUPPORT_ENABLED=n`, `high-speed` dropped from UAC2 node) — High-Speed enumeration caused the host's iso-OUT stream to be continuously cancelled (headset starved). All three boards now run UAC2 @ Full-Speed. Proper HS = future work. |
| 2026-06-26-10-00 | Updated to PRD v2026-06-26-09-55: migrate USB audio from legacy USB Audio Class 1.0 (`usb_audio` / `USB_DEVICE_STACK`) to UAC2 (`usbd_uac2` / `USB_DEVICE_STACK_NEXT` + `USBD_AUDIO2_CLASS`) on all three boards. Adapter design: `audio_usb.c` keeps its `data_fifo`-based public API so `audio_system.c` is unchanged; UAC2 callbacks bridge to `data_fifo`. New `audio_usb_init.c` builds the USBD device context. DTS `usb-audio-hs` `hs_0` nodes replaced by `zephyr,uac2` nodes (Audio DK overrides the base-board `hs_0`). nRF54LM20DK enumerates High-Speed; nRF5340/nRF7002DK Full-Speed. Resolves 0-overview Open Issue #3 (UDC migration). |
| 2026-06-23-14-27 | Synced to PRD v2026-06-23-14-27: nRF7002DK + nRF54LM20DK promoted to P1 (gateway only); ZEGO_BANNER_APP_NAME set per role in CMakeLists.txt (`nordic-wifi-audio-gateway` / `nordic-wifi-audio-headset`); CONFIG_LOG_BUFFER_SIZE=4096 added to nRF5340 Audio DK board conf |
| 2026-06-22-15-18 | Updated to PRD v2026-06-22-15-18: added per-board zego button/LED Kconfig, noted deferred USB-audio boards, nrf54l_init.c simplified (bricks own GPIO) |
| 2026-05-27-23-14 | Initial spec derived from code (Mode C Reverse) |

---

## Overview

This module provides multi-board hardware abstraction for initialization,
persistent storage (UICR), board version detection, and optional audio I/O
hardware (CS47L63 codec, I2S, USB audio, SD card).

| File | Board(s) | Role |
|---|---|---|
| `src/utils/nrf5340_audio_dk.c` | nRF53-based | Full board init: NVMC, clock divider, board_version ADC |
| `src/utils/nrf54l_init.c` | nRF54LM20A | Minimal init: LEDs + buttons (no NVMC, no clock divider) |
| `src/utils/uicr.c` | nRF53-based | UICR channel read/write via `nrfx_nvmc` |
| `src/utils/uicr_stub.c` | non-nRF53 | No-op UICR stubs (returns 0) |
| `src/utils/board_version.c` | nRF5340 Audio DK | ADC-based board version detection |
| `src/modules/audio_i2s.c` | nRF53 (I2S) | I2S PCM audio driver wrapper |
| `src/modules/audio_usb.c` | All boards | UAC2 audio class — bridges UAC2 callbacks to `data_fifo` |
| `src/modules/audio_usb_init.c` | All boards | Builds the USBD device context (descriptors, FS/HS config) for UAC2 |
| `src/modules/hw_codec.c` | nRF5340 Audio DK | CS47L63 codec volume/routing control |
| `src/drivers/cs47l63_comm.c` | nRF5340 Audio DK | CS47L63 SPI communication driver |
| `src/modules/sd_card.c` | nRF5340 Audio DK | SD card FAT filesystem access |
| `src/modules/sd_card_playback.c` | nRF5340 Audio DK | WAV file playback thread |
| `src/modules/audio_sync_timer.c` | nRF53 | RTC0-based audio sync timer |

---

## File Locations

```
src/utils/
├── nrf5340_audio_dk.c/h    — nRF53 board init (NVMC, clock div, board_version)
├── nrf54l_init.c           — nRF54LM20A board init (LEDs, buttons)
├── uicr.c/h                — UICR R/W via nrfx_nvmc (nRF53)
├── uicr_stub.c             — no-op UICR (non-nRF53)
├── board_version.c/h       — ADC board HW revision detect (nRF5340 Audio DK)
└── channel_assignment.c/h  — L/R/GW channel selection (UICR-backed)
src/modules/
├── audio_i2s.c/h           — I2S PCM (nRF53; no-op stubs otherwise)
├── audio_usb.c/h           — UAC2 audio class, data_fifo bridge (all boards)
├── audio_usb_init.c        — USBD device context for UAC2 (all boards)
├── hw_codec.c/h            — CS47L63 codec (nRF5340 Audio DK)
├── audio_sync_timer.c/h    — RTC0 audio sync (nRF53 only)
├── sd_card.c/h             — FatFS + SD card driver
└── sd_card_playback.c/h    — WAV playback thread
src/drivers/
└── cs47l63_comm.c/h        — CS47L63 SPI driver
```

---

## Multi-Board Compilation Guards

| Source file | Compiled when |
|---|---|
| `nrf5340_audio_dk.c` | `CONFIG_SOC_SERIES_NRF53=y` |
| `nrf54l_init.c` | `NOT CONFIG_SOC_SERIES_NRF53` |
| `uicr.c` | `CONFIG_SOC_SERIES_NRF53=y` |
| `uicr_stub.c` | `NOT CONFIG_SOC_SERIES_NRF53` |
| `board_version.c` | `CONFIG_BOARD_NRF5340_AUDIO_DK_NRF5340_CPUAPP=y` |
| `audio_i2s.c` | `CONFIG_NRFX_I2S=y` |
| `audio_sync_timer.c` | `CONFIG_AUDIO_SYNC_TIMER_USES_RTC=y` (nRF53 default) |
| `audio_usb.c` | Always (UAC2 device on all boards) |
| `audio_usb_init.c` | Always (USBD device context on all boards) |
| `hw_codec.c` | `CONFIG_BOARD_NRF5340_AUDIO_DK_NRF5340_CPUAPP=y` (implicit via Kconfig) |
| `cs47l63_comm.c` | `CONFIG_BOARD_NRF5340_AUDIO_DK_NRF5340_CPUAPP=y` |

In `audio_i2s.h`, all public functions are wrapped with `#if IS_ENABLED(CONFIG_NRFX_I2S)`
and provide empty inline stubs in the `#else` branch, so callers compile unconditionally.

---

## Zbus Integration

This module does not use Zbus. Board init runs via `SYS_INIT` before the
application event loop starts.

---

## USB Audio (audio_usb.c + audio_usb_init.c)

The Gateway presents itself to the host PC as a **UAC2** (USB Audio Class 2.0) device
when `CONFIG_AUDIO_SOURCE_USB=y` (the default gateway source; overridden only by
`overlay-gateway-linein.conf`). UAC2 is class-native on Windows 10+, macOS, and Linux.

### Why UAC2 (migration from UAC1)

The legacy `usb_audio` class (`CONFIG_USB_DEVICE_STACK`) is deprecated. UAC2
(`usbd_uac2`, on the USBD-next / UDC stack) adds an explicit feedback endpoint
(prevents long-term sample drift between host and device clocks) and supports
High-Speed enumeration on controllers that have it. This resolves
[0-overview.md](0-overview.md) Open Issue #3.

### Adapter design (data_fifo preserved)

`audio_usb.c` keeps its existing `data_fifo`-based public API
(`audio_usb_init(void)`, `audio_usb_start(struct data_fifo *tx, struct data_fifo *rx)`,
`audio_usb_stop()`, `audio_usb_disable()`), so **`audio_system.c` is not modified**.
Internally the legacy `usb_audio_*` calls are replaced with UAC2; the `uac2_ops`
callbacks bridge to `data_fifo`:

| UAC2 callback (`struct uac2_ops`) | Bridge action |
|---|---|
| `get_recv_buf` | Allocate a 1 ms USB RX scratch block from a `K_MEM_SLAB` |
| `data_recv_cb` | Copy the received 1 ms block into `fifo_rx` (`data_fifo_pointer_first_vacant_get` → `data_fifo_block_lock`); on full FIFO, drop oldest (same overrun handling as legacy `data_received`) |
| `sof_cb` | TX only (`CONFIG_STREAM_BIDIRECTIONAL`): pull a block from `fifo_tx`, `usbd_uac2_send()` |
| `terminal_update_cb` | Track per-terminal enable state set by the host |
| `buf_release_cb` | Free the TX scratch block back to its slab |

`audio_usb_init.c` builds the `usbd_context` via `USBD_DEVICE_DEFINE` +
`audio_usbd_init_device()` (descriptors, VID/PID, power attributes). It registers a
**Full-Speed** configuration on all boards; a High-Speed configuration is added only when
`usbd_caps_speed()` reports High-Speed. On nRF54LM20DK the controller is forced to Full-Speed
(see below), so only the Full-Speed configuration is registered there too.

### DTS nodes per board (`zephyr,uac2`)

The legacy `hs_0` / `"usb-audio-hs"` nodes are replaced by a `zephyr,uac2` node tree
(clock-source + input/output terminals + audio-streaming, 48 kHz, 16-bit, subslot-size 2):

| Board | USB controller | UAC2 node placement | Speed |
|---|---|---|---|
| nRF5340 Audio DK + nRF7002EK | `&usbd` (`zephyr_udc0`) | Base board DTS defines `hs_0` under `&usbd`; the **app overlay must disable/delete `hs_0`** and add the `zephyr,uac2` node | Full-Speed |
| nRF7002DK | `&usbd` | App overlay (replaces existing `hs_0`) | Full-Speed |
| nRF54LM20DK + nRF7002EB2 | `&usbhs` (DWC2, HS-capable) | App overlay (replaces existing `hs_0`) | **Full-Speed** (forced) |

> **nRF54LM20DK speed caveat:** the `usbhs` (DWC2) controller is HS-capable, but
> High-Speed enumeration caused the host's isochronous OUT audio stream to be continuously
> cancelled (no PCM delivered, headset starved). It is therefore forced to **Full-Speed** via
> `CONFIG_UDC_DRIVER_HIGH_SPEED_SUPPORT_ENABLED=n` in the board conf (UAC2 node advertises
> `full-speed` only). Full-Speed (1 ms / 192-byte packets) matches the audio FIFO block sizing
> and is the validated-working mode. Proper High-Speed UAC2 (125 µs micro-framing) is future work.

> **Audio DK caveat:** `hs_0` lives in the shared base board DTS
> (`nrf5340_audio_dk_nrf5340_cpuapp_common.dtsi`), which must not be edited. The app
> overlay overrides it (`/delete-node/ &hs_0;` or `status = "disabled"`) before adding
> the UAC2 node, otherwise both classes claim the same endpoints.

### Framing / latency staging

The audio pipeline accumulates `CONFIG_FIFO_FRAME_SPLIT_NUM` × 1 ms blocks into one
10 ms frame. The UAC2 audio-streaming endpoint uses a **1 ms service interval at Full-Speed
on all boards**, matching the `data_fifo` block sizing (192-byte / 1 ms packets). All three
boards therefore run UAC2 at Full-Speed. High-Speed on nRF54LM20DK was attempted but the host
continuously cancelled the isochronous OUT stream at a 1 ms interval; running the HS endpoint
correctly (125 µs micro-framing, which requires reworking `data_fifo` block sizing) is a
**documented future optimization**, not part of this migration.

---

## UICR / Persistent Channel Storage

`uicr.c` stores the headset's audio channel (L or R) in UICR via `nrfx_nvmc`.

```c
int uicr_channel_get(enum audio_channel *channel);
int uicr_channel_set(enum audio_channel channel);
uint32_t uicr_snr_get(void);
```

On nRF54LM20A (no NVMC), `uicr_stub.c` provides identical signatures returning 0:
- `uicr_channel_get()` → returns `AUDIO_CH_L` (default).
- `uicr_channel_set()` → no-op, returns 0.
- `uicr_snr_get()` → returns 0.

---

## Kconfig Flags

### Audio hardware
| Symbol | Description | Default |
|---|---|---|
| `CONFIG_NRFX_I2S` | Enable I2S PCM driver | y (nRF53 board conf) |
| `CONFIG_NRFX_NVMC` | Enable NVMC for UICR access | y if nRF53 |
| `CONFIG_AUDIO_SYNC_TIMER_USES_RTC` | Use RTC0 for audio sync timer | y if nRF53 |
| `CONFIG_NRF5340_AUDIO_SD_CARD_MODULE` | Enable SD card + FatFS | n |
| `CONFIG_FAT_FILESYSTEM_ELM` | FatFS library (required by SD card module) | n (nRF7002DK, nRF54LM20DK board conf) |
| `CONFIG_CS47L63_STACK_SIZE` | CS47L63 SPI thread stack size (bytes) | 4096 |
| `CONFIG_CS47L63_THREAD_PRIO` | CS47L63 thread priority | 4 |
| `CONFIG_SD_CARD_PLAYBACK_STACK_SIZE` | SD playback thread stack size | 4096 |
| `CONFIG_POWER_MEAS_START_ON_BOOT` | Start power measurement thread on boot | n |

### USB Audio (UAC2)

Selected automatically by `CONFIG_AUDIO_SOURCE_USB` (mirrors the nRF5340 Audio app pattern):

| Symbol | Description | Default |
|---|---|---|
| `CONFIG_USB_DEVICE_STACK_NEXT` | USBD-next / UDC stack (replaces legacy `USB_DEVICE_STACK`) | y (selected by `AUDIO_SOURCE_USB`) |
| `CONFIG_USBD_AUDIO2_CLASS` | UAC2 class driver (`usbd_uac2`) | y (selected by `AUDIO_SOURCE_USB`) |
| `CONFIG_USBD_VID` | USB Vendor ID (Nordic `0x1915`) | 0x1915 |
| `CONFIG_USBD_PID` | USB Product ID (project-specific) | project value |
| `CONFIG_USBD_PRODUCT` | Product string descriptor | "Nordic Wi-Fi Audio" |
| `CONFIG_USBD_MANUFACTURER` | Manufacturer string descriptor | "Nordic Semiconductor" |
| `CONFIG_USBD_MAX_POWER` | Max bus current (×2 mA) | per board |
| `CONFIG_USBD_SELF_POWERED` | Self-powered attribute | y |

> The legacy `CONFIG_USB_DEVICE_STACK`, `CONFIG_USB_DEVICE_AUDIO`, and related
> `USB_DEVICE_*` symbols are removed when migrating; they conflict with the next stack.

### zego brick button/LED configuration (per-board `boards/*.conf`)

| Symbol | Description | nRF5340 Audio DK | nRF7002DK | nRF54LM20DK |
|---|---|---|---|---|
| `CONFIG_ZEGO_BUTTON_NUM_BUTTONS` | Number of physical buttons available | 5 | 2 | 3 |
| `CONFIG_ZEGO_LED_NUM_LEDS` | Number of LED units available | 9 | 2 | 4 |
| `CONFIG_APP_UX_WIFI_LED_IDX` | LED index for Wi-Fi status | 0 (RGB1) | 0 | 0 |

nRF5340 Audio DK LED layout (9 LEDs):
- idx 0–2: RGB1 (used for Wi-Fi status ROTATE animation by default — `rotate_count=3, rotate_indices[0..2]`)
- idx 3–5: RGB2
- idx 6–8: mono LEDs

### Board scope (P0 vs deferred)

| Board | Role(s) | Audio I/O | Status |
|---|---|---|---|
| nRF5340 Audio DK + nRF7002EK | Gateway + Headset | I2S / CS47L63 (gateway can also use UAC2 or LINE IN) | **P0 — must work** |
| nRF7002DK | Gateway (build) | UAC2 (Full-Speed) | Build must pass; UAC2 enumeration + receive validated in Phase 4.2 |
| nRF54LM20DK + nRF7002EB2 | Gateway (build) | UAC2 (Full-Speed, HS forced off) | Build must pass; UAC2 enumeration + receive validated in Phase 4.2 |

If a deferred board **cannot build at all** (not just lacks audio I/O), log it as
a known gap rather than silently dropping it.

---

## Board Init Entry Point

Both `nrf5340_audio_dk.c` and `nrf54l_init.c` provide the same public function:

```c
int nrf5340_audio_dk_init(void);
```

Called from `main()` as an early initialization step. Returns 0 on success,
negative errno on failure.

**nRF53 implementation** (`nrf5340_audio_dk.c`):
1. `nrfx_nvmc` clock divider setup
2. `board_version_valid_check()` (nRF5340 Audio DK only — guarded by `IS_ENABLED(CONFIG_BOARD_NRF5340_AUDIO_DK_NRF5340_CPUAPP)`)
3. `led_init()` + `button_handler_init()`
4. `leds_set()` — blink APP_3_GREEN, solid APP_RGB green

**nRF54LM20A implementation** (`nrf54l_init.c`):
1. `led_init()` + `button_handler_init()`
2. `leds_set()` — blink APP_3_GREEN, solid APP_RGB green
   (No NVMC, no clock divider, no ADC board version)

---

## Error Handling

| Condition | Handling |
|---|---|
| `led_init()` failure | `LOG_ERR`, return error (init fails at boot) |
| `button_handler_init()` failure | `LOG_ERR`, return error |
| `nrfx_nvmc` unavailable | Only compiled when `CONFIG_NRFX_NVMC=y` (nRF53 only) |
| Board version ADC failure | `LOG_ERR`, return error |
| UAC2 device node not ready (`device_is_ready`) | `LOG_ERR`, return `-EIO`; USB audio path disabled |
| `audio_usbd_init_device()` / `usbd_enable()` failure | `LOG_ERR`, return error; USB audio path disabled |
| UAC2 RX FIFO full (host retransmit / disconnect) | Drop oldest block, increment overrun counter (logged every 100) |

---

## Test Points

| UART log string | Expected condition |
|---|---|
| `Board version: <n>` | nRF5340 Audio DK ADC board version read |
| `LED initialized` | `led_init()` success |
| `Button handler initialized` | `button_handler_init()` success |
| `UICR channel: L` / `R` | Channel read from UICR (nRF53 only) |
| `nrf5340_audio_dk_init done` | Board init completed |
| `Ready for USB host to send/receive.` | UAC2 device enabled (`usbd_enable` success) |
| `USB RX first data received.` | First UAC2 OUT block reached `fifo_rx` |
