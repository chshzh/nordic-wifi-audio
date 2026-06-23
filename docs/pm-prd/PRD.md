# Product Requirements Document - nordic-wifi-audio

## Document Information

| Field | Value |
|---|---|
| Product Name | Nordic Wi-Fi Audio Demo |
| Version | 2026-06-23-14-27 |
| NCS Version | v3.3.0 |
| Target Board(s) | nRF5340 Audio DK + nRF7002EK (P0); nRF7002DK (P1, gateway only); nRF54LM20DK + nRF7002EB2 (P1, gateway only) |
| Status | In Review |

> **Status values:** `Draft` → `In Review` → `Approved` → `Implemented` → `Archived`

---

## Changelog

| Version | Summary of changes |
|---|---|
| 2026-06-23-14-27 | Promote nRF7002DK and nRF54LM20DK from deferred to P1 (gateway only, build verified); role-specific boot banner (gateway/headset names); log buffer increased on nRF5340 Audio DK for reliable boot logging; README build section updated with multi-board commands; full structural reformat to PRD template |
| 2026-06-22-15-18 | Introduce P2P autoconnect as default mode; adopt zego brick library; scope Opus to STA-only overlay; clarify board scope; remove SoftAP as selectable mode; migrate Document Information to template format |
| 2026-05-27-23-14 | Initial PRD derived from existing code (Mode A reverse scan) |

---

## 1. Executive Summary

### 1.1 Product Overview

Nordic Wi-Fi Audio Demo (`nordic-wifi-audio`) is a runnable reference application demonstrating low-latency audio transport over Wi-Fi using the nRF70 Wi-Fi chip family and the nRF Connect SDK. The default experience is zero-infrastructure: power on a Gateway board and a Headset board and audio flows automatically via Wi-Fi Direct (P2P). An optional STA mode enables the same demo over an existing Wi-Fi network with Opus compression.

### 1.2 Problem Statement

Developers and product teams evaluating the nRF70 Wi-Fi chip family need a concrete, runnable audio demo that proves low-latency audio transport over Wi-Fi using Nordic hardware and the nRF Connect SDK.

Without this demo, teams must build all audio plumbing from scratch — codec selection, UDP framing, Wi-Fi Direct (P2P) pairing, mDNS service discovery — before they can answer: "Is Wi-Fi good enough for real-time audio on nRF70?"

### 1.3 Target Users

| User type | Description |
|---|---|
| Primary | Nordic developer or field applications engineer evaluating Wi-Fi audio latency |
| Secondary | Customer / partner evaluating nRF70 platform for audio products |

### 1.4 Success Metrics

| Metric | Target | How to measure |
|---|---|---|
| P2P audio stream ready | < 60 s from power-on of both boards | Time from boot to audible audio |
| End-to-end audio latency | < 200 ms | Clap test with slow-motion camera or oscilloscope |
| Glitch-free runtime | ≥ 60 s continuous under normal 2.4/5 GHz conditions | Manual listen test |
| All P0 builds succeed | Zero compiler errors on nRF5340 Audio DK gateway + headset | `west build -p` on both targets |
| P1 builds succeed | Zero compiler errors on nRF7002DK + nRF54LM20DK gateway | `west build -p` on both targets |
| nRF5340 flash utilisation | ≤ 85% for all P0 configurations | Build output flash summary |

### 1.5 Assumptions

| # | Assumption | Risk if wrong |
|---|---|---|
| A1 | Wi-Fi Direct (P2P) is not blocked by the test environment | Medium — some enterprise environments restrict P2P; STA mode is the fallback |
| A2 | nRF5340 Audio DK is the primary development and validation board for both gateway and headset roles | High — other boards support gateway role only |
| A3 | Opus + P2P is permanently out of scope (nRF5340 ≤ 1 MB flash hard constraint) | Low — constraint is architectural |
| A4 | User has NCS v3.3.0 installed | High — API differences in other versions may break the build |

---

## 2. Device Capabilities

### 2.1 Wi-Fi Connectivity

- **P2P mode (Wi-Fi Direct) — default:** Gateway creates a P2P Group Owner; Headset discovers and connects automatically by OUI prefix within 60 seconds. Static IP pair: 192.168.7.1 (Gateway) / 192.168.7.2 (Headset). No router required.
- **STA mode — optional overlay:** Both devices join an existing Wi-Fi network. Gateway advertises via mDNS (`audiogateway.local`); Headset auto-resolves hostname within 10 seconds.
- Mode is stored in NVS and survives reboot. Long press on Button 0 cycles modes (STA → P2P_GO → P2P_CLIENT).

### 2.2 Communication & Protocols

- UDP audio frames with start/end byte framing for receiver integrity.
- mDNS / DNS-SD service advertisement in STA mode (`audiogateway.local`).
- No MQTT, HTTPS, BLE, or CoAP — audio transport only.

### 2.3 Storage & Memory

- NVS used for Wi-Fi mode selection persistence across reboots.
- No Wi-Fi credential management (P2P = static link; STA credentials provided via `overlay-sta.conf` at build time).

### 2.4 Buttons & LEDs

| Hardware | nRF5340 Audio DK | nRF7002DK | nRF54LM20DK + nRF7002EB2 |
|---|---|---|---|
| Buttons | 4 (sw0–sw3) | 2 (Button 1–2) | 3+ |
| LEDs | 4 | 2 | 4 |

| Button | Behavior |
|---|---|
| Button 0 short press | Print current Wi-Fi mode to UART |
| Button 0 long press (≥ 3 s) | Cycle Wi-Fi mode (STA → P2P_GO → P2P_CLIENT), save to NVS, reboot |
| Button 1 (headset, sw1) | Volume Up |
| Button 2 (sw2) | Play / Pause audio stream |
| Button 3 (sw3) | Test tone trigger |

> **Note:** Volume Down is not available on nRF5340 Audio DK in this release; sw0 is reserved for Wi-Fi mode selection.

| LED state | Effect |
|---|---|
| Boot / connecting | Rotate (cycling effect) |
| Audio link active | Solid ON |
| Error / disconnected | Fast blink (100 ms) |

### 2.5 Audio I/O

| I/O path | Board | Notes |
|---|---|---|
| I2S + CS47L63 codec (3.5 mm jack) | nRF5340 Audio DK | Hardware codec; auto-disabled on all other boards |
| USB audio class (headset composite) | All boards | Gateway presents as USB audio device |
| LINE IN (3.5 mm) | nRF5340 Audio DK | External audio source; enabled via `overlay-gateway-linein.conf` |
| SD card WAV playback | nRF5340 Audio DK | `CONFIG_NRF5340_AUDIO_SD_CARD_MODULE=y`; graceful skip if absent |

### 2.6 Developer & Debug Features

- Boot banner includes role-specific product name (`nordic-wifi-audio-gateway` / `nordic-wifi-audio-headset`), firmware version, and MAC address.
- Zephyr shell accessible over USB UART.
- Wi-Fi mode readable at any time via Button 0 short press.

---

## 3. Functional Requirements

### P0 — Must Have

| ID | As a… | I want to… | So that… | Acceptance Criteria | Engineering Spec |
|---|---|---|---|---|---|
| FR-001 | developer | stream encoded audio from a Gateway to a Headset over Wi-Fi | I can evaluate latency and quality of Wi-Fi audio on Nordic hardware | Audio frames transmitted with < 200 ms end-to-end latency; start/end byte framing ensures frame integrity; no audible glitching under normal 2.4/5 GHz for ≥ 60 s continuous | [audio-pipeline.md](../dev-specs/audio-pipeline.md) |
| FR-002 | developer | evaluate audio quality with PCM (default) or Opus (overlay, STA only) | I can choose the right bitrate/complexity trade-off for my product | Default build uses raw PCM in P2P and STA modes; Opus enabled via `overlay-opus.conf` in STA mode only; Opus encodes at 6–320 kbps; P2P + Opus is explicitly out of scope (nRF5340 flash constraint) | [audio-pipeline.md](../dev-specs/audio-pipeline.md) |
| FR-003 | user | have each device operate in a defined Gateway or Headset role | the demo setup is unambiguous | Gateway = UDP server, captures and encodes audio; Headset = UDP client, receives and plays; role selected at build time via `overlay-audio-gateway.conf` / `overlay-audio-headset.conf` | [audio-pipeline.md](../dev-specs/audio-pipeline.md) |
| FR-004 | developer | have Gateway and Headset connect directly via Wi-Fi Direct (P2P) without a router | the demo works in any environment without infrastructure | Gateway starts as P2P Group Owner; Headset discovers by OUI prefix and connects within 60 s; static IP 192.168.7.1/7.2; P2P is the default mode on first boot | [network-module.md](../dev-specs/network-module.md), [mode-selector.md](../dev-specs/mode-selector.md) |
| FR-009 | developer | use the onboard CS47L63 codec and 3.5 mm jack on nRF5340 Audio DK | the demo works with standard headphones | I2S PCM flows between nRF5340 and CS47L63; volume adjustable via codec registers; I2S path auto-disabled on boards without hardware codec | [board-init-module.md](../dev-specs/board-init-module.md) |
| FR-010 | developer | build the same codebase for all supported hardware platforms | the demo can be demonstrated on different Nordic boards | P0: nRF5340 Audio DK gateway + headset (I2S audio); P1 gateway-only: nRF7002DK and nRF54LM20DK + nRF7002EB2 (build verified NCS v3.3.0); per-board DTS overlay and Kconfig; zero compiler errors from single `src/` tree | [board-init-module.md](../dev-specs/board-init-module.md) |

### P1 — Should Have

| ID | As a… | I want to… | So that… | Acceptance Criteria | Engineering Spec |
|---|---|---|---|---|---|
| FR-005 | developer | have both devices optionally join an existing Wi-Fi network and auto-discover each other | I can evaluate the demo in an infrastructure environment | STA mode enabled via overlay or long-press mode cycle; Gateway advertises `audiogateway.local` via mDNS; Headset resolves and connects within 10 s of both devices joining; Opus overlay is valid in STA mode only | [network-module.md](../dev-specs/network-module.md) |
| FR-006 | user | control the device with physical buttons | the demo does not require a serial terminal | Button 0 short = print Wi-Fi mode to UART; Button 0 long ≥ 3 s = cycle mode + reboot; Button 1 (headset) = Volume Up; Button 2 = Play/Pause; Button 3 = test tone; all actions debounced and reliable across all supported boards | [ui-module.md](../dev-specs/ui-module.md) |
| FR-007 | user | have LEDs reflect the device state | the demo is self-explanatory without a serial monitor | Rotate while connecting; solid ON when audio link established; fast blink on error / disconnected; consistent across all boards | [ui-module.md](../dev-specs/ui-module.md) |
| FR-008 | developer | have the Gateway accept USB audio input | any computer can feed audio into the demo without hardware modification | Gateway presents as USB audio class device (headset composite); PC microphone audio streams over Wi-Fi; received Wi-Fi audio plays on USB headphones | [board-init-module.md](../dev-specs/board-init-module.md) |

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

- Must build cleanly on NCS v3.3.0 with zero compiler errors.
- Kconfig experimental symbols are acceptable (NCS framework, not app-owned).
