# Product Requirements Document — Nordic Wi-Fi Opus Audio Demo

## Document Information

| Field            | Value                                       |
|------------------|---------------------------------------------|
| Product Name     | Nordic Wi-Fi Opus Audio Demo                |
| NCS Version      | v3.3.0                                      |
| Author           | Nordic Semiconductor ASA                    |
| Latest Version   | 2026-05-27-23-14                            |

## Changelog

| Version          | Summary of changes                                               |
|------------------|------------------------------------------------------------------|
| 2026-05-27-23-14 | Initial PRD derived from existing code (Mode A reverse scan)    |

---

## 1. Problem Statement

Developers and product teams evaluating the nRF70 Wi-Fi chip family need a
concrete, runnable audio demo that proves low-latency audio transport over Wi-Fi
using Nordic hardware and the nRF Connect SDK.

Without this demo, teams must build all audio plumbing from scratch — codec
selection, UDP framing, SoftAP pairing, mDNS service discovery — before they can
answer the question: "Is Wi-Fi good enough for real-time audio on nRF70?"

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

### FR-002 — Opus Codec Compression

**As a** developer, **I want** audio to be compressed using the Opus codec **so
that** the bitrate fits within Wi-Fi network bandwidth with headroom for retries.

**Acceptance criteria:**
- Opus encoding runs at configurable bitrates (6 kbps – 320 kbps).
- Decoded audio plays back without perceptible artifacts at 48 kHz / 16-bit stereo.
- Codec can be swapped to LC3 or raw PCM at build time via a Kconfig overlay.

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

### FR-004 — SoftAP Mode (Gateway-Led Pairing)

**As a** developer, **I want** the Gateway to optionally create its own Wi-Fi
access point **so that** the demo works in environments with no pre-existing
infrastructure.

**Acceptance criteria:**
- Gateway starts a WPA2 access point (SSID: `GatewayAP`) when `overlay-gateway-softap.conf` is included.
- Headset connects to this AP using stored credentials.
- SoftAP band (2.4 GHz / 5 GHz) and channel are configurable at build time.

**Priority:** P1

---

### FR-005 — Headset Auto-Discovery via mDNS

**As a** developer, **I want** the Headset to automatically discover the Gateway
on the local network **so that** no manual IP address configuration is needed.

**Acceptance criteria:**
- Gateway advertises an audio service via DNS-SD / mDNS (hostname: `audiogateway.local`).
- Headset resolves `audiogateway.local` and initiates a UDP connection without user input.
- Discovery completes within 10 seconds of both devices being connected to the same network.

**Priority:** P1

---

### FR-006 — Button Control

**As a** user, **I want** to control playback with physical buttons **so that**
the demo does not require a serial terminal.

**Acceptance criteria:**
- Volume Up / Volume Down buttons adjust playback volume in steps.
- Play/Pause button starts or stops the audio stream.
- Button 4 (sw3) performs a role-specific secondary action (mute, channel toggle).
- All button actions debounced and reliable across all supported boards.

**Priority:** P1

---

### FR-007 — LED Status Indication

**As a** user, **I want** LEDs to reflect the device state **so that** the demo
is self-explanatory without a serial monitor.

**Acceptance criteria:**
- RGB LED (APP) indicates: initializing (blinking), streaming (solid green), error (red).
- Green LED (APP_3) blinks during initialization/pairing.
- LED behaviour is consistent across all supported boards.

**Priority:** P1

---

### FR-008 — USB Audio I/O

**As a** developer, **I want** the Gateway to accept USB audio input **so that**
any computer can feed audio into the demo without hardware modification.

**Acceptance criteria:**
- Gateway presents as a USB audio class device (headset composite).
- Microphone capture path sends PC microphone audio over Wi-Fi.
- Headphone playback path plays received Wi-Fi audio on USB headphones.
- Functional on nRF5340 Audio DK, nRF7002DK, and nRF54LM20DK.

**Priority:** P1

---

### FR-009 — I2S / Hardware Codec Audio I/O (nRF5340 Audio DK only)

**As a** developer, **I want** to use the onboard CS47L63 codec and 3.5 mm jack
on the nRF5340 Audio DK **so that** the demo works with standard headphones.

**Acceptance criteria:**
- I2S PCM data flows between the nRF5340 and the CS47L63 codec.
- Audio volume is adjustable via hardware codec registers.
- I2S path is automatically disabled on boards without a hardware codec.

**Priority:** P1

---

### FR-010 — Multi-Board Support

**As a** developer, **I want** the same application codebase to build for three
different hardware platforms **so that** the demo can be demonstrated on the
latest Nordic development boards.

**Acceptance criteria:**
- nRF5340 Audio DK + nRF7002EK: gateway and headset roles.
- nRF7002DK (nRF5340 + onboard nRF7002): gateway role.
- nRF54LM20DK + nRF7002EB2 shield: gateway role.
- Each board has its own DTS overlay for flash partitions, button aliases, and USB node.
- All three boards build without errors from the same `src/` tree.

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

- All four build configurations must fit within the available flash of their respective SoC.
- nRF5340 (1 MB): ≤ 85% flash utilisation.
- nRF54LM20A (1940 KB): ≤ 70% flash utilisation.

### NFR-003 — NCS Compatibility

- Must build cleanly on NCS v3.3.0 with zero compiler errors.
- Kconfig experimental symbols are acceptable (NCS framework, not app-owned).

---

## 5. Out of Scope (v3.3.0)

- Cloud connectivity (MQTT / HTTP to remote server)
- Over-the-air firmware updates (Memfault OTA or MCUboot DFU)
- BLE provisioning for Wi-Fi credentials
- iOS / Android companion app
- Headset role for nRF7002DK or nRF54LM20DK

---

## 6. Success Metrics

| Metric | Target | Measurement Method |
|--------|--------|--------------------|
| Build success | 4/4 configurations | `west build -p` zero errors |
| Audio stream latency | < 200 ms | Oscilloscope / UART timestamps |
| Stream stability | ≥ 60 s no glitch | UART log, listener test |
| Discovery time | < 10 s | UART log timestamp from boot |
| Flash utilisation | < 85% (nRF5340), < 70% (nRF54LM20A) | `west build` SIZE output |

---

## 7. Release Criteria (P0 Gate)

All of the following must pass before any tagged release:

1. All four `west build -p` configurations succeed with zero errors.
2. nRF5340 Audio DK gateway streams audio to nRF5340 Audio DK headset without glitching for 60 s.
3. USB audio input on gateway produces audible output on headset.
4. UART log shows `audiogateway.local` mDNS resolution on headset within 10 s.
