# Product Requirements Document - nordic-wifi-audio

## Document Information

| Field | Value |
|---|---|
| Product Name | Nordic Wi-Fi Audio |
| Version | 2026-08-07-15-52 |
| NCS Version | v3.4.0 |
| Target Board(s) | nRF5340 Audio DK + nRF7002EK (P0); nRF7002DK (P1, gateway only); nRF54LM20DK + nRF7002EB2 (P1, gateway only) |
| Status | Approved |

> **Status values:** `Draft` → `In Review` → `Approved` → `Implemented` → `Archived`

---

## Changelog

| Version | Summary of changes |
|---|---|
| 2026-08-07-15-52 | Synced PRD with code changes since the last revision: (1) Gateway boards gained a physical Play/Pause button — nRF7002DK Button 2/SW1 (idx 1) and nRF54LM20DK BUTTON1 (idx 1) — mirroring the existing Headset gesture (§2.4 updated); wire protocol commands renamed AUDIO_PLAY/AUDIO_STOP → REQ_PLAY/REQ_PAUSE. (2) Fixed a bug where a Wi-Fi client that vanished mid-stream (e.g. power-cut) could take up to ~300 s to be detected and reconnected; an app-level 15 s liveness eviction plus an always-on keepalive now recovers audio in ~15-20 s. (3) Reduced the Headset's audio jitter-buffer depth from ~80 ms to ~10 ms based on real hardware gap measurements, cutting steady-state end-to-end audio latency by ~70 ms (FR-001). |
| 2026-08-06-21-23 | Reversed the 21-04 fix per explicit user direction: RGB2 (idx 3–5) is Wi-Fi/Network Status again, RGB1 (idx 0–2) is the role indicator. Net effect vs. the pre-21-04 config: same RGB2-for-Wi-Fi assignment as originally shipped, but the conflict with the role indicator is gone since the role indicator now lives on RGB1 (not also on RGB2). Colors unchanged (Gateway green, Headset blue, per 21-10). |
| 2026-08-06-21-10 | Swapped RGB2 role indicator colors: Gateway is now Green (was Blue), Headset is now Blue (was Green). `role_led_init()` in `src/modules/audio_led/audio_led.c` updated accordingly. |
| 2026-08-06-21-04 | Bug fix: `boards/nrf5340_audio_dk_nrf5340_cpuapp.conf` had `CONFIG_ZEGO_UX_ROTATE_FIRST_LED`/`CONFIG_ZEGO_UX_CONNECTED_LED` pointed at RGB2 (idx 3-5), the same physical LEDs as the new RGB2 role indicator (FR-015 changelog above) — the two fought over RGB2, so the role color kept getting overwritten by Wi-Fi ROTATE/CONNECTED updates (this is why RGB2 never showed the role color reliably). Moved the Wi-Fi status ROTATE/CONNECTED/ERROR/PAIRING LEDs back to RGB1 (idx 0-2), matching what §2.4 always documented; RGB2 is now exclusively owned by `role_led_init()`. |
| 2026-08-06-20-48 | Implemented the nRF5340 Audio DK RGB2 role indicator (§2.4) — documented since 2026-06-22 but never actually coded (the original Kconfig-based mechanism was deleted during the v3.4.0 migration to `zego/bricks/ux` and the PRD text was never updated to match). Now set once at boot: RGB2 solid blue for Gateway, solid green for Headset (`role_led_init()` in `src/modules/audio_led/audio_led.c`). No-op on nRF7002DK/nRF54LM20DK (no RGB2 hardware). |
| 2026-08-06-19-30 | Refined FR-015's Headset LED (idx 6) to a 3rd state, based on hardware log evidence: the headset was blinking even while no audio was actually flowing (stalled on the gateway side, jitter buffer empty), because "streaming" only reflected command intent, not real playback. Now: Solid OFF once a pause command is sent; Solid ON once a play command is sent but audio isn't flowing yet (stalled/buffering); Blinking only while audio is actually playing. Gateway LED behavior (idx 1 / idx 6) is unchanged. |
| 2026-08-06-19-00 | Renamed LED 0 from "Wi-Fi / audio connection state" to **Wi-Fi / Network Status LED** (FR-007) — its states (ROTATE / Solid ON / Fast BLINK) now describe network connectivity only, not any audio condition; "Audio link active" → "Wi-Fi connected / ready". This is a documentation/naming clarification only — LED 0 was already driven purely by `net_event_app.c`'s network hooks (DHCP bound, disconnect, last AP client left), never by `stream_state`, so no behavior changed. Removes ambiguity now that FR-015 owns audio-state indication on its own dedicated LED. |
| 2026-08-06-18-00 | Added FR-015 (P1): a second, audio-specific LED (separate from the existing LED 0 Wi-Fi/audio-link indicator) shows USB source and streaming activity at a glance. Gateway: LED idx 1 (nRF7002DK, nRF54LM20DK + nRF7002EB2) / idx 6 (nRF5340 Audio DK) — Solid ON when USB host audio is available, Solid OFF when no USB audio, Blinking while audio is actively streaming to a connected headset. Headset (nRF5340 Audio DK only): LED idx 6 — Blinking while streaming, Solid OFF otherwise. Both chosen indices were already listed as free in §2.4 and don't collide with LED 0 or the RGB2 role indicator. |
| 2026-08-04-10-56 | Added FR-014 (P1): factory reset via a mode-button hold (≥ 10 s) or the `zego_factory_reset` shell command — erases stored Wi-Fi credentials, saved Wi-Fi mode, and P2P GO MAC, then reboots to the fresh-flash state. `zego` bumped v3.4.0.2→v3.4.0.3 (adds `zego/bricks/factory_reset`). The 10 s hold is deliberately distinct from the existing 3 s mode-cycle gesture (FR-006) on the same button: the 3 s gesture now fires only on release when released before 10 s (guarded), and is superseded by the 10 s hold. Added a new row to the §2.4 button table for each board. |
| 2026-07-31-14-13 | Migrated to NCS v3.4.0 (from v3.3.0); adopted zego v3.4.0.2's `zego/bricks/ux` (button gestures, LED state machine, and startup banner now brick-owned — replaces the local ux module). P2P_GC pairing changed from a compile-time gateway MAC (`CONFIG_ZEGO_WIFI_P2P_GC_TARGET_GO_MAC`, removed by zego) to runtime pairing: a fresh Headset boots idle in P2P_GC, a **double-click** on the mode button runs WPS PBC discovery and joins the pairing Gateway, and the learned MAC persists to NVS for automatic reconnect on later boots/power cycles. Added FR-013 for this pairing gesture; updated FR-004 and FR-006 to match. No change to audio transport, latency, or board support. |
| 2026-06-26-11-29 | HW-validation correction: nRF54LM20DK runs UAC2 at **Full-Speed**, not High-Speed — HS enumeration broke the USB audio stream on macOS hosts (iso-OUT continuously cancelled). All boards now UAC2 @ Full-Speed; the per-board lower-latency HS benefit noted in the 09-55 entry does not apply and is deferred to future work. |
| 2026-06-26-09-55 | Migrate USB audio source from USB Audio Class 1.0 (legacy stack) to UAC2 (USB Audio Class 2.0, USBD-next stack) on all boards. UAC2 enables explicit feedback (prevents long-term sample drift) and, on nRF54LM20DK, High-Speed enumeration (125 µs vs 1 ms service interval) for lower USB-side latency. Host-compatibility caveat: UAC2 is class-native on Windows 10+, macOS, and Linux; pre-Windows-10 hosts (which UAC1 supported driver-free) are no longer covered. |
| 2026-06-25-13-30 | Reverse the "separate binaries" decision: single dual-mode firmware (default build, with wifi-p2p snippet) now supports both P2P and STA-with-mDNS, runtime-switchable. Enabled by switching to picolibc (−~15 KB flash/−~14 KB RAM) and replacing the LC3/CMSIS-DSP test-tone with a local square-wave. Renamed P2P Client → P2P_GC (Group Client) throughout. Per-role mode visibility (Gateway: STA+P2P_GO, Headset: STA+P2P_GC). Verified on HW: P2P auto-connect+stream and STA mDNS auto-discovery+stream both working. |
| 2026-06-24-15-04 | Separate P2P and STA into distinct firmware binaries; P2P built with -Dnordic-wifi-audio_SNIPPET=wifi-p2p, STA built without; mDNS DNS-SD auto-discovery implemented for STA mode (headset resolves gateway IP via PTR query); src/debug removed; SD card module disabled; memonitor simplified to ZView-only; COMPILER_OPT moved to overlay-opus.conf |
| 2026-06-23-14-27 | Promote nRF7002DK and nRF54LM20DK from deferred to P1 (gateway only, build verified); role-specific boot banner (gateway/headset names); log buffer increased on nRF5340 Audio DK for reliable boot logging; README build section updated with multi-board commands; full structural reformat to PRD template |
| 2026-06-22-15-18 | Introduce P2P autoconnect as default mode; adopt zego brick library; scope Opus to STA-only overlay; clarify board scope; remove SoftAP as selectable mode; migrate Document Information to template format |
| 2026-05-27-23-14 | Initial PRD derived from existing code (Mode A reverse scan) |

---

## 1. Executive Summary

### 1.1 Product Overview

Nordic Wi-Fi Audio Demo (`nordic-wifi-audio`) is a runnable reference application demonstrating low-latency audio transport over Wi-Fi using the nRF70 Wi-Fi chip family and the nRF Connect SDK. The default experience is zero-infrastructure: power on a Gateway board and a Headset board and audio flows automatically via Wi-Fi Direct (P2P). An optional STA mode enables the same demo over an existing Wi-Fi network. Audio is transported as raw PCM by default; an optional Opus codec overlay (`overlay-opus.conf`) is available in STA mode for reduced bandwidth at configurable bitrate (6–320 kbps).

### 1.2 Problem Statement

Developers and product teams evaluating the nRF70 Wi-Fi chip family need a concrete, runnable audio demo that proves low-latency audio transport over Wi-Fi using Nordic hardware and the nRF Connect SDK.

Without this demo, teams must build all audio plumbing from scratch — codec selection, UDP framing, Wi-Fi Direct (P2P) pairing, mDNS service discovery — before they can answer: "Is Wi-Fi good enough for real-time audio on nRF70?"

### 1.3 Target Users

| User type | Description |
|---|---|
| Primary | Nordic developer or field applications engineer evaluating Wi-Fi audio latency |
| Secondary | Customer / partner evaluating nRF70 platform for audio products |

### 1.4 Success Metrics

| Metric | Target | How to measure | Verified by |
|---|---|---|---|
| All P0 builds succeed | Zero compiler errors on nRF5340 Audio DK gateway + headset | `west build -p` on both targets | **chsh-sk-ncs-4.1-verification** — build verification step |
| P1 builds succeed | Zero compiler errors on nRF7002DK + nRF54LM20DK gateway | `west build -p` on both targets | **chsh-sk-ncs-4.1-verification** — build verification step |
| P2P audio stream ready | < 60 s from power-on of both boards | Time from boot to audible audio | **chsh-sk-ncs-4.2-validation** — hardware runtime scenario |
| End-to-end audio latency | < 200 ms | Clap test with slow-motion camera or oscilloscope | **chsh-sk-ncs-4.2-validation** — hardware runtime scenario |
| Glitch-free runtime | ≥ 60 s continuous under normal 2.4/5 GHz conditions | Manual listen test | **chsh-sk-ncs-4.2-validation** — hardware runtime scenario |

### 1.5 Assumptions

| # | Assumption | Risk if wrong |
|---|---|---|
| A1 | nRF5340 Audio DK is the primary development and validation board for both gateway and headset roles | High — other boards support gateway role only |
| A2 | User has NCS v3.4.0 installed | High — API differences in other versions may break the build |
| A3 | Wi-Fi Direct (P2P) is not blocked by the test environment | Medium — some enterprise environments restrict P2P; STA mode is the fallback |
| A4 | Opus + P2P is out of scope on nRF5340 (1 MB flash); Opus is an STA-only build option | Low — constraint is architectural |
| A5 | P2P and STA-with-mDNS coexist in a single dual-mode firmware (default build) | Low — fits ~99% of 1 MB flash after switching to picolibc and removing the LC3/CMSIS-DSP test-tone path |

---

## 2. Device Capabilities

### 2.1 Wi-Fi Connectivity

- **Single dual-mode firmware (default build):** One image supports both Wi-Fi Direct P2P and infrastructure STA; the active mode is stored in NVS and switchable at runtime (shell command or Button 0 long-press). Built with `-Dnordic-wifi-audio_SNIPPET=wifi-p2p`. A smaller STA-only image (no snippet) is available when P2P is not needed.
- **P2P mode (Wi-Fi Direct, default on fresh flash):** Gateway is the Group Owner (P2P_GO); Headset is the Group Client (P2P_GC). A fresh Headset boots idle (no gateway pinned at build time) — a **double-click** on the mode button triggers WPS PBC discovery and pairing with the Gateway; the learned MAC is persisted to NVS, so subsequent boots and reconnects are automatic with no further pairing step (see FR-013). Static IP pair: 192.168.7.1 (Gateway) / 192.168.7.2 (Headset), served by the GO's DHCP server. No router required.
- **STA mode (infrastructure Wi-Fi):** Both devices join an existing Wi-Fi network. The Gateway advertises an audio service via mDNS DNS-SD; the Headset discovers the Gateway's DHCP-assigned IP automatically via a DNS-SD PTR→SRV→A query — no hardcoded address. Credentials stored in NVS.
- **Per-role mode availability:** Gateway exposes STA + P2P_GO; Headset exposes STA + P2P_GC.

### 2.2 Communication & Protocols

- UDP audio frames with start/end byte framing for receiver integrity.
- mDNS / DNS-SD service advertisement in STA mode (`audiogateway.local`).
- No MQTT, HTTPS, BLE, or CoAP — audio transport only.

### 2.3 Storage & Memory

- NVS used for Wi-Fi mode selection persistence across reboots.
- STA Wi-Fi credentials entered at runtime via Zephyr shell (`wifi connect` / `wifi cred` commands provided by the zego Wi-Fi brick); credentials persist in NVS across reboots. No static credential baking at build time.

### 2.4 Buttons & LEDs

| Hardware | nRF5340 Audio DK + nRF7002EK | nRF7002DK | nRF54LM20DK + nRF7002EB2 |
|---|---|---|---|
| Buttons | 5 (VOL-, VOL+, PLAY/PAUSE, BTN4, BTN5) | 2 (Button 1–2) | 3 (BUTTON0–2) |
| LEDs | 9 (idx 0–8) | 2 | 4 |

### Buttons

| Board | Button | Gesture | Action |
|---|---|---|---|
| nRF5340 Audio DK + nRF7002EK | VOL- (idx 0) | Single click | Volume Down  |
| | VOL+ (idx 1) | Single click | Volume Up |
| | PLAY/PAUSE (idx 2) | Single click | Play / Pause audio stream |
| | BTN4 (idx 3) | Single click | Trigger test tone |
| | BTN5 (idx 4) | Single click | Print current Wi-Fi state to UART |
| | | Double-click | Trigger WPS PBC pairing (P2P modes only; see FR-013) |
| | | Long press ≥ 3 s (fires at release) | Cycle mode (STA → P2P_GO → P2P_GC); save to NVS; reboot |
| | | Hold ≥ 10 s (fires immediately, no release needed) | Factory reset (FR-014) — erase stored Wi-Fi credentials, saved Wi-Fi mode, and P2P GO MAC, then reboot; supersedes the 3 s mode cycle for that press |
| nRF7002DK | Button 1 / SW0 (idx 0) | Single click | Print current Wi-Fi state to UART |
| | | Double-click | Trigger WPS PBC pairing (P2P modes only; see FR-013) |
| | | Long press ≥ 3 s (fires at release) | Cycle mode (STA → P2P_GO → P2P_GC); save to NVS; reboot |
| | | Hold ≥ 10 s (fires immediately, no release needed) | Factory reset (FR-014) — erase stored Wi-Fi credentials, saved Wi-Fi mode, and P2P GO MAC, then reboot; supersedes the 3 s mode cycle for that press |
| | Button 2 / SW1 (idx 1) | Single click | Play / Pause audio stream |
| nRF54LM20DK + nRF7002EB2 | BUTTON0 (idx 0) | Single click | Print current Wi-Fi state to UART |
| | | Double-click | Trigger WPS PBC pairing (P2P modes only; see FR-013) |
| | | Long press ≥ 3 s (fires at release) | Cycle mode (STA → P2P_GO → P2P_GC); save to NVS; reboot |
| | | Hold ≥ 10 s (fires immediately, no release needed) | Factory reset (FR-014) — erase stored Wi-Fi credentials, saved Wi-Fi mode, and P2P GO MAC, then reboot; supersedes the 3 s mode cycle for that press |
| | BUTTON1 (idx 1) | Single click | Play / Pause audio stream |
| | BUTTON2 (idx 2) | Any | Available (no default audio function — gateway only) |


### LEDs

LED 0 reflects Wi-Fi / network connection status only — it no longer indicates
any audio state. A second, audio-specific LED (idx 1 or idx 6, board-dependent
— see FR-015) shows USB source / streaming activity. All remaining LEDs are
available for application use.

| Board | LED 0 (idx 0) — Wi-Fi / Network Status | Audio Streaming LED (FR-015) | Other LEDs |
|---|---|---|---|
| nRF5340 Audio DK + nRF7002EK | RGB2 R/G/B (idx 3–5) — ROTATE for Wi-Fi/network state | LED1 (idx 6) | RGB1 (idx 0–2) — role indicator (see below); LED2–3 (idx 7–8) — free |
| nRF7002DK | LED1 — Wi-Fi / network status | LED2 (idx 1) | — |
| nRF54LM20DK + nRF7002EB2 | LED0 — Wi-Fi / network status | LED1 (idx 1) | LED2–3 (idx 2–3) — free |

nRF7002DK and nRF54LM20DK + nRF7002EB2 LED 0 (Wi-Fi / Network Status) state effects:

| State | Effect |
|---|---|
| Boot / connecting | ROTATE |
| Wi-Fi connected / ready | Solid ON |
| Error / disconnected | Fast BLINK (100 ms half-period) |

nRF5340 Audio DK + nRF7002EK RGB2 (Wi-Fi / Network Status) state effects:

| State | Effect |
|---|---|
| Boot / connecting | RGB2 ROTATE (all three channels) |
| Wi-Fi connected / ready | RGB2 Green — Solid ON |
| Error / disconnected | RGB2 Red — Fast BLINK (100 ms half-period) |

nRF5340 Audio DK + nRF7002EK RGB1 role indicator (solid, set at boot):

| Role | RGB1 colour |
|---|---|
| Gateway | Green |
| Headset | Blue |

Audio Streaming LED (FR-015) state effects — Gateway (idx 1 or idx 6, board-dependent):

| State | Effect |
|---|---|
| USB host audio available, not yet streaming to a headset | Solid ON |
| No USB host audio | Solid OFF |
| Actively streaming to a connected headset | Blinking |

Audio Streaming LED (FR-015) state effects — Headset (idx 6, nRF5340 Audio DK only):

| State | Effect |
|---|---|
| Pause command sent | Solid OFF |
| Play command sent, no audio actually flowing yet (stalled/buffering) | Solid ON |
| Audio actually playing | Blinking |

### 2.5 Audio I/O

| I/O path | Board | Notes |
|---|---|---|
| I2S + CS47L63 codec (3.5 mm jack) | nRF5340 Audio DK | Hardware codec; auto-disabled on all other boards |
| USB audio class (UAC2 headset composite) | All boards | Gateway presents as a USB Audio Class 2.0 device; class-native on Windows 10+/macOS/Linux. Full-Speed on all boards (nRF54LM20DK is HS-capable but forced to Full-Speed — see specs). |
| LINE IN (3.5 mm) | nRF5340 Audio DK | External audio source; enabled via `overlay-gateway-linein.conf` |
| SD card WAV playback | nRF5340 Audio DK | `CONFIG_NRF5340_AUDIO_SD_CARD_MODULE=y`; graceful skip if absent |

### 2.6 Developer & Debug Features

- Boot banner includes role-specific product name (`nordic-wifi-audio-gateway` / `nordic-wifi-audio-headset`), firmware version, and MAC address.

---

## 3. Functional Requirements

### P0 — Must Have

| ID | As a… | I want to… | So that… | Acceptance Criteria | Engineering Spec |
|---|---|---|---|---|---|
| FR-001 | developer | stream encoded audio from a Gateway to a Headset over Wi-Fi | I can evaluate latency and quality of Wi-Fi audio on Nordic hardware | Audio frames transmitted with < 200 ms end-to-end latency; start/end byte framing ensures frame integrity; no audible glitching under normal 2.4/5 GHz for ≥ 60 s continuous | [audio-pipeline.md](../dev-specs/audio-pipeline.md) |
| FR-002 | developer | evaluate audio quality with PCM (default) or Opus (overlay, STA only) | I can choose the right bitrate/complexity trade-off for my product | Default build uses raw PCM in P2P and STA modes; Opus enabled via `overlay-opus.conf` in STA mode only; Opus encodes at 6–320 kbps; P2P + Opus is explicitly out of scope (nRF5340 flash constraint) | [audio-pipeline.md](../dev-specs/audio-pipeline.md) |
| FR-003 | user | have each device operate in a defined Gateway or Headset role | the demo setup is unambiguous | Gateway = UDP server, captures and encodes audio; Headset = UDP client, receives and plays; role selected at build time via `overlay-audio-gateway.conf` / `overlay-audio-headset.conf` | [audio-pipeline.md](../dev-specs/audio-pipeline.md) |
| FR-004 | developer | have Gateway and Headset connect directly via Wi-Fi Direct (P2P) without a router | the demo works in any environment without infrastructure | Gateway starts as P2P Group Owner (P2P_GO); Headset (P2P_GC) pairs with the GO via the double-click gesture (see FR-013), then connects within 60 s; GO runs a DHCP server assigning static IP 192.168.7.1/7.2; P2P is the default mode on first boot | [network-module.md](../dev-specs/network-module.md), [mode-selector.md](../dev-specs/mode-selector.md) |
| FR-013 | user | pair a fresh Headset with its Gateway without reflashing or a hardcoded MAC | the same Headset image works with any Gateway, and re-pairing is a physical action, not a rebuild | Headset (P2P_GC) boots idle with no saved Gateway; double-click on the mode button runs WPS PBC discovery and joins the pairing-window Gateway (P2P_GO); the learned Gateway MAC persists to NVS and is used automatically on later boots and reconnects — no re-pairing needed unless a different Gateway is desired | [network-module.md](../dev-specs/network-module.md), [ui-module.md](../dev-specs/ui-module.md) |
| FR-009 | developer | use the onboard CS47L63 codec and 3.5 mm jack on nRF5340 Audio DK | the demo works with standard headphones | I2S PCM flows between nRF5340 and CS47L63; volume adjustable via codec registers; I2S path auto-disabled on boards without hardware codec | [board-init-module.md](../dev-specs/board-init-module.md) |
| FR-010 | developer | build the same codebase for all supported hardware platforms | the demo can be demonstrated on different Nordic boards | P0: nRF5340 Audio DK gateway + headset (I2S audio); P1 gateway-only: nRF7002DK and nRF54LM20DK + nRF7002EB2 (build verified NCS v3.4.0); per-board DTS overlay and Kconfig; zero compiler errors from single `src/` tree | [board-init-module.md](../dev-specs/board-init-module.md) |

### P1 — Should Have

| ID | As a… | I want to… | So that… | Acceptance Criteria | Engineering Spec |
|---|---|---|---|---|---|
| FR-005 | developer | have both devices optionally join an existing Wi-Fi network and auto-discover each other | I can evaluate the demo in an infrastructure environment | STA mode enabled via overlay or long-press mode cycle; Gateway advertises `audiogateway.local` via mDNS; Headset resolves and connects within 10 s of both devices joining; Opus overlay is valid in STA mode only | [network-module.md](../dev-specs/network-module.md) |
| FR-006 | user | control the device with physical buttons | the demo does not require a serial terminal | Mode button single click = print Wi-Fi mode to UART; double-click = trigger WPS PBC pairing in P2P modes (see FR-013); long press ≥ 3 s = cycle mode + reboot; Button 1 (headset) = Volume Up; Button 2 = Play/Pause; Button 3 = test tone; all actions debounced and reliable across all supported boards | [ui-module.md](../dev-specs/ui-module.md) |
| FR-007 | user | have LEDs reflect the device state | the demo is self-explanatory without a serial monitor | LED 0 (Wi-Fi / Network Status): Rotate while connecting; solid ON when Wi-Fi is connected/ready; fast blink on error / disconnected; consistent across all boards; does not reflect audio streaming state (see FR-015) | [ui-module.md](../dev-specs/ui-module.md) |
| FR-008 | developer | have the Gateway accept USB audio input | any computer can feed audio into the demo without hardware modification | Gateway presents as a UAC2 (USB Audio Class 2.0) device (headset composite), class-native on Windows 10+/macOS/Linux; PC microphone audio streams over Wi-Fi; received Wi-Fi audio plays on USB headphones | [board-init-module.md](../dev-specs/board-init-module.md) |
| FR-014 | developer | factory-reset the device back to its as-flashed state | I can recover a misconfigured device or hand it off clean without reflashing | Holding the mode button for ≥ 10 s, or running the `zego_factory_reset` shell command, erases stored Wi-Fi credentials, the saved Wi-Fi mode, and the learned P2P GO MAC, then reboots; the 10 s hold is distinct from the existing 3 s mode-cycle gesture (FR-006) on the same button — releasing before 10 s still cycles the mode (now at release instead of immediately), holding to 10 s supersedes it and performs the reset instead | [ui-module.md](../dev-specs/ui-module.md), [zego/factory_reset ↗](https://github.com/chshzh/zego/blob/main/bricks/factory_reset/docs/factory-reset-spec.md) |
| FR-015 | user | see USB audio availability and streaming activity on a dedicated LED | I can tell at a glance whether audio is available from the PC and whether it's actually flowing to a headset, without a serial monitor | Gateway: LED idx 1 (nRF7002DK, nRF54LM20DK + nRF7002EB2) / idx 6 (nRF5340 Audio DK) — Solid ON when USB host audio is available but not yet streaming, Solid OFF when no USB host audio, Blinking while actively streaming to a connected headset. Headset (nRF5340 Audio DK only): LED idx 6 — Blinking while audio is actually playing; Solid ON while a play command was sent but no audio is flowing yet (stalled/buffering); Solid OFF once a pause command was sent. Distinct from and independent of LED 0 (Wi-Fi / Network Status, FR-007), which no longer reflects audio state | [ui-module.md](../dev-specs/ui-module.md), [audio-pipeline.md](../dev-specs/audio-pipeline.md) |

### P2 — Nice to Have

| ID | As a… | I want to… | So that… | Acceptance Criteria | Engineering Spec |
|---|---|---|---|---|---|
| FR-011 | developer | play audio from an SD card on nRF5340 Audio DK | the demo runs without a live PC audio source | WAV files from micro-SD read and encoded as audio source; enabled via `CONFIG_NRF5340_AUDIO_SD_CARD_MODULE=y`; gracefully disabled if no SD card present | [board-init-module.md](../dev-specs/board-init-module.md) |
| FR-012 | developer | feed audio via the 3.5 mm LINE IN jack on nRF5340 Audio DK | any audio source (phone, PC) can be used without a USB cable | LINE IN mode selected via `overlay-gateway-linein.conf`; captured audio encoded and streamed as with any other source | [board-init-module.md](../dev-specs/board-init-module.md) |

---

## 4. Non-Functional Requirements

### 4.1 Performance

| Requirement | Target |
|---|---|
| End-to-end audio latency (P2P PCM) | < 200 ms |
| P2P connection establishment | < 60 s from power-on of both boards |
| mDNS hostname resolution (STA) | < 10 s after both devices join the network |
| nRF5340 flash utilisation | ≤ 85% for all P0 build configurations |
| nRF54LM20A flash utilisation | ≤ 70% for all P1 build configurations |

### 4.2 Reliability

| Requirement | Target |
|---|---|
| Glitch-free audio runtime | ≥ 60 s continuous under normal 2.4/5 GHz conditions |
| Wi-Fi mode persistence | Mode selection survives reboot via NVS |
| Build reproducibility | `west build -p` succeeds cleanly from a fresh workspace |

### 4.3 Build System

- Single `CMakeLists.txt` covering all boards and roles.
- Board-specific hardware code gated by Kconfig (`CONFIG_SOC_SERIES_NRF53`, `CONFIG_BOARD_*`).
- No Partition Manager; DTS-based flash partitioning only (`SB_CONFIG_PARTITION_MANAGER=n`).

### 4.4 Maintainability

- Device UI (buttons, LEDs) and Wi-Fi connectivity layer built on the shared `zego` brick library: `zego/bricks/button`, `zego/bricks/led`, `zego/bricks/wifi`, `zego/bricks/network`, `zego/bricks/memonitor`.
- App consumes bricks via `EXTRA_ZEPHYR_MODULES`; brick source is not modified.
- Opus codec is an **opt-in overlay** (`overlay-opus.conf`). Default PCM build must not be broken by codec changes.
- P2P + Opus is explicitly excluded from scope (RAM/flash constraint is architectural).

### 4.5 NCS Version Compatibility

- Must build cleanly on NCS v3.4.0 with zero compiler errors.
- Kconfig experimental/deprecated symbols selected unconditionally by Nordic's own Wi-Fi driver or hostap crypto stack are acceptable (NCS framework, not app-owned); nothing in this app's own `prj.conf`/`boards/*.conf` triggers them.
