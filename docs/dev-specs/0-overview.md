# Engineering Specs Overview - nordic-wifi-audio

## Document Information

| Field | Value |
|---|---|
| Project | Nordic Wi-Fi Audio Demo |
| Version | 2026-08-04-12-20 |
| PRD Version | 2026-08-04-10-56 |
| NCS Version | v3.4.0 |
| Target Board(s) | nRF5340 Audio DK + nRF7002EK (P0); nRF7002DK (P1, gateway only); nRF54LM20DK + nRF7002EB2 (P1, gateway only) |
| Status | In Review |

## Changelog

| Version | Summary of changes |
|---|---|
| 2026-08-04-12-20 | **Bug fix (found via hardware test):** `CONFIG_ZEGO_FACTORY_RESET_BUTTON_IDX` on nRF5340 Audio DK was left at its default (0) instead of matching `CONFIG_ZEGO_UX_BUTTON_IDX=4` (BTN5) — the 10 s hold silently did nothing on that board. See [board-init-module.md](board-init-module.md) and [ui-module.md](ui-module.md) Changelogs. |
| 2026-08-04-10-58 | Updated to PRD v2026-08-04-10-56: added FR-014 factory reset. `zego` bumped v3.4.0.2→v3.4.0.3 (adds `zego/bricks/factory_reset`). New design decisions: (1) `zego/bricks/factory_reset` registered as an `EXTRA_ZEPHYR_MODULE` (`CONFIG_ZEGO_FACTORY_RESET=y`) — erases stored Wi-Fi credentials, saved Wi-Fi mode, and P2P GO MAC, then reboots; (2) `CONFIG_ZEGO_BUTTON_LONGER_PRESS_MS=10000` enables the button brick's guarded two-tier hold on the mode-control button, so the existing 3 s mode-cycle gesture (`src/modules/ux/ux.c`'s `zego_ux_on_long_press()` override, unchanged) now fires at release instead of immediately, and is superseded by factory reset at 10 s; (3) shell trigger (`zego_factory_reset`) and button trigger both work on all boards (`CONFIG_SHELL=y` globally). See [ui-module.md](ui-module.md). |
| 2026-07-31-14-13 | Updated to PRD v2026-07-31-14-13: migrated to NCS v3.4.0 / zego v3.4.0.2. Adopted `zego/bricks/ux` — button gestures, LED 0 state machine, and the startup banner move from `src/modules/ux/ux.c` into the brick; the app now only overrides `zego_ux_on_long_press()` to keep its SoftAP-excluded mode cycle. `APP_WIFI_STATE_CHAN` (app-owned) replaced by `ZEGO_UX_WIFI_STATE_CHAN` (brick-owned); `zego_on_net_event_wifi_disconnect()` gained a `will_retry` param (ignored). P2P_GC pairing changed from compile-time `CONFIG_ZEGO_WIFI_P2P_GC_TARGET_GO_MAC` (removed by zego) to runtime double-click → WPS PBC discovery → NVS-persisted MAC (FR-013). See [ui-module.md](ui-module.md) and [network-module.md](network-module.md). OI-008 (proxy headers) resolved — `src/modules/ux/{Kconfig,button.h,led.h,wifi.h}` deleted, `<led.h>`/`<ux.h>` now resolve directly to the zego brick headers via a dedicated `zephyr_library`. |
| 2026-06-26-11-29 | Updated to PRD v2026-06-26-11-29: UAC2 runs Full-Speed on all boards (nRF54LM20DK HS forced off after HW validation); see [board-init-module.md](board-init-module.md) |
| 2026-06-26-10-00 | Updated to PRD v2026-06-26-09-55: USB audio migrated UAC1→UAC2 on all boards (see [board-init-module.md](board-init-module.md)); Open Issue #3 (UDC migration) resolved |
| 2026-06-25-13-35 | Updated to PRD v2026-06-25-13-30: single dual-mode firmware (P2P + STA-mDNS) replaces separate-binaries framing; picolibc + square-wave-tone design decisions; FR-011 SD card now off-by-default |
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

### Zego library modules (no local src/)

| Module | Provided by | Canonical spec |
|--------|-------------|----------------|
| Factory reset | `zego/bricks/factory_reset` | [zego/factory_reset ↗](https://github.com/chshzh/zego/blob/main/bricks/factory_reset/docs/factory-reset-spec.md) |

---

## 3. Architecture Summary

Pattern: **zego brick + hooks**. Application code is minimal; zego bricks own all
Wi-Fi lifecycle, mode persistence, button gestures, LED animations, and memory monitoring.
The app supplies strong overrides of the network brick's weak hooks to start/stop the
audio pipeline at the right moments.

A single **dual-mode firmware** (the default build, with `-Dnordic-wifi-audio_SNIPPET=wifi-p2p`)
supports both Wi-Fi Direct P2P and infrastructure STA in one image. On fresh flash the Gateway
boots as **P2P_GO** and the Headset as **P2P_GC**; either device can be switched to **STA**
(with mDNS auto-discovery) at runtime — the active mode is persisted in NVS and applied on a
cold reboot. A smaller STA-only image (no snippet) is available when P2P is not needed.

Audio data flows directly: hook callback → `audio_system_encoder_start()` → encoder
thread → `socket_utils_tx_data()`. Zbus carries mode/state messages between modules.

### Top Design Decisions

| # | Decision | Rationale |
|---|---|---|
| 1 | Dual-mode firmware: P2P_GO/P2P_GC default on fresh flash, STA-with-mDNS switchable at runtime | One image (default build, `-Dnordic-wifi-audio_SNIPPET=wifi-p2p`) covers both transports; zero-infrastructure P2P out of the box, infrastructure STA when a router is available — mode persisted in NVS, no reflash to switch |
| 2 | Weak-hook API (not semaphores) to start audio | zego network brick handles all WPA supplicant sequencing; app reacts to `dhcp_bound` / `ap_sta_connected` events |
| 3 | Static IPs for P2P (192.168.7.1/7.2) | P2P mode has no DHCP server on the client; mDNS unreliable over P2P link |
| 4 | Opus = STA-only overlay | P2P WPA supplicant heap + libopus working set exceed nRF5340 RAM |
| 5 | zego bricks as read-only dependency | Consistent patterns across projects; brick gaps surface as separate decisions |
| 6 | UDP transport (not TCP) | Lower latency; audio can tolerate lost frames but not head-of-line blocking |
| 7 | picolibc as the C library (`CONFIG_PICOLIBC=y`, newlib off) | Saves ~15 KB flash / ~14 KB RAM vs newlib — the headroom that lets the P2P + STA dual-mode image fit in the nRF5340's 1 MB flash |
| 8 | Local square-wave generator replaces the LC3/CMSIS-DSP `tone_gen` test-tone path | A small in-tree `square_tone_gen()` (`src/audio/audio_system.c`) matches the old `tone_gen()` contract without pulling in CMSIS-DSP, freeing flash for dual-mode |
| 9 | Adopt `zego/bricks/ux` (button gestures, LED 0, startup banner) instead of the local ux module | zego v3.4.0.2 consolidated this logic into a shared brick; app keeps only the one weak-hook override (long-press mode cycle) that differs from the brick default |
| 10 | Runtime WPS PBC pairing (double-click) replaces the compile-time P2P_GC target-GO MAC | zego v3.4.0.2 removed the compile-time MAC Kconfig; one Headset image now works with any Gateway, paired by a physical action instead of a rebuild |

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
| FR-008 | `board-init-module.md`, `audio-pipeline.md` | UAC2 audio class |
| FR-009 | `board-init-module.md`, `audio-pipeline.md` | CS47L63, I2S |
| FR-010 | `board-init-module.md`, `1-architecture.md` | Multi-board DTS overlays, build matrix |
| FR-011 | `board-init-module.md` | SD card playback (off by default — `CONFIG_NRF5340_AUDIO_SD_CARD_MODULE` removed from the board conf; source retained but unbuilt, optional via `_release.conf`) |
| FR-012 | `board-init-module.md` | LINE IN overlay |
| FR-013 | `network-module.md`, `ui-module.md` | zego/bricks/ux double-click → WPS PBC pairing; NVS-persisted GO MAC |
| FR-014 | `ui-module.md`, [zego/factory_reset ↗](https://github.com/chshzh/zego/blob/main/bricks/factory_reset/docs/factory-reset-spec.md) | Factory reset (button hold ≥ 10 s or shell command) |
| NFR-001 | `1-architecture.md`, `2-dts-partition.md` | CMakeLists, EXTRA_ZEPHYR_MODULES, flash partitions |
| NFR-002 | `3-memopt.md`, `diagnostics-module.md` | Memory budget, stack watermarks, memonitor |
| NFR-003 | `1-architecture.md` | Build configurations table |
| NFR-004 | `1-architecture.md`, all module specs | EXTRA_ZEPHYR_MODULES, brick Kconfig |
| NFR-005 | `audio-pipeline.md`, `mode-selector.md` | overlay-opus.conf gating, P2P+Opus = never |

---

## 5. Module Dependency Map

```
  zego/bricks/button ─── BUTTON_CHAN ─────────►  zego/bricks/ux
  zego/bricks/button ─── BUTTON_CHAN (BUTTON_LONGER_PRESS) ─►  zego/bricks/factory_reset
  zego/bricks/led  ◄──── LED_CMD_CHAN ─────────  zego/bricks/ux
  zego/bricks/wifi  ◄─── zego_wifi_get_mode() ──  zego/bricks/ux (single-click / long-press)
                                                    │
                                                    │ zego_ux_on_long_press() — app override
                                                    ▼
                                          src/modules/ux/ux.c (mode cycle: STA→P2P_GO→P2P_GC→STA)

  zego/bricks/network ── weak hooks ──────────►  src/modules/network/net_event_app.c
                                                     │
                                    ┌────────────────┼─────────────┐
                                    ▼                ▼             ▼
                           audio_system_    ZEGO_UX_WIFI_    socket_utils
                           encoder_start/   STATE_CHAN       peer addr
                           stop()           (Zbus)

  ZEGO_UX_WIFI_STATE_CHAN ─────────────────────────────────────►  zego/bricks/ux (LED 0)

  zego/bricks/memonitor ── MEMONITOR_CHAN ─► status shell command

  Audio data path (no Zbus — direct calls):
  [net_event_app hook] → audio_system_encoder_start()
                       → [encoder thread] → socket_utils_tx_data() → UDP → nRF70
```

> Button gestures, the LED 0 state machine, and the startup banner (`zego_ux_print_banner()`,
> called once from `main()`) are all owned by `zego/bricks/ux` as of the NCS v3.4.0 migration.
> The app only overrides the long-press hook — see [ui-module.md](ui-module.md).

> For the full Zbus channel table, see [1-architecture.md](1-architecture.md).

---

## 6. Open Issues

| # | Description | Owner | Target |
|---|---|---|---|
| 1 | Post-refactor flash and RAM measurements not yet captured; pre-refactor baselines in [3-memopt.md](3-memopt.md) | — | Phase 4.2 |
| 2 | `cs47l63_comm.c` SPI DT macro `delay` param deprecated — P1 compiler warning on nRF5340 configs | Developer | Next NCS update |
| 3 | ~~`USB_DEVICE_DRIVER` / `USB_DEVICE_STACK` Kconfig deprecated — UDC API migration needed~~ **RESOLVED 2026-06-26**: migrated to UAC2 / `USB_DEVICE_STACK_NEXT` on all boards (see [board-init-module.md](board-init-module.md)) | Developer | Done |
| 4 | ~~Volume-down button unassigned: nRF5340 Audio DK sw0 reserved for Wi-Fi mode; future fix = double-click gesture~~ **SUPERSEDED 2026-07-31**: double-click on the mode button is now used by `zego/bricks/ux` for WPS PBC pairing (FR-013); volume-down remains unassigned on nRF5340 Audio DK's mode-control button, but that gesture slot is no longer available for it | PM/Developer | — |
| OI-007 | wifi_utils.c retired in Step 3.3 (not 3.5 as planned) — name collision with zego/network internal wifi_utils.c forced early retirement | Acceptable; SoftAP functions already removed; no callers remain | Developer |
| OI-008 | ~~Proxy headers in src/modules/ux/{led,button,wifi}.h are verbatim copies of zego brick headers (transition workaround); must be removed when src/modules/led.h + button_handler.h are fully cleaned from include path~~ **RESOLVED 2026-07-31**: `zego/bricks/ux` adoption deleted these proxy headers entirely; `src/modules/ux/ux.c` now builds as its own `zephyr_library` so `<led.h>`/`<ux.h>` resolve directly to the zego brick headers | Done | Developer |
