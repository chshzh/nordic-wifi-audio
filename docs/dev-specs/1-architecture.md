# Architecture Spec - nordic-wifi-audio

## Document Information

| Field | Value |
|---|---|
| Project | Nordic Wi-Fi Audio Demo |
| Version | 2026-06-25-13-35 |
| PRD Version | 2026-06-25-13-30 |
| NCS Version | v3.3.0 |
| Target Board(s) | nRF5340 Audio DK + nRF7002EK (P0); nRF7002DK, nRF54LM20DK + nRF7002EB2 (build) |
| Status | In Review |

## Changelog

| Version | Summary of changes |
|---|---|
| 2026-06-25-13-35 | Updated to PRD v2026-06-25-13-30: dual-mode boot/dispatch; SD card + src/debug removed from module map; picolibc + NET_MAX_CONN/CONTEXTS=8 in memory budget; P2P Client→P2P_GC |
| 2026-06-23-14-48 | Renamed from architecture.md; Flash Partition Layout moved to 2-dts-partition.md; Memory Budget replaced by reference to 3-memopt.md; Target Board(s) updated to P1 |
| 2026-06-22-15-18 | Updated to PRD v2026-06-22-15-18: zego brick architecture, P2P default boot sequence, module map revised, memory budget updated |
| 2026-05-27-23-14 | Initial architecture derived from code (Mode C Reverse) |

---

## Overview

Pattern: **zego brick + weak hooks**. Audio data flows through a dedicated pipeline of
threads (encode → TX socket → network → RX socket → decode → output). UI events and
state broadcasts use Zbus. Network lifecycle is handled entirely by the zego network
brick; the app reacts via weak-hook overrides.

The codebase compiles two separate application entry points under one CMakeLists:
- `wifi_audio_gateway/` — UDP server, audio source (I2S / USB; SD card present but disabled by default)
- `wifi_audio_headset/` — UDP client, audio sink (I2S / USB hardware codec)

Each entry point builds as a **single dual-mode firmware**: with the default
`-Dnordic-wifi-audio_SNIPPET=wifi-p2p` snippet, both P2P and STA support are compiled in,
and the active mode is NVS-persisted and runtime-switchable (no separate STA-only vs
P2P-only image). Per-role mode visibility is gated by `CONFIG_ZEGO_WIFI_MODE_*_ENABLED`
overlays (gateway exposes STA + P2P_GO; headset exposes STA + P2P_GC).

Board-specific hardware is gated by Kconfig (`CONFIG_SOC_SERIES_NRF53`,
`CONFIG_NRFX_I2S`, `CONFIG_BOARD_NRF5340_AUDIO_DK_*`) so the same `src/` tree
builds for all three boards.

---

## Module Map

```
nordic-wifi-audio/
├── wifi_audio_gateway/
│   └── main.c              — gateway entry: audio threads, ux setup, stream control
├── wifi_audio_headset/
│   └── main.c              — headset entry: audio threads, ux setup, stream control
├── src/
│   ├── audio/
│   │   ├── audio_system.c/h        — encoder/decoder thread lifecycle
│   │   ├── audio_datapath.c/h      — drift compensation, presentation delay
│   │   ├── sw_codec_select.c/h     — codec abstraction (Opus / LC3 / raw)
│   │   └── wifi_audio_rx.c/h       — RX handler, frame protocol, decode
│   ├── modules/
│   │   ├── ux/
│   │   │   ├── ux.c                — button gestures → mode cycle; APP_WIFI_STATE_CHAN → LED
│   │   │   ├── Kconfig             — CONFIG_APP_UX_MODULE, UX LED indices
│   │   │   └── CMakeLists.txt
│   │   ├── network/
│   │   │   └── net_event_app.c     — strong overrides of zego weak hooks → audio + state
│   │   ├── audio_i2s.c/h           — nRF53 I2S PCM driver wrapper
│   │   ├── audio_usb.c/h           — USB audio class (headset composite)
│   │   ├── audio_sync_timer.c/h    — RTC-based audio sync timer (nRF53 only)
│   │   ├── hw_codec.c/h            — CS47L63 HW audio codec (nRF5340 Audio DK)
│   │   ├── sd_card.c/h             — SD card FAT access (present; disabled by default)
│   │   └── sd_card_playback.c/h    — WAV playback thread from SD (present; disabled by default)
│   ├── net/
│   │   └── socket_utils.c/h        — UDP socket (server/client), TX/RX thread, peer resolution
│   ├── drivers/
│   │   └── cs47l63_comm.c/h        — CS47L63 SPI comms driver
│   └── utils/
│       ├── nrf5340_audio_dk.c/h    — board init: NVMC, clock, board_version (nRF53)
│       ├── nrf54l_init.c           — board init: (nRF54LM20A — minimal; LEDs/buttons via bricks)
│       ├── uicr.c/h                — UICR channel read/write via NVMC (nRF53)
│       ├── uicr_stub.c             — no-op UICR stubs (non-nRF53)
│       ├── board_version.c/h       — ADC board version detection (nRF5340 Audio DK)
│       ├── channel_assignment.c/h  — L/R/GW channel selection
│       └── error_handler.c         — fatal error handler
└── zego/bricks/ (read-only, via EXTRA_ZEPHYR_MODULES)
    ├── button/     — gesture classification → BUTTON_CHAN
    ├── led/        — LED state machine → LED_CMD_CHAN
    ├── wifi/       — mode persistence, WIFI_MODE_CHAN, shell command
    ├── network/    — Wi-Fi lifecycle, WPA supplicant, weak-hook API
    └── memonitor/  — heap/stack watermark sampler, MEMONITOR_CHAN
```

> **SD card disabled by default:** `sd_card.c/h` and `sd_card_playback.c/h` are still in
> the tree but are no longer built — `CONFIG_NRF5340_AUDIO_SD_CARD_MODULE` was removed from
> `boards/nrf5340_audio_dk_nrf5340_cpuapp.conf`. Re-enable the Kconfig to build them back in.
>
> **`src/debug/` removed:** the former `src/debug/` directory (heaps_monitor) was deleted;
> heap/stack watermark sampling is now provided by the zego/memonitor brick.

---

## Zbus Channels

| Channel | Message type | Publisher(s) | Subscriber(s) | Notes |
|---|---|---|---|---|
| `BUTTON_CHAN` | `struct zego_button_msg` | zego/button brick | ux.c | Gestures: SINGLE_CLICK, LONG_PRESS |
| `WIFI_MODE_CHAN` | `struct zego_wifi_mode_msg` | zego/wifi brick | zego/network, ux.c | Published once at SYS_INIT |
| `LED_CMD_CHAN` | `struct zego_led_cmd` | ux.c, net_event_app.c | zego/led brick | Commands: ON, ROTATE, BLINK |
| `APP_WIFI_STATE_CHAN` | `struct app_wifi_state_msg` | net_event_app.c | ux.c | States: CONNECTING/CONNECTED/ERROR |
| `MEMONITOR_CHAN` | `struct zego_memonitor_msg` | zego/memonitor brick | status shell command | Snapshot every INTERVAL_MS |
| `le_audio_chan` | `struct le_audio_msg` | wifi_audio_rx, main | le_audio_evt_sub | Stream START/STOP events |
| `button_chan` | `struct button_msg` | (legacy, during transition) | button_msg_sub_thread | Audio volume/play; retired in Step 3.5 |
| `volume_chan` | `struct volume_msg` | main (from button sub) | hw_codec | VOLUME_UP/DOWN/SET/MUTE |
| `sdu_ref_msg` | `struct sdu_ref_msg` | audio_datapath | sdu_ref_msg_listen | TX sync timestamp for drift compensation |

---

## Message Definitions

`APP_WIFI_STATE_CHAN` is defined in `src/modules/network/net_event_app.c` (or a shared `src/messages.h`):

```c
enum app_wifi_state {
    APP_WIFI_STATE_CONNECTING = 0,
    APP_WIFI_STATE_CONNECTED,
    APP_WIFI_STATE_ERROR,
};

struct app_wifi_state_msg {
    enum app_wifi_state state;
    enum zego_wifi_mode mode;   /* from WIFI_MODE_CHAN — for ux to distinguish modes */
};
```

See `src/zbus_common.h` for legacy channel structs. See `zego/bricks/*/include/` for brick channel message types.

---

## Thread Budget

| Thread | Stack size config | Priority | Purpose |
|---|---|---|---|
| `button_msg_sub_thread` | `CONFIG_BUTTON_MSG_SUB_STACK_SIZE` | app | Blocks on `button_chan`, dispatches audio actions (vol/play) |
| `le_audio_msg_sub_thread` | `CONFIG_LE_AUDIO_MSG_SUB_STACK_SIZE` | app | Blocks on `le_audio_chan`, drives stream state |
| `encoder_thread` | `CONFIG_ENCODER_STACK_SIZE` | app | Runs SW codec encode loop |
| `audio_datapath_thread` | `CONFIG_AUDIO_DATAPATH_STACK_SIZE` | app | Audio drift compensation, decode dispatch |
| `socket_utils_thread` | `CONFIG_SOCKET_STACK_SIZE` | app | UDP socket bind/recv/send loop |
| `volume_msg_sub_thread` | `CONFIG_VOLUME_MSG_SUB_STACK_SIZE` | app | Blocks on `volume_chan`, calls hw_codec |
| `cs47l63_thread` | `CONFIG_CS47L63_STACK_SIZE` | high | SPI codec comms (nRF5340 Audio DK only) |

Zego brick threads (internal, not app-owned):
- zego/network: one thread for WPA supplicant sequencing and P2P reconnect
- zego/button: runs on system workqueue
- zego/led: runs on system workqueue  
- zego/memonitor: runs on system workqueue
- zego/wifi: SYS_INIT only, no persistent thread

---

## Boot Sequence

```
1. SYS_INIT (PRE_KERNEL_1): Zephyr kernel, drivers, net stack
2. SYS_INIT (PRE_KERNEL_2): nRF70 Wi-Fi driver
3. SYS_INIT (POST_KERNEL, ~41): zego/wifi brick — Wi-Fi mode selector
   — reads NVS key `app/zego_wifi_mode` for the saved mode
   — publishes WIFI_MODE_CHAN (default P2P_GO on gateway, P2P_GC on headset per role; or STA)
   — prints the app/mode banner to UART
4. SYS_INIT (POST_KERNEL, ~42): zego/network brick
   — reads WIFI_MODE_CHAN
   — registers all net_mgmt callbacks
   — waits for WPA supplicant ready (30 s timeout, bounded)
   — dispatches to the mode's startup:
     • P2P_GO — autonomous GO group + on-link DHCP server (clients get 192.168.7.x; GO is 192.168.7.1)
     • P2P_GC — auto-join the GO by its exact MAC, then DHCP (GC settles at 192.168.7.2)
     • STA    — connect to the configured AP, then DHCP + mDNS (gateway advertises, headset discovers)
5. SYS_INIT (POST_KERNEL, ~45): zego/button brick — GPIO configured
   SYS_INIT (POST_KERNEL, ~45): zego/led brick — LED hardware ready
   SYS_INIT (APPLICATION): zego/memonitor — starts sampling
   SYS_INIT (APPLICATION): ux.c — starts ROTATE animation, app_ux_ready = true
6. main() — nrf5340_audio_dk_init() / nrf54l_init() — board-specific init
7. main() — audio_system_init(), wifi_audio_rx_init()
8. main() — socket_utils_init() — spawns socket_utils_thread
   socket_utils_thread waits in zego hook (no k_sem_take on global sems)
9. main() — zbus_subscribers_create() — button, le_audio subs
10. → Network events arrive via zego weak hooks in net_event_app.c:
    zego_on_net_event_dhcp_bound (STA + P2P_GC; P2P_GC sets the fixed GO IP 192.168.7.1
    directly, STA discovers the gateway via mDNS) and
    zego_on_net_event_wifi_ap_enabled / zego_on_net_event_wifi_ap_sta_connected (P2P_GO):
      → audio_system_encoder_start()
      → publish APP_WIFI_STATE_CHAN (CONNECTED)
      → signal socket ready
→ Streaming begins
```

**Key difference from pre-refactor**: `main()` no longer calls `k_sem_take()` on
`iface_up_sem`, `wpa_supplicant_ready_sem`, `ipv4_dhcp_bond_sem`, or
`station_connected_sem`. The zego network brick owns all sequencing internally.
The audio starts from a hook, not from a sequential boot step.

---

## Memory Budget

See [3-memopt.md](3-memopt.md) for the current memory budget, stack watermarks, heap sizing, and headroom tracking.

Key footprint decisions for the dual-mode build (nRF5340 Audio DK + nRF7002EK):

- **C library is picolibc** (`CONFIG_PICOLIBC=y`, `CONFIG_NEWLIB_LIBC=n`) — saves ~15 KB flash / ~14 KB RAM vs newlib, which is what makes the dual-mode image fit.
- **Socket pools raised to 8** (`CONFIG_NET_MAX_CONN=8`, `CONFIG_NET_MAX_CONTEXTS=8`, up from 4/6) so the P2P_GO DHCP server can bind its socket alongside the mDNS responder, hostap, and app sockets without exhausting the pool.
- **Dual-mode flash on the nRF5340 Audio DK is ≈ 99 % of the 1016 KB app partition** — tight but fitting; do not add the Opus overlay on top of the P2P snippet (mutually exclusive).

Pre-refactor flash baselines (NCS v3.3.0, with Opus overlay):

| Config | Board | Flash used | Flash avail | Headroom |
|---|---|---|---|---|
| gateway + opus | nRF5340 Audio DK | 776 KB | 1024 KB | 248 KB |
| headset + opus | nRF5340 Audio DK | 802 KB | 1024 KB | 222 KB |
| gateway + opus | nRF7002DK | 748 KB | 1024 KB | 276 KB |
| gateway + opus | nRF54LM20DK | 722 KB | 1940 KB | 1218 KB |

> Post-refactor measurements will replace these baselines after Phase 4 validation.
> Target: ≤ 85 % nRF5340 flash, ≤ 70 % nRF54LM20A flash (NFR-002).

---

## Flash Partitions

See [2-dts-partition.md](2-dts-partition.md) for the full per-board DTS-based partition layout.

This project uses single-image (no MCUboot/OTA) DTS fixed-partitions only. `SB_CONFIG_PARTITION_MANAGER=n` is set in `sysbuild.conf`.

---

## Build Configurations (Build Matrix)
|---|---|---|---|---|---|---|
| 1 | gateway | nRF5340 Audio DK + EK | P2P_GO | PCM | P0 default | `overlay-audio-gateway.conf` |
| 2 | headset | nRF5340 Audio DK + EK | P2P_GC | PCM | P0 default | `overlay-audio-headset.conf` |
| 3 | gateway | nRF5340 Audio DK + EK | STA | opus | P0 | `overlay-audio-gateway.conf;overlay-opus.conf` |
| 4 | headset | nRF5340 Audio DK + EK | STA | opus | P0 | `overlay-audio-headset.conf;overlay-opus.conf` |
| 5 | gateway | nRF5340 Audio DK + EK | STA | PCM | P1 | `overlay-audio-gateway.conf` |
| 6 | headset | nRF5340 Audio DK + EK | STA | PCM | P1 | `overlay-audio-headset.conf` |
| 7 | gateway | nRF7002DK | P2P_GO | PCM | P2 (build) | `overlay-audio-gateway.conf` |
| ✗ | any | any | P2P+opus | — | NEVER | — |

---

## Test Points (UART Log Markers)

| Marker string | Expected at |
|---|---|
| `Current Wi-Fi Mode: STA` / `P2P_GO` / `P2P_GC` | zego/wifi brick banner — active mode selected |
| `zego_wifi_mode [...]` | zego/wifi brick banner — switch hint; printed only when >1 mode is enabled for the role |
| `[Network] WPA supplicant ready` | zego/network brick internal |
| `[Network] P2P GO started` / `P2P_GC connected` | zego/network — P2P link up |
| `[net_event_app] Connected — starting audio` | `zego_on_net_event_*` hook fired |
| `Audio stream started` | `audio_system_encoder_start()` called |
| `Drft comp state: CALIB` | Drift compensation entering calibration |
| `Drft comp state: STEADY` | Drift compensation converged (good sign) |
