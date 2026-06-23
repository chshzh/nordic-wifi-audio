# Engineering Specs — Overview

## Document Information

| Field          | Value                                                                            |
|----------------|----------------------------------------------------------------------------------|
| Project        | Nordic Wi-Fi Audio Demo                                                          |
| Version        | 2026-06-22-15-18                                                                 |
| PRD Version    | 2026-06-22-15-18                                                                 |
| NCS Version    | v3.3.0                                                                           |
| Target Board(s)| nRF5340 Audio DK + nRF7002EK (P0); nRF7002DK, nRF54LM20DK + nRF7002EB2 (build) |
| Status         | In Review                                                                        |

## Changelog

| Version          | Summary of changes                                                              |
|------------------|---------------------------------------------------------------------------------|
| 2026-06-22-15-18 | Updated to PRD v2026-06-22-15-18: zego brick architecture, P2P default mode, module map revised, zbus channel table updated |
| 2026-05-27-23-14 | Initial specs derived from code via Mode C Reverse Design                       |

---

## Spec Index

| File                     | Description                                                           | PRD sections covered                  |
|--------------------------|-----------------------------------------------------------------------|---------------------------------------|
| `overview.md`            | This file — spec index, module map, design decisions                  | All                                   |
| `architecture.md`        | Module map, Zbus channels, threads, boot sequence, memory budget      | NFR-001, NFR-002, NFR-004             |
| `audio-pipeline.md`      | SW codec, audio datapath, WiFi RX, drift compensation, peer resolution| FR-001, FR-002, FR-009                |
| `network-module.md`      | zego-network consumption: weak hooks → audio + state channel          | FR-001, FR-003, FR-004, FR-005        |
| `mode-selector.md`       | Wi-Fi mode persistence (tombstone → zego wifi brick)                  | FR-004, FR-005, NFR-004               |
| `ui-module.md`           | App ux module: button gestures, LED state machine                     | FR-006, FR-007                        |
| `diagnostics-module.md`  | Memory/thread monitoring via memonitor brick, status shell command    | NFR-002, NFR-004                      |
| `board-init-module.md`   | Board init, UICR, multi-board support, zego button/LED config         | FR-008, FR-009, FR-010                |

---

## Architecture Summary

Pattern: **zego brick + hooks**. Application code is minimal; zego bricks own all
Wi-Fi lifecycle, mode persistence, button gestures, LED animations, and memory monitoring.
The app supplies strong overrides of the network brick's weak hooks to start/stop the
audio pipeline at the right moments.

Audio data flows directly: hook callback → `audio_system_encoder_start()` → encoder
thread → `socket_utils_tx_data()`. Zbus carries mode/state messages between modules.

### Top Design Decisions

| # | Decision | Rationale |
|---|----------|-----------|
| 1 | P2P_GO/P2P_CLIENT as the default mode | Zero-infrastructure; power on two boards, audio flows — no router, no credentials |
| 2 | Weak-hook API (not semaphores) to start audio | zego network brick handles all WPA supplicant sequencing; app reacts to `dhcp_bound` / `ap_sta_connected` events |
| 3 | Static IPs for P2P (192.168.7.1/7.2) | P2P mode has no DHCP server on the client; mDNS unreliable over P2P link |
| 4 | Opus = STA-only overlay | P2P WPA supplicant heap + libopus working set exceed nRF5340 RAM |
| 5 | zego bricks as read-only dependency | Consistent patterns across projects; brick gaps surface as separate decisions |
| 6 | UDP transport (not TCP) | Lower latency; audio can tolerate lost frames but not head-of-line blocking |

---

## PRD-to-Spec Mapping

| PRD Req  | Spec file(s)                              | Key section                               |
|----------|-------------------------------------------|-------------------------------------------|
| FR-001   | `audio-pipeline.md`, `network-module.md`  | UDP framing, audio data path, weak hooks  |
| FR-002   | `audio-pipeline.md`                       | Codec abstraction, STA-only Opus gating   |
| FR-003   | `network-module.md`                       | Socket role (server/client)               |
| FR-004   | `network-module.md`, `mode-selector.md`   | P2P_GO auto-start, Kconfig default mode   |
| FR-005   | `network-module.md`, `audio-pipeline.md`  | STA connect, mDNS, STA peer resolution    |
| FR-006   | `ui-module.md`                            | Button gestures, mode cycle               |
| FR-007   | `ui-module.md`                            | LED state machine driven by APP_WIFI_STATE_CHAN |
| FR-008   | `board-init-module.md`, `audio-pipeline.md` | USB audio class                         |
| FR-009   | `board-init-module.md`, `audio-pipeline.md` | CS47L63, I2S                            |
| FR-010   | `board-init-module.md`, `architecture.md` | Multi-board DTS overlays, build matrix   |
| FR-011   | `board-init-module.md`                    | SD card playback                          |
| FR-012   | `board-init-module.md`                    | LINE IN overlay                           |
| NFR-001  | `architecture.md`                         | CMakeLists, EXTRA_ZEPHYR_MODULES          |
| NFR-002  | `architecture.md`, `diagnostics-module.md`| Memory budget, memonitor                  |
| NFR-003  | `architecture.md`                         | Build configurations table                |
| NFR-004  | `architecture.md`, all module specs       | EXTRA_ZEPHYR_MODULES, brick Kconfig       |
| NFR-005  | `audio-pipeline.md`, `mode-selector.md`   | overlay-opus.conf gating, P2P+Opus = never|

---

## Module Dependency Map

```
  zego/bricks/wifi  ──── WIFI_MODE_CHAN ────►  src/modules/ux/ux.c
  zego/bricks/button ─── BUTTON_CHAN ─────────►  (mode cycle, print)
  zego/bricks/led  ◄──── LED_CMD_CHAN ─────────  src/modules/ux/ux.c
  zego/bricks/network ── weak hooks ──────────►  src/modules/network/net_event_app.c
                                                     │
                                    ┌────────────────┼─────────────┐
                                    ▼                ▼             ▼
                           audio_system_    APP_WIFI_STATE_  socket_utils
                           encoder_start/   CHAN (Zbus)      peer addr
                           stop()

  APP_WIFI_STATE_CHAN ─────────────────────────────────────────►  ux.c (LED)

  zego/bricks/memonitor ── MEMONITOR_CHAN ─► status shell command

  Audio data path (no Zbus — direct calls):
  [net_event_app hook] → audio_system_encoder_start()
                       → [encoder thread] → socket_utils_tx_data() → UDP → nRF70
```

## Zbus Channels

| Channel              | Message type                   | Publisher(s)           | Subscriber(s)          | Notes                          |
|----------------------|--------------------------------|------------------------|------------------------|--------------------------------|
| `BUTTON_CHAN`        | `struct zego_button_msg`       | zego/button brick      | ux.c                   | Gestures: SINGLE_CLICK, LONG_PRESS |
| `WIFI_MODE_CHAN`     | `struct zego_wifi_mode_msg`    | zego/wifi brick        | zego/network, ux.c     | Published once at SYS_INIT     |
| `LED_CMD_CHAN`       | `struct zego_led_cmd`          | ux.c, net_event_app.c  | zego/led brick         | Commands: ON, ROTATE, BLINK    |
| `APP_WIFI_STATE_CHAN`| `struct app_wifi_state_msg`    | net_event_app.c        | ux.c                   | States: CONNECTING/CONNECTED/ERROR |
| `MEMONITOR_CHAN`     | `struct zego_memonitor_msg`    | zego/memonitor brick   | status shell command   | Snapshot every INTERVAL_MS     |
| `le_audio_chan`      | `struct le_audio_msg`          | wifi_audio_rx, main    | le_audio_evt_sub       | Stream START/STOP events       |
| `button_chan`        | `struct button_msg`            | legacy button_handler  | button_msg_sub_thread  | Audio volume/play buttons; migrated after Step 3.2 |
| `volume_chan`        | `struct volume_msg`            | main (from button sub) | hw_codec               | VOLUME_UP/DOWN/SET/MUTE        |
| `sdu_ref_msg`        | `struct sdu_ref_msg`           | audio_datapath         | sdu_ref_msg_listen     | TX sync timestamp for drift compensation |

> **Note on dual button channels**: During transition (Steps 3.1–3.2), both button channels
> coexist. After Step 3.5 retires legacy button_handler, audio volume/play buttons move to ux.c
> subscribing to `BUTTON_CHAN` (zego). The `button_chan` is then retired.

---

## Open Issues

| # | Issue | Impact | Owner |
|---|-------|--------|-------|
| OI-001 | `cs47l63_comm.c` SPI DT macro `delay` param deprecated | P1 compiler warning on nRF5340 configs | Developer |
| OI-002 | `USB_DEVICE_DRIVER` / `USB_DEVICE_STACK` Kconfig deprecated | P2 warning — UDC API migration needed in future NCS update | Developer |
| OI-003 | `dhcp_bound` fires for P2P_CLIENT from CONNECT_RESULT (static IP) — confirmed from zego network-spec.md | Resolved ✓ | — |
| OI-004 | FR-013/014/015 (persistent pairing, per-mode profiles, auto-recovery) | Not in PRD or code; not in this refactor scope | PM |
| OI-005 | STA headset mDNS retry: socket thread starts before DHCP completes; initial 3 mDNS attempts may fail | Fixed in Step 3.5: `while (!serveraddr_set_signall)` loop now retries mDNS every 5 s — resolves the ordering dependency that ipv4_dhcp_bond_sem previously provided | Developer |
| OI-006 | Volume-down button lost: nRF5340 Audio DK sw0 (button 0) is now the mode button; volume-down is unassigned | PRD FR-006 updated; future fix = double-click gesture or button remap | PM/Developer |
| OI-007 | wifi_utils.c retired in Step 3.3 (not 3.5 as planned) — name collision with zego/network internal wifi_utils.c forced early retirement | Acceptable; SoftAP functions already removed; no callers remain | Developer |
| OI-008 | Proxy headers in src/modules/ux/{led,button,wifi}.h are verbatim copies of zego brick headers (transition workaround); must be removed when src/modules/led.h + button_handler.h are fully cleaned from include path | Step 3.5 complete; proxy headers could now be replaced with direct includes — schedule for minor cleanup | Developer |
