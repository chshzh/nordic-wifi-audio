# Board Init Module Spec

## Document Information

| Field          | Value                        |
|----------------|------------------------------|
| Project        | Nordic Wi-Fi Opus Audio Demo |
| NCS Version    | v3.3.0                       |
| PRD Version    | 2026-05-27-23-14             |
| Latest Version | 2026-05-27-23-14             |

## Changelog

| Version          | Summary of changes                                         |
|------------------|------------------------------------------------------------|
| 2026-05-27-23-14 | Initial spec derived from code (Mode C Reverse)            |

---

## Overview

This module provides multi-board hardware abstraction for initialization,
persistent storage (UICR), board version detection, and optional audio I/O
hardware (CS47L63 codec, I2S, USB audio, SD card).

| File                         | Board(s)          | Role                                           |
|------------------------------|-------------------|------------------------------------------------|
| `src/utils/nrf5340_audio_dk.c` | nRF53-based     | Full board init: NVMC, clock divider, board_version ADC |
| `src/utils/nrf54l_init.c`    | nRF54LM20A        | Minimal init: LEDs + buttons (no NVMC, no clock divider) |
| `src/utils/uicr.c`           | nRF53-based       | UICR channel read/write via `nrfx_nvmc`        |
| `src/utils/uicr_stub.c`      | non-nRF53         | No-op UICR stubs (returns 0)                   |
| `src/utils/board_version.c`  | nRF5340 Audio DK  | ADC-based board version detection              |
| `src/modules/audio_i2s.c`    | nRF53 (I2S)       | I2S PCM audio driver wrapper                   |
| `src/modules/audio_usb.c`    | All boards        | USB audio class (CDC + headset composite)      |
| `src/modules/hw_codec.c`     | nRF5340 Audio DK  | CS47L63 codec volume/routing control           |
| `src/drivers/cs47l63_comm.c` | nRF5340 Audio DK  | CS47L63 SPI communication driver               |
| `src/modules/sd_card.c`      | nRF5340 Audio DK  | SD card FAT filesystem access                  |
| `src/modules/sd_card_playback.c` | nRF5340 Audio DK | WAV file playback thread                    |
| `src/modules/audio_sync_timer.c` | nRF53         | RTC0-based audio sync timer                    |

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
├── audio_usb.c/h           — USB audio class (all boards)
├── hw_codec.c/h            — CS47L63 codec (nRF5340 Audio DK)
├── audio_sync_timer.c/h    — RTC0 audio sync (nRF53 only)
├── sd_card.c/h             — FatFS + SD card driver
└── sd_card_playback.c/h    — WAV playback thread
src/drivers/
└── cs47l63_comm.c/h        — CS47L63 SPI driver
```

---

## Multi-Board Compilation Guards

| Source file              | Compiled when                                    |
|--------------------------|--------------------------------------------------|
| `nrf5340_audio_dk.c`     | `CONFIG_SOC_SERIES_NRF53=y`                      |
| `nrf54l_init.c`          | `NOT CONFIG_SOC_SERIES_NRF53`                    |
| `uicr.c`                 | `CONFIG_SOC_SERIES_NRF53=y`                      |
| `uicr_stub.c`            | `NOT CONFIG_SOC_SERIES_NRF53`                    |
| `board_version.c`        | `CONFIG_BOARD_NRF5340_AUDIO_DK_NRF5340_CPUAPP=y` |
| `audio_i2s.c`            | `CONFIG_NRFX_I2S=y`                              |
| `audio_sync_timer.c`     | `CONFIG_AUDIO_SYNC_TIMER_USES_RTC=y` (nRF53 default) |
| `audio_usb.c`            | Always (USB headset composite on all boards)     |
| `hw_codec.c`             | `CONFIG_BOARD_NRF5340_AUDIO_DK_NRF5340_CPUAPP=y` (implicit via Kconfig) |
| `cs47l63_comm.c`         | `CONFIG_BOARD_NRF5340_AUDIO_DK_NRF5340_CPUAPP=y` |

In `audio_i2s.h`, all public functions are wrapped with `#if IS_ENABLED(CONFIG_NRFX_I2S)`
and provide empty inline stubs in the `#else` branch, so callers compile unconditionally.

---

## Zbus Integration

This module does not use Zbus. Board init runs via `SYS_INIT` before the
application event loop starts.

---

## USB Audio (audio_usb.c)

Provides a USB audio headset composite device (microphone + headphones):
- DTS node `hs_0` (compatible `"usb-audio-hs"`) must exist in the board overlay.
- nRF5340 / nRF7002DK: node under `&usbd`.
- nRF54LM20DK: node under `&usbhs` (USB High-Speed controller).
- Mic and headphone channels: L+R, with mute feature enabled.

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

| Symbol                              | Description                                    | Default      |
|-------------------------------------|------------------------------------------------|--------------|
| `CONFIG_NRFX_I2S`                   | Enable I2S PCM driver                          | y (nRF53 board conf) |
| `CONFIG_NRFX_NVMC`                  | Enable NVMC for UICR access                    | y if nRF53   |
| `CONFIG_AUDIO_SYNC_TIMER_USES_RTC`  | Use RTC0 for audio sync timer                  | y if nRF53   |
| `CONFIG_NRF5340_AUDIO_SD_CARD_MODULE` | Enable SD card + FatFS                       | n            |
| `CONFIG_FAT_FILESYSTEM_ELM`         | FatFS library (required by SD card module)     | n (nRF7002DK, nRF54LM20DK board conf) |
| `CONFIG_CS47L63_STACK_SIZE`         | CS47L63 SPI thread stack size (bytes)          | 4096         |
| `CONFIG_CS47L63_THREAD_PRIO`        | CS47L63 thread priority                        | 4            |
| `CONFIG_SD_CARD_PLAYBACK_STACK_SIZE`| SD playback thread stack size                  | 4096         |
| `CONFIG_POWER_MEAS_START_ON_BOOT`   | Start power measurement thread on boot         | n            |

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

| Condition                      | Handling                                              |
|--------------------------------|-------------------------------------------------------|
| `led_init()` failure           | `LOG_ERR`, return error (init fails at boot)          |
| `button_handler_init()` failure | `LOG_ERR`, return error                              |
| `nrfx_nvmc` unavailable        | Only compiled when `CONFIG_NRFX_NVMC=y` (nRF53 only) |
| Board version ADC failure      | `LOG_ERR`, return error                               |
| USB audio init failure         | `LOG_ERR`, USB audio path disabled                    |

---

## Test Points

| UART log string                     | Expected condition                        |
|-------------------------------------|-------------------------------------------|
| `Board version: <n>`                | nRF5340 Audio DK ADC board version read   |
| `LED initialized`                   | `led_init()` success                      |
| `Button handler initialized`        | `button_handler_init()` success           |
| `UICR channel: L` / `R`            | Channel read from UICR (nRF53 only)       |
| `nrf5340_audio_dk_init done`        | Board init completed                      |
