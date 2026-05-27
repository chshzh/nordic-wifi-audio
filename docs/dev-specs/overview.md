# Engineering Specs Overview — nordic-wifi-audio

## Document Information

| Field          | Value                        |
|----------------|------------------------------|
| Project        | Nordic Wi-Fi Opus Audio Demo |
| NCS Version    | v3.3.0                       |
| PRD Version    | 2026-05-27-23-14             |
| Author         | Reverse-derived from code    |
| Latest Version | 2026-05-27-23-14             |

## Changelog

| Version          | Summary of changes                                          |
|------------------|-------------------------------------------------------------|
| 2026-05-27-23-14 | Initial specs derived from code via Mode C Reverse Design   |

---

## Spec Index

| File                            | Description                                          | PRD sections covered          |
|---------------------------------|------------------------------------------------------|-------------------------------|
| `overview.md`                   | This file — spec index, module map, design decisions | All                           |
| `architecture.md`               | Module map, Zbus channels, threads, memory budget    | NFR-001, NFR-002              |
| `audio-pipeline.md`             | SW codec, audio datapath, WiFi RX, drift comp        | FR-001, FR-002, FR-009        |
| `network-module.md`             | Socket (UDP), WiFi utils, net event management       | FR-001, FR-003, FR-004, FR-005|
| `ui-module.md`                  | Button handler, LED control, channel assignment      | FR-006, FR-007                |
| `board-init-module.md`          | Board init, UICR, board version, multi-board support | FR-008, FR-009, FR-010        |

---

## Architecture Summary

**Pattern**: Multi-threaded with Zbus for UI event routing.

Audio data flow is direct (callback → thread → encode → socket TX), not routed
through Zbus. Zbus is used for button events, volume events, stream control
events, and SDU reference timestamps.

**Top design decisions:**

| # | Decision | Rationale |
|---|----------|-----------|
| 1 | UDP transport (not TCP) | Lower latency; audio can tolerate occasional lost frames but not head-of-line blocking |
| 2 | Opus codec default | Good compression at voice/music bitrates; configurable; LC3/raw selectable at build time |
| 3 | SoftAP pairing overlay | Allows demo without infrastructure AP; same codebase, different Kconfig |
| 4 | mDNS / DNS-SD discovery | Headset finds gateway by hostname, no static IP config needed |
| 5 | DTS-based partitions | `SB_CONFIG_PARTITION_MANAGER=n` — simpler single-image layout, no MCUboot |
| 6 | Board-gated compilation | `CONFIG_SOC_SERIES_NRF53` / `CONFIG_NRFX_I2S` / board macros — same src tree, 3 boards |

---

## PRD-to-Spec Mapping

| PRD Req     | Spec file(s)                          | Key section                          |
|-------------|---------------------------------------|--------------------------------------|
| FR-001      | `audio-pipeline.md`, `network-module.md` | UDP framing, audio data path      |
| FR-002      | `audio-pipeline.md`                   | SW codec (Opus), `CONFIG_SW_CODEC_OPUS` |
| FR-003      | `network-module.md`                   | Socket role (server/client)          |
| FR-004      | `network-module.md`                   | `wifi_run_softap_mode()`, credentials |
| FR-005      | `network-module.md`                   | mDNS responder, DNS-SD, resolver     |
| FR-006      | `ui-module.md`                        | `button_handler`, `button_chan` Zbus  |
| FR-007      | `ui-module.md`                        | `led_blink()`, `led_on()`            |
| FR-008      | `board-init-module.md`, `audio-pipeline.md` | USB audio class, `audio_usb.c` |
| FR-009      | `board-init-module.md`, `audio-pipeline.md` | CS47L63, `audio_i2s.c`        |
| FR-010      | `board-init-module.md`, `architecture.md` | Multi-board DTS overlays        |
| FR-011      | `board-init-module.md`                | `sd_card_playback`, FatFS            |
| FR-012      | `board-init-module.md`                | LINE IN overlay, CS47L63 routing     |
| NFR-001     | `architecture.md`                     | CMakeLists, Kconfig guards           |
| NFR-002     | `architecture.md`                     | Memory budget table                  |
| NFR-003     | `architecture.md`                     | Build configurations table           |

---

## Module Dependency Map

```
                    BUTTONS
                       │ button_chan (Zbus)
                       ▼
                 [main thread]
                  gateway: button_msg_sub_thread
                  headset: button_msg_sub_thread
                       │
          ┌────────────┼─────────────┐
          │            │             │
    volume_chan    PLAY/PAUSE    MUTE/CHAN
  (Zbus)    │         │
          ▼            │
   [hw_codec]    stream_state_set()
   CS47L63 vol         │
                       ▼
               audio_system_encoder_start/stop()
                       │
               ┌───────┴──────────────────┐
               │                          │
         [encoder thread]          [audio_datapath thread]
         SW codec (Opus)           drift compensation (RTC/GRTC)
               │                          │
               │ socket_utils_tx_data()   │ hfclkaudio_set()
               ▼                          │
        [socket_utils thread]             │
        UDP server/client                 │
               │                          │
      NET ◄────┘          wifi_audio_rx_data_handler()
      (nRF70 Wi-Fi)               ▲
                                  │ socket_recv_queue (k_msgq)
                           [socket_utils RX]
                                  │
                           [audio_datapath]
                            SW codec decode
                                  │
                            audio output
                          (I2S / USB / HW codec)

NET events:
  [net_event_mgmt] ──── semaphores ────► [wifi_audio_gateway/headset main]
                    (iface_up, dhcp_bond, wpa_ready, station_connected)
```

---

## Open Issues

| # | Issue | Impact | Owner |
|---|-------|--------|-------|
| OI-001 | `cs47l63_comm.c` SPI DT macro `delay` param deprecated | P1 compiler warning on 3 of 4 configs | Developer |
| OI-002 | `USB_DEVICE_DRIVER` / `USB_DEVICE_STACK` Kconfig deprecated | P2 Kconfig warning — requires UDC API migration in future NCS update | Developer |
| OI-003 | `CONFIG_SHELL=y` unconditional in `prj.conf` | P2 — should be in debug overlay for production | Developer |
| OI-004 | `overlay-wifi-cred-static.conf` not in `.gitignore` | P2 — credentials are clearly placeholders but file should be gitignored | Developer |
