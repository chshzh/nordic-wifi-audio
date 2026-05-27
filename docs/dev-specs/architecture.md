# Architecture Spec — nordic-wifi-audio

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
| 2026-05-27-23-14 | Initial architecture derived from code (Mode C Reverse)    |

---

## Overview

Multi-threaded architecture. Audio data flows through a dedicated pipeline of
threads (encode → TX socket → network → RX socket → decode → output). UI events
(buttons, volume) and stream control events use Zbus. Network lifecycle events
use kernel semaphores.

The codebase compiles two separate application entry points under one CMakeLists:
- `wifi_audio_gateway/` — UDP server, audio source (I2S / USB / SD card)
- `wifi_audio_headset/` — UDP client, audio sink (I2S / USB hardware codec)

Board-specific hardware is gated by Kconfig (`CONFIG_SOC_SERIES_NRF53`,
`CONFIG_NRFX_I2S`, `CONFIG_BOARD_NRF5340_AUDIO_DK_*`) so the same `src/` tree
builds for all three boards.

---

## Module Map

```
nordic-wifi-audio/
├── wifi_audio_gateway/
│   └── main.c              — gateway entry: button/event threads, stream control
├── wifi_audio_headset/
│   └── main.c              — headset entry: button/event threads, stream control
└── src/
    ├── audio/
    │   ├── audio_system.c/h        — encoder/decoder thread lifecycle
    │   ├── audio_datapath.c/h      — drift compensation, presentation delay
    │   ├── sw_codec_select.c/h     — codec abstraction (Opus / LC3 / raw)
    │   └── wifi_audio_rx.c/h       — RX handler, frame protocol, decode
    ├── modules/
    │   ├── button_handler.c/h      — GPIO debounce, publishes button_chan
    │   ├── led.c/h                 — RGB + mono LED control
    │   ├── audio_i2s.c/h           — nRF53 I2S PCM driver wrapper
    │   ├── audio_usb.c/h           — USB audio class (headset composite)
    │   ├── audio_sync_timer.c/h    — RTC-based audio sync timer (nRF53 only)
    │   ├── hw_codec.c/h            — CS47L63 HW audio codec (nRF5340 Audio DK)
    │   ├── sd_card.c/h             — SD card FAT access
    │   └── sd_card_playback.c/h    — WAV playback thread from SD
    ├── net/
    │   ├── socket_utils.c/h        — UDP socket (server/client), TX/RX thread
    │   ├── wifi_utils.c/h          — SoftAP, connect, regulatory domain
    │   └── net_event_mgmt.c/h      — net_mgmt event callbacks, semaphores
    ├── drivers/
    │   └── cs47l63_comm.c/h        — CS47L63 SPI comms driver
    ├── debug/
    │   └── heaps_monitor.c/h       — kernel heap monitor (optional)
    └── utils/
        ├── nrf5340_audio_dk.c/h    — board init: NVMC, clock, board_version (nRF53)
        ├── nrf54l_init.c           — board init: LEDs, buttons only (nRF54LM20A)
        ├── uicr.c/h                — UICR channel read/write via NVMC (nRF53)
        ├── uicr_stub.c             — no-op UICR stubs (non-nRF53)
        ├── board_version.c/h       — ADC board version detection (nRF5340 Audio DK)
        ├── channel_assignment.c/h  — L/R/GW channel selection
        └── error_handler.c         — fatal error handler
```

---

## Zbus Channels

| Channel            | Message type              | Publisher(s)           | Subscriber(s)              | Notes |
|--------------------|---------------------------|------------------------|----------------------------|-------|
| `button_chan`       | `struct button_msg`        | `button_handler`       | `button_evt_sub` (main)    | `button_pin` + `button_action` |
| `le_audio_chan`     | `struct le_audio_msg`      | `wifi_audio_rx`, main  | `le_audio_evt_sub` (main)  | Carries stream events (START/STOP/STREAMING) |
| `volume_chan`       | `struct volume_msg`        | main (button handler)  | `hw_codec`, headset main   | `VOLUME_UP/DOWN/SET/MUTE/UNMUTE` |
| `content_control_chan` | `struct content_control_msg` | main              | audio subsystem            | `MEDIA_START/STOP` |
| `sdu_ref_msg`      | `struct sdu_ref_msg`       | `audio_datapath`       | `sdu_ref_msg_listen`       | TX sync timestamp for drift compensation |

---

## Message Definitions

See `src/zbus_common.h` for all struct definitions. Key types:

```c
struct button_msg { uint32_t button_pin; enum button_action button_action; };
struct le_audio_msg { enum le_audio_evt_type event; /* + BT fields unused in WiFi mode */ };
struct volume_msg { enum volume_evt_type event; uint8_t volume; };
struct content_control_msg { enum content_control_evt_type event; };
struct sdu_ref_msg { uint32_t tx_sync_ts_us; uint32_t curr_ts_us; bool adjust; };
```

---

## Thread Budget

| Thread                     | Stack size config                  | Priority | Purpose                                |
|----------------------------|------------------------------------|----------|----------------------------------------|
| `button_msg_sub_thread`    | `CONFIG_BUTTON_MSG_SUB_STACK_SIZE` | app      | Blocks on `button_chan`, dispatches actions |
| `le_audio_msg_sub_thread`  | `CONFIG_LE_AUDIO_MSG_SUB_STACK_SIZE` | app    | Blocks on `le_audio_chan`, drives stream state |
| `encoder_thread`           | `CONFIG_ENCODER_STACK_SIZE`        | app      | Runs SW codec encode loop              |
| `audio_datapath_thread`    | `CONFIG_AUDIO_DATAPATH_STACK_SIZE` | app      | Audio drift compensation, decode dispatch |
| `socket_utils_thread`      | `CONFIG_SOCKET_STACK_SIZE`         | app      | UDP socket bind/recv/send loop         |
| `volume_msg_sub_thread`    | `CONFIG_VOLUME_MSG_SUB_STACK_SIZE` | app      | Blocks on `volume_chan`, calls hw_codec |
| `content_ctrl_sub_thread`  | `CONFIG_CONTENT_CONTROL_MSG_SUB_STACK_SIZE` | app | media start/stop events         |
| `cs47l63_thread`           | `CONFIG_CS47L63_STACK_SIZE`        | high     | SPI codec comms (nRF5340 Audio DK only)|
| `sd_card_playback_thread`  | `CONFIG_SD_CARD_PLAYBACK_STACK_SIZE` | app    | SD WAV playback (optional)             |
| `power_meas_thread`        | `CONFIG_POWER_MEAS_STACK_SIZE`     | low      | Periodic power measurement (optional)  |

---

## Boot Sequence

```
1. SYS_INIT: Zephyr kernel, drivers, net stack
2. SYS_INIT: led_init()        — LED hardware ready
3. SYS_INIT: button_handler_init()
4. SYS_INIT: nrf5340_audio_dk_init() / nrf54l_init()
              — nRF53: NVMC, clock divider, board_version ADC, LEDs
              — nRF54LM20A: LEDs, buttons
5. main() — init_network_events()  — register net_mgmt callbacks
6. main() — WiFi connect / SoftAP start
7. main() — k_sem_take(iface_up_sem, ...)  — wait for interface up
8. main() — k_sem_take(wpa_supplicant_ready_sem, ...)
9. main() — k_sem_take(ipv4_dhcp_bond_sem, ...)   [STA mode]
         or k_sem_take(station_connected_sem, ...) [SoftAP mode — wait for client]
10. main() — audio_system_init(), wifi_audio_rx_init()
11. main() — socket_utils thread spawned
12. main() — k_thread_create(button_msg_sub_thread)
13. main() — k_thread_create(le_audio_msg_sub_thread)
14. main() — audio_system_encoder_start()
→ Streaming begins
```

---

## Memory Budget

*From verified pristine builds (NCS v3.3.0):*

| Config                              | Board          | Flash used | Flash total | Headroom |
|-------------------------------------|----------------|-----------|-------------|----------|
| gateway + opus                      | nRF5340 Audio DK | 776 KB  | 1024 KB     | 248 KB   |
| headset + opus                      | nRF5340 Audio DK | 802 KB  | 1024 KB     | 222 KB   |
| gateway + opus (nRF7002DK)          | nRF7002DK      | 748 KB    | 1024 KB     | 276 KB   |
| gateway + opus (nRF54LM20DK)        | nRF54LM20DK    | 722 KB    | 1940 KB     | 1218 KB  |

---

## Build Configurations

| Board target                         | Shield         | Role overlays                              | Board conf file                              |
|--------------------------------------|----------------|--------------------------------------------|----------------------------------------------|
| `nrf5340_audio_dk/nrf5340/cpuapp`   | `nrf7002ek`    | `overlay-opus.conf;overlay-audio-gateway.conf` | `boards/nrf5340_audio_dk_nrf5340_cpuapp.conf` |
| `nrf5340_audio_dk/nrf5340/cpuapp`   | `nrf7002ek`    | `overlay-opus.conf;overlay-audio-headset.conf` | same                                         |
| `nrf7002dk/nrf5340/cpuapp`          | (none)         | `overlay-opus.conf;overlay-audio-gateway.conf` | `boards/nrf7002dk_nrf5340_cpuapp.conf`       |
| `nrf54lm20dk/nrf54lm20a/cpuapp`     | `nrf7002eb2`   | `overlay-opus.conf;overlay-audio-gateway.conf` | `boards/nrf54lm20dk_nrf54lm20a_cpuapp.conf`  |

---

## Flash Partition Layout

### nRF5340 / nRF7002DK (1 MB flash)

```
0x000000 ┌─────────────────────────────┐
         │  slot0_partition             │  0xFC000 = 1008 KB
0x0FC000 ├─────────────────────────────┤
         │  storage_partition           │  0x4000 = 16 KB (settings/NVS)
0x100000 └─────────────────────────────┘
```

### nRF54LM20A (RRAM, 1940 KB available)

```
0x000000 ┌─────────────────────────────┐
         │  slot0_partition             │  0x1DD000 = 1908 KB
0x1DD000 ├─────────────────────────────┤
         │  storage_partition           │  0x8000 = 32 KB (settings/NVS)
0x1E5000 └─────────────────────────────┘
```

---

## Test Points (UART Log Markers)

| Marker string                            | Expected at                                   |
|------------------------------------------|-----------------------------------------------|
| `NRF5340_WIFI_AUDIO_COMP_DATE=...`       | Build info printed at boot                    |
| `Waiting for WiFi connection...`         | Before net interface up                       |
| `WiFi connected`                         | After DHCP / SoftAP client joined             |
| `Socket connected`                       | After UDP socket established                  |
| `Audio stream started`                   | After encoder_start() called                  |
| `Drft comp state: CALIB`                | Drift compensation entering calibration       |
| `Drft comp state: STEADY`               | Drift compensation converged (good sign)      |
