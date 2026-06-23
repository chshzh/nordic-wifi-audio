# Product Requirements Document — Nordic Wi-Fi Audio Demo

## Document Information

| Field            | Value                                            |
|------------------|--------------------------------------------------|
| Product Name     | Nordic Wi-Fi Audio Demo                          |
| Version          | 2026-06-22-15-18                                 |
| NCS Version      | v3.3.0                                           |
| Target Board(s)  | nRF5340 Audio DK + nRF7002EK (P0); nRF7002DK, nRF54LM20DK + nRF7002EB2 (deferred) |
| Status           | In Review                                        |

## Changelog

| Version          | Summary of changes                                                                              |
|------------------|-------------------------------------------------------------------------------------------------|
| 2026-06-22-15-18 | Introduce P2P autoconnect as default mode; adopt zego brick library; scope Opus to STA-only overlay; clarify board scope; remove SoftAP as selectable mode; migrate Document Information to template format |
| 2026-05-27-23-14 | Initial PRD derived from existing code (Mode A reverse scan)                                   |

---

## 1. Problem Statement

Developers and product teams evaluating the nRF70 Wi-Fi chip family need a
concrete, runnable audio demo that proves low-latency audio transport over Wi-Fi
using Nordic hardware and the nRF Connect SDK.

Without this demo, teams must build all audio plumbing from scratch — codec
selection, UDP framing, Wi-Fi Direct (P2P) pairing, mDNS service discovery — before
they can answer the question: "Is Wi-Fi good enough for real-time audio on nRF70?"

The default experience is **zero-infrastructure**: power on two boards and audio
flows automatically via Wi-Fi Direct (P2P). An optional STA mode lets teams evaluate
the same demo over an existing Wi-Fi network with Opus compression.

## 2. Users

| User          | Description                                                    |
|---------------|----------------------------------------------------------------|
| Primary       | Nordic developer or field applications engineer evaluating Wi-Fi audio latency |
| Secondary     | Customer / partner evaluating nRF70 platform for audio products |

## 3. Functional Requirements

### FR-001 — Real-Time Audio Streaming

**As a** developer, **I want** to stream encoded audio from a Gateway device to a
Headset device over a Wi-Fi network **so that** I can evaluate the latency and
quality of Wi-Fi audio transport on Nordic hardware.

**Acceptance criteria:**
- Audio frames are transmitted from Gateway to Headset with sub-200 ms end-to-end latency under normal network conditions.
- A framing protocol (start/end byte sequences) ensures frame integrity at the receiver.
- No audible glitching under normal 2.4 GHz or 5 GHz Wi-Fi conditions for at least 60 continuous seconds.

**Priority:** P0

---

### FR-002 — Codec Support

**As a** developer, **I want** to evaluate audio quality with different codecs **so that** I can choose the right trade-off between bitrate and complexity for my product.

**Acceptance criteria:**
- Default build uses **raw PCM** (no compression) — functions in P2P and STA modes.
- **Opus codec** is available as an opt-in build overlay (`overlay-opus.conf`) in **STA mode only**.
- When Opus is enabled: encoding runs at configurable bitrates (6 kbps – 320 kbps); decoded audio plays back without perceptible artifacts at 48 kHz / 16-bit stereo.
- **P2P + Opus is explicitly out of scope** (RAM/flash constraint: WPA supplicant P2P heap + libopus working set do not fit together on nRF5340).
- Codec selection is a build-time choice; no runtime switching.

**Priority:** P0

---

### FR-003 — Two Device Roles: Gateway and Headset

**As a** user, **I want** each device to operate in a defined role (Gateway =
source/server, Headset = sink/client) **so that** the demo setup is unambiguous.

**Acceptance criteria:**
- Gateway firmware: acts as UDP server, captures audio, encodes, and streams.
- Headset firmware: acts as UDP client, receives, decodes, and plays audio.
- Role is selected at build time via `overlay-audio-gateway.conf` or `overlay-audio-headset.conf`.

**Priority:** P0

---

### FR-004 — P2P Autoconnect (Default Mode)

**As a** developer, **I want** the Gateway and Headset to connect directly to each
other over Wi-Fi Direct **so that** the demo works in any environment without a
router or pre-shared credentials.

**Acceptance criteria:**
- Gateway starts as a P2P Group Owner (GO); Headset finds and connects to it automatically within 60 seconds of both boards powering on.
- Headset identifies the Gateway by OUI prefix (not a fixed MAC address), so any matching Gateway is found automatically.
- After connection, audio streams over the P2P link using the static IP pair 192.168.7.1 (Gateway) / 192.168.7.2 (Headset).
- No router, no credential setup, no user action required for the default experience.
- P2P is the **default mode** on first boot of a freshly flashed device.

**Priority:** P0

---

### FR-005 — STA Mode with mDNS Auto-Discovery (Optional)

**As a** developer, **I want** both devices to optionally join an existing Wi-Fi network
and discover each other automatically **so that** I can evaluate the demo in an
infrastructure environment.

**Acceptance criteria:**
- STA mode is enabled via a build overlay or a long-press mode cycle.
- Gateway advertises an audio service via DNS-SD / mDNS (hostname: `audiogateway.local`).
- Headset resolves `audiogateway.local` and initiates a UDP connection without user input within 10 seconds of both devices joining the same network.
- Opus compression overlay is valid only in STA mode.

**Priority:** P1

---

### FR-006 — Button Control

**As a** user, **I want** to control the device with physical buttons **so that**
the demo does not require a serial terminal.

**Acceptance criteria:**
- Short press on Button 0: print current Wi-Fi mode to UART.
- Long press (≥ 3 s) on Button 0: cycle Wi-Fi mode (STA → P2P_GO → P2P_CLIENT), save to NVS, reboot into new mode.
- Volume Up button (sw1 / button 1, headset): adjust playback volume up.
- Play/Pause button (sw2 / button 2): start or stop the audio stream.
- Test tone button (sw3 / button 3): trigger test tone (when enabled).
- **Volume Down is not available on nRF5340 Audio DK in this release:** button 0 (sw0) is reserved for Wi-Fi mode selection. A future update may use a double-click gesture for volume down.
- All button actions debounced and reliable across all supported boards.

**Priority:** P1

---

### FR-007 — LED Status Indication

**As a** user, **I want** LEDs to reflect the device state **so that** the demo
is self-explanatory without a serial monitor.

**Acceptance criteria:**
- LED rotates (cycling effect) while connecting.
- LED solid ON when audio link is established (P2P connected or DHCP bound in STA mode).
- LED fast-blinks on error (Wi-Fi disconnected unexpectedly).
- LED behaviour is consistent across all supported boards.

**Priority:** P1

---

### FR-008 — USB Audio I/O (Gateway, deferred boards)

**As a** developer, **I want** the Gateway to accept USB audio input **so that**
any computer can feed audio into the demo without hardware modification.

**Acceptance criteria:**
- Gateway presents as a USB audio class device (headset composite).
- Microphone capture path sends PC microphone audio over Wi-Fi.
- Headphone playback path plays received Wi-Fi audio on USB headphones.

**Priority:** P1 (nRF5340 Audio DK); deferred for nRF7002DK and nRF54LM20DK USB-audio path.

---

### FR-009 — I2S / Hardware Codec Audio I/O (nRF5340 Audio DK only)

**As a** developer, **I want** to use the onboard CS47L63 codec and 3.5 mm jack
on the nRF5340 Audio DK **so that** the demo works with standard headphones.

**Acceptance criteria:**
- I2S PCM data flows between the nRF5340 and the CS47L63 codec.
- Audio volume is adjustable via hardware codec registers.
- I2S path is automatically disabled on boards without a hardware codec.

**Priority:** P0 (nRF5340 Audio DK only)

---

### FR-010 — Multi-Board Support

**As a** developer, **I want** the same application codebase to build for all
supported hardware platforms **so that** the demo can be demonstrated on different
Nordic development boards.

**Acceptance criteria:**
- **P0 boards:** nRF5340 Audio DK + nRF7002EK — gateway and headset roles, I2S audio.
- **Deferred (build must not break):** nRF7002DK (gateway, no audio I/O parity); nRF54LM20DK + nRF7002EB2 (gateway USB-audio — build only, audio I/O validation deferred).
- Headset role is only validated on nRF5340 Audio DK in this release.
- Each board has its own DTS overlay and board-specific Kconfig.
- All supported boards build without errors from the same `src/` tree.

**Priority:** P0

---

### FR-011 — SD Card Audio Playback (nRF5340 Audio DK only)

**As a** developer, **I want** to play audio from an SD card **so that** the
demo can run without a live PC audio source.

**Acceptance criteria:**
- WAV files on a micro-SD card are read and encoded as the audio source.
- Enabled via `CONFIG_NRF5340_AUDIO_SD_CARD_MODULE=y`.
- Gracefully disabled if no SD card is present.

**Priority:** P2

---

### FR-012 — Line-In Audio Input (nRF5340 Audio DK only)

**As a** developer, **I want** to feed audio via the 3.5 mm LINE IN jack on the
Audio DK **so that** any audio source (phone, PC) can be used.

**Acceptance criteria:**
- LINE IN mode is selected via `overlay-gateway-linein.conf`.
- Captured audio is encoded and streamed as with any other source.

**Priority:** P2

---

## 4. Non-Functional Requirements

### NFR-001 — Build System

- Single CMakeLists.txt covering all boards and roles.
- Board-specific hardware code gated by Kconfig (`CONFIG_SOC_SERIES_NRF53`, `CONFIG_BOARD_*`).
- No Partition Manager; DTS-based flash partitioning only (`SB_CONFIG_PARTITION_MANAGER=n`).

### NFR-002 — Memory

- All P0 build configurations must fit within the available flash of their respective SoC.
- nRF5340 (1 MB): ≤ 85% flash utilisation.
- nRF54LM20A (1940 KB): ≤ 70% flash utilisation.
- P2P + PCM default and STA + Opus overlay are each measured on nRF5340 Audio DK before release.

### NFR-003 — NCS Compatibility

- Must build cleanly on NCS v3.3.0 with zero compiler errors.
- Kconfig experimental symbols are acceptable (NCS framework, not app-owned).

### NFR-004 — zego Brick Library

- Device UI (buttons, LEDs) and Wi-Fi connectivity layer are built on the shared `zego` brick library: `zego/bricks/button`, `zego/bricks/led`, `zego/bricks/wifi`, `zego/bricks/network`, `zego/bricks/memonitor`.
- The app consumes bricks via `EXTRA_ZEPHYR_MODULES`; brick source is not modified.
- This is the canonical architecture for Nordic Wi-Fi applications and enables consistent patterns across projects.

### NFR-005 — Codec Gating

- The Opus codec is an **opt-in overlay** (`overlay-opus.conf`).
- Opus is supported **only in STA mode**. A build combining P2P mode and Opus is explicitly **out of scope** due to the combined RAM/flash footprint of WPA supplicant P2P and libopus.
- Raw PCM is the default codec; it functions in both P2P and STA modes.

---

## 5. Out of Scope (v3.3.0)

| Not building | Note |
|---|---|
| SoftAP as a selectable user mode | P2P_GO provides zero-infrastructure AP capability; SoftAP overlay is retired from the default mode cycle |
| P2P + Opus combination in one image | Memory constraint — see NFR-005 |
| Cloud connectivity (MQTT / HTTP to remote server) | Not in scope |
| Over-the-air firmware updates | Not in scope |
| BLE provisioning for Wi-Fi credentials | Not in scope |
| iOS / Android companion app | Not in scope |
| Headset role on nRF7002DK or nRF54LM20DK | Not in scope for this release |
| Persistent P2P pairing (remember last peer), per-mode audio profiles, auto-recovery after disconnect | Not implemented in current codebase — deferred to a future sprint if needed |

---

## 6. Success Metrics

| Metric | Target | Measurement Method |
|--------|--------|--------------------|
| Build success — P0 cells | 4/4 configurations (matrix cells 1–4) | `west build -p` zero errors |
| Build success — P1 cells | 2/2 configurations (matrix cells 5–6) | `west build -p` zero errors |
| P2P audio stream latency | < 200 ms | Oscilloscope / UART timestamps |
| STA audio stream latency | < 200 ms | Oscilloscope / UART timestamps |
| P2P stream stability | ≥ 60 s no glitch | UART log, listener test |
| P2P autoconnect time | ≤ 60 s from both boards powered | UART log timestamp from boot |
| mDNS discovery time (STA) | < 10 s | UART log timestamp from boot |
| Flash utilisation | < 85% (nRF5340), < 70% (nRF54LM20A) | `west build` SIZE output |

---

## 7. Release Criteria (P0 Gate)

All of the following must pass before any tagged release:

1. All four P0 `west build -p` configurations (matrix cells 1–4) succeed with zero errors.
2. nRF5340 Audio DK gateway (P2P_GO) and headset (P2P_CLIENT) auto-connect and stream audio for ≥ 60 s without glitching.
3. nRF5340 Audio DK gateway and headset stream Opus audio in STA mode for ≥ 60 s without glitching.
4. UART log shows P2P connection established within 60 s of both boards powering on.
5. Long-press button mode cycle (STA → P2P_GO → P2P_CLIENT) persists across reboot.
6. Flash utilisation within NFR-002 limits for all P0 configurations.
