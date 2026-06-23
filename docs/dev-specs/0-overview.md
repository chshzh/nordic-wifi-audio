# Engineering Specs Overview - nordic-wifi-audio

## Document Information

| Field | Value |
|---|---|
| Project | Nordic Wi-Fi Audio Demo |
| Version | 2026-06-23-14-48 |
| PRD Version | 2026-06-23-14-27 |
| NCS Version | v3.3.0 |
| Target Board(s) | nRF5340 Audio DK + nRF7002EK (P0); nRF7002DK (P1, gateway only); nRF54LM20DK + nRF7002EB2 (P1, gateway only) |
| Status | In Review |

## Changelog

| Version | Summary of changes |
|---|---|
| 2026-06-23-14-48 | Renamed to 0-overview.md; architecture.md to 1-architecture.md; added 2-dts-partition.md and 3-memopt.md to Spec Index; added Purpose section; renumbered sections |
| 2026-06-23-14-27 | Synced to PRD v2026-06-23-14-27: nRF7002DK + nRF54LM20DK promoted to P1; role-specific banner names (gateway/headset) in CMakeLists.txt; CONFIG_LOG_BUFFER_SIZE=4096 on nRF5340 Audio DK; README build section rewritten for multi-board |
| 2026-06-22-15-18 | Updated to PRD v2026-06-22-15-18: zego brick architecture, P2P default mode, module map revised, zbus channel table updated |
| 2026-05-27-23-14 | Initial specs derived from code via Mode C Reverse Design |

---

## 1. Purpose

This document is the entry point for the engineering specs of `nordic-wifi-audio`.
It maps product requirements to spec files and captures top-level design decisions.

For the product requirements that drive this design, see [../pm-prd/PRD.md](../pm-prd/PRD.md).

---

## 2. Spec Index

| File | Description | PRD sections covered |
|---|---|---|
| [0-overview.md](0-overview.md) | This file — spec index, design decisions, PRD-to-spec mapping | All |
| [1-architecture.md](1-architecture.md) | Module map, Zbus channels, threads, boot sequence | NFR-001, NFR-004 |
| [2-dts-partition.md](2-dts-partition.md) | Flash partition layout per board, NVS capacity | NFR-001 (flash) |
| [3-memopt.md](3-memopt.md) | Memory optimization — stack watermarks, heap budget, headroom | NFR-002 |
| [audio-pipeline.md](audio-pipeline.md) | SW codec, audio datapath, WiFi RX, drift compensation, peer resolution | FR-001, FR-002, FR-009 |
| [network-module.md](network-module.md) | zego-network consumption: weak hooks → audio + state channel | FR-001, FR-003, FR-004, FR-005 |
| [mode-selector.md](mode-selector.md) | Wi-Fi mode persistence (tombstone → zego wifi brick) | FR-004, FR-005, NFR-004 |
| [ui-module.md](ui-module.md) | App ux module: button gestures, LED state machine | FR-006, FR-007 |
| [diagnostics-module.md](diagnostics-module.md) | Memory/thread monitoring via memonitor brick, status shell command | NFR-002, NFR-004 |
| [board-init-module.md](board-init-module.md) | Board init, UICR, multi-board support, zego button/LED config | FR-008, FR-009, FR-010 |

---

## 3. Architecture Summary

Pattern: **zego brick + hooks**. Application code is minimal; zego bricks own all
Wi-Fi lifecycle, mode persistence, button gestures, LED animations, and memory monitoring.
The app supplies strong overrides of the network brick's weak hooks to start/stop the
audio pipeline at the right moments.

Audio data flows directly: hook callback → `audio_system_encoder_start()` → encoder
thread → `socket_utils_tx_data()`. Zbus carries mode/state messages between modules.

### Top Design Decisions

| # | Decision | Rationale |
|---|---|---|
| 1 | P2P_GO/P2P_CLIENT as the default mode | Zero-infrastructure; power on two boards, audio flows — no router, no credentials |
| 2 | Weak-hook API (not semaphores) to start audio | zego network brick handles all WPA supplicant sequencing; app reacts to `dhcp_bound` / `ap_sta_connected` events |
| 3 | Static IPs for P2P (192.168.7.1/7.2) | P2P mode has no DHCP server on the client; mDNS unreliable over P2P link |
| 4 | Opus = STA-only overlay | P2P WPA supplicant heap + libopus working set exceed nRF5340 RAM |
| 5 | zego bricks as read-only dependency | Consistent patterns across projects; brick gaps surface as separate decisions |
| 6 | UDP transport (not TCP) | Lower latency; audio can tolerate lost frames but not head-of-line blocking |

---

## 4. PRD-to-Spec Mapping

| PRD Req | Spec file(s) | Key section |
|---|---|---|
| FR-001 | `audio-pipeline.md`, `network-module.md` | UDP framing, audio data path, weak hooks |
| FR-002 | `audio-pipeline.md` | Codec abstraction, STA-only Opus gating |
| FR-003 | `network-module.md` | Socket role (server/client) |
| FR-004 | `network-module.md`, `mode-selector.md` | P2P_GO auto-start, Kconfig default mode |
| FR-005 | `network-module.md`, `audio-pipeline.md` | STA connect, mDNS, STA peer resolution |
| FR-006 | `ui-module.md` | Button gestures, mode cycle |
| FR-007 | `ui-module.md` | LED state machine driven by APP_WIFI_STATE_CHAN |
| FR-008 | `board-init-module.md`, `audio-pipeline.md` | USB audio class |
| FR-009 | `board-init-module.md`, `audio-pipeline.md` | CS47L63, I2S |
| FR-010 | `board-init-module.md`, `1-architecture.md` | Multi-board DTS overlays, build matrix |
| FR-011 | `board-init-module.md` | SD card playback |
| FR-012 | `board-init-module.md` | LINE IN overlay |
| NFR-001 | `1-architecture.md`, `2-dts-partition.md` | CMakeLists, EXTRA_ZEPHYR_MODULES, flash partitions |
| NFR-002 | `3-memopt.md`, `diagnostics-module.md` | Memory budget, stack watermarks, memonitor |
| NFR-003 | `1-architecture.md` | Build configurations table |
| NFR-004 | `1-architecture.md`, all module specs | EXTRA_ZEPHYR_MODULES, brick Kconfig |
| NFR-005 | `audio-pipeline.md`, `mode-selector.md` | overlay-opus.conf gating, P2P+Opus = never |

---

## 5. Module Dependency Map

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

> For the full Zbus channel table, see [1-architecture.md](1-architecture.md).

---

## 6. Open Issues

| # | Description | Owner | Target |
|---|---|---|---|
| 1 | Post-refactor flash and RAM measurements not yet captured; pre-refactor baselines in [3-memopt.md](3-memopt.md) | — | Phase 4.2 |
| 2 | `cs47l63_comm.c` SPI DT macro `delay` param deprecated — P1 compiler warning on nRF5340 configs | Developer | Next NCS update |
| 3 | `USB_DEVICE_DRIVER` / `USB_DEVICE_STACK` Kconfig deprecated — UDC API migration needed in future NCS update | Developer | — |
| 4 | Volume-down button unassigned: nRF5340 Audio DK sw0 reserved for Wi-Fi mode; future fix = double-click gesture | PM/Developer | — |
| OI-007 | wifi_utils.c retired in Step 3.3 (not 3.5 as planned) — name collision with zego/network internal wifi_utils.c forced early retirement | Acceptable; SoftAP functions already removed; no callers remain | Developer |
| OI-008 | Proxy headers in src/modules/ux/{led,button,wifi}.h are verbatim copies of zego brick headers (transition workaround); must be removed when src/modules/led.h + button_handler.h are fully cleaned from include path | Step 3.5 complete; proxy headers could now be replaced with direct includes — schedule for minor cleanup | Developer |
