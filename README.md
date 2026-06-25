# Nordic Wi-Fi Audio

[![Validation](https://github.com/chshzh/nordic-wifi-audio/actions/workflows/validation.yml/badge.svg)](https://github.com/chshzh/nordic-wifi-audio/actions/workflows/validation.yml)
[![Latest Release](https://img.shields.io/github/v/release/chshzh/nordic-wifi-audio?label=Latest%20Release&color=skyblue)](https://github.com/chshzh/nordic-wifi-audio/releases/latest)


## Project Overview

### Introduction

`nordic-wifi-audio` is a real-time Wi-Fi audio streaming demo for nRF7x development kits. It targets embedded developers and audio engineers who need a low-latency wireless audio link — no Bluetooth stack, no router required in the default P2P mode. The firmware ships in two roles: an **Audio Gateway** that captures audio from USB or LINE IN and transmits over UDP, and an **Audio Headset** that receives the stream and drives the codec output.

### Supported hardware

| Board | Build target | Notes |
|-------|--------------|-------|
| nRF5340 Audio DK + nRF7002EK | `nrf5340_audio_dk/nrf5340/cpuapp` + `-DSHIELD=nrf7002ek` | Gateway + Headset (P0) |
| nRF7002DK | `nrf7002dk/nrf5340/cpuapp` | Gateway only (P1) |
| nRF54LM20DK + nRF7002EB2 | `nrf54lm20dk/nrf54lm20a/cpuapp` + `-DSHIELD=nrf7002eb2` | Gateway only (P1) |

### Features

- **Wi-Fi P2P audio link (default)** — device boots into Wi-Fi Direct P2P_GO (Gateway) / P2P_GC (Headset) mode; no router or credentials needed on first boot.
- **STA mode support** — join an existing Wi-Fi network for integration into home or studio setups.
- **Dual device roles** — Gateway (audio source, UDP server) and Headset (audio sink, UDP client); role selected at build time.
- **Raw PCM streaming (default)** — low-latency uncompressed 16-bit PCM over UDP with no codec overhead in the default build.
- **Opus codec option** — add `overlay-opus.conf` for compressed streaming with configurable bitrate (STA mode only; mutually exclusive with P2P on nRF5340).
- **USB audio source (default)** — Gateway appears as a USB sound card on the host PC; set it as the output device and any audio plays wirelessly.
- **Analog audio source** — Gateway can capture analog audio via `overlay-gateway-linein.conf` on nRF5340 Audio DK.
- **Runtime mode switching** — Button 0 long press (≥ 3 s) cycles Wi-Fi mode and reboots; mode persists in NVS flash.
- **Visual status feedback** — LED rotates while connecting, solid ON during active audio link, fast blink on error.
- **Startup banner** — prints firmware version, board, role, Wi-Fi mode, and connection instructions at every boot.

### Target Users

- **Evaluator** — grab a pre-built `.hex` from the [Releases](https://github.com/chshzh/nordic-wifi-audio/releases/latest) page, flash it, and follow the [Evaluator Quick Start](#evaluator-quick-start) guide.
- **Developer** — clone the workspace, build from source, and customise the firmware; see [Developer Guide](#developer-guide) for build setup and [Documentation](#documentation) for product requirements, architecture, and per-module specs.

---

## Evaluator Quick Start

### Step 1 — Flash the firmware

Download the pre-built `.hex` for your board and role from the [Releases](https://github.com/chshzh/nordic-wifi-audio/releases/latest) page:

| Board | Role | File |
|-------|------|------|
| nRF5340 Audio DK + nRF7002EK | Gateway | `nordic-wifi-audio-gateway-nrf5340-audio-dk-<version>.hex` |
| nRF5340 Audio DK + nRF7002EK | Headset | `nordic-wifi-audio-headset-nrf5340-audio-dk-<version>.hex` |
| nRF7002DK | Gateway | `nordic-wifi-audio-gateway-nrf7002dk-<version>.hex` |
| nRF54LM20DK + nRF7002EB2 | Gateway | `nordic-wifi-audio-gateway-nrf54lm20dk-<version>.hex` |

Flash using **nRF Connect for Desktop → Programmer** (Erase & Write), or via CLI:

```sh
nrfutil device program --firmware nordic-wifi-audio-<role>-<board>-<version>.hex --verify
```

> **Two-device setup:** Flash one nRF5340 Audio DK as Gateway and a second as Headset. Both boot into P2P mode — Gateway starts as P2P_GO and Headset as P2P_GC. They pair automatically using the static P2P link (`192.168.7.1` Gateway / `192.168.7.2` Headset).

### Step 2 — Verify

**1. UART log** — open a serial terminal at 115200 baud:

| Board | Port | Baud |
|-------|------|------|
| nRF5340 Audio DK + nRF7002EK | VCOM0 (`/dev/tty.usbmodem*1`) | 115200 |
| nRF7002DK | VCOM1 (`/dev/tty.usbmodem*3`) | 115200 |
| nRF54LM20DK + nRF7002EB2 | VCOM0 (`/dev/tty.usbmodem*1`) | 115200 |

At boot you will see a startup banner:

```
*** nordic-wifi-audio-gateway v3.3.0.1 | NCS v3.3.0 ***
Board: nrf5340_audio_dk  MAC: AA:BB:CC:DD:EE:FF
Mode: P2P_GO  |  Modules: [wifi] [network] [ux] [button] [led]
P2P_GO: DIRECT-xx started — Headset will auto-connect via static P2P link
```

LED 0 begins rotating. Mode-specific connection:

- **P2P mode** (default on fresh flash): Gateway boots as P2P_GO; Headset boots as P2P_GC. They pair automatically — wait for both to log `Audio stream READY` and LED 0 to go solid ON.
- **STA mode**: run `wifi cred add -s <SSID> -p <pass> -k 1` then `wifi cred auto_connect` on both devices; both join the same AP; wait for `Audio stream READY`.

**2. Buttons & LEDs**

### Buttons

| Board | Button | Gesture | Action |
|-------|--------|---------|--------|
| nRF5340 Audio DK + nRF7002EK | VOL- (idx 0) | Single click | Volume Down |
| | VOL+ (idx 1) | Single click | Volume Up |
| | PLAY/PAUSE (idx 2) | Single click | Play / Pause audio stream |
| | BTN4 (idx 3) | Single click | Trigger test tone |
| | BTN5 (idx 4) | Single click | Print current Wi-Fi state to UART |
| | | Long press ≥ 3 s | Cycle mode (STA → P2P_GO → P2P_GC); save to NVS; reboot |
| nRF7002DK | Button 1 / SW0 (idx 0) | Single click | Print current Wi-Fi state to UART |
| | | Long press ≥ 3 s | Cycle mode (STA → P2P_GO → P2P_GC); save to NVS; reboot |
| | Button 2 / SW1 (idx 1) | Any | Available (no default audio function — gateway only) |
| nRF54LM20DK + nRF7002EB2 | BUTTON0 (idx 0) | Single click | Print current Wi-Fi state to UART |
| | | Long press ≥ 3 s | Cycle mode (STA → P2P_GO → P2P_GC); save to NVS; reboot |
| | BUTTON1–2 (idx 1–2) | Any | Available (no default audio function — gateway only) |

> **Note:** BTN5 (idx 4) is reserved for Wi-Fi mode control on nRF5340 Audio DK; VOL- (idx 0) is available for Volume Down.

### LEDs

LED 0 reflects the Wi-Fi / audio link state. All other LEDs are available for application use.

| Board | LED 0 (idx 0) | Other LEDs |
|-------|---------------|------------|
| nRF5340 Audio DK + nRF7002EK | RGB1 R/G/B (idx 0–2) — ROTATE for Wi-Fi/audio state | RGB2 (idx 3–5) — role indicator (see below); LED1–3 (idx 6–8) — free |
| nRF7002DK | LED1 — Wi-Fi / audio state | LED2 (idx 1) — free |
| nRF54LM20DK + nRF7002EB2 | LED0 — Wi-Fi / audio state | LED1–3 (idx 1–3) — free |

nRF5340 Audio DK + nRF7002EK — RGB1 state effects:

| State | Effect |
|-------|--------|
| Boot / connecting | RGB1 ROTATE (all three channels) |
| Audio link active | RGB1 Green — Solid ON |
| Error / disconnected | RGB1 Red — Fast BLINK (100 ms half-period) |

nRF5340 Audio DK + nRF7002EK — RGB2 role indicator (solid, set at boot):

| Role | RGB2 colour |
|------|-------------|
| Gateway | Blue |
| Headset | Green |

nRF7002DK and nRF54LM20DK + nRF7002EB2 — LED 0 state effects:

| State | Effect |
|-------|--------|
| Boot / connecting | ROTATE |
| Audio link active | Solid ON |
| Error / disconnected | Fast BLINK (100 ms half-period) |

**3. Application logic**

Once both devices have an audio link, set the **nRF5340 Audio DK Gateway** as the audio output device on your PC. Press PLAY/PAUSE on the Headset to start streaming — audio plays through the headphones connected to the Headset's 3.5 mm output.

---

## Developer Guide

### Project Structure

```text
nordic-wifi-audio/
├── CMakeLists.txt                ← registers zego bricks; sets role-specific ZEGO_BANNER_APP_NAME
├── Kconfig / Kconfig.defaults    ← project Kconfig
├── Kconfig.sysbuild
├── prj.conf                      ← base Kconfig configuration
├── sysbuild.conf                 ← SB_CONFIG_PARTITION_MANAGER=n; SB_CONFIG_WIFI_NRF70=y
├── overlay-audio-gateway.conf    ← gateway role (CONFIG_AUDIO_GATEWAY=y)
├── overlay-audio-headset.conf    ← headset role (CONFIG_AUDIO_HEADSET=y)
├── overlay-opus.conf             ← Opus codec (STA mode only; mutually exclusive with P2P)
├── overlay-gateway-linein.conf   ← LINE IN audio input on nRF5340 Audio DK
├── overlay-sta.conf              ← STA mode with credential placeholders
├── overlay-wifi-cred-static.conf ← static Wi-Fi credentials
├── boards/
│   ├── nrf5340_audio_dk_nrf5340_cpuapp.conf    ← Audio DK Kconfig (LOG_BUFFER_SIZE=4096)
│   ├── nrf5340_audio_dk_nrf5340_cpuapp.overlay ← DTS: nRF7002EK SPI pinout; partition layout
│   ├── nrf7002dk_nrf5340_cpuapp.conf
│   ├── nrf7002dk_nrf5340_cpuapp.overlay         ← DTS: button aliases; USB audio node; partitions
│   ├── nrf54lm20dk_nrf54lm20a_cpuapp.conf
│   └── nrf54lm20dk_nrf54lm20a_cpuapp.overlay    ← DTS: button aliases; USB audio node; RRAM partitions
├── docs/
│   ├── pm-prd/PRD.md             ← Product Requirements Document
│   └── dev-specs/
│       ├── 0-overview.md         ← Start here — spec index, PRD-to-spec mapping
│       ├── 1-architecture.md     ← Module map, Zbus channels, threads, boot sequence
│       ├── 2-dts-partition.md    ← Flash partition layout per board
│       ├── 3-memopt.md           ← Memory budget and optimization report
│       ├── audio-pipeline.md     ← Audio encode/decode datapath spec
│       ├── network-module.md     ← zego-network weak-hook consumption
│       ├── ui-module.md          ← Button gestures, LED state machine
│       ├── board-init-module.md  ← Multi-board hardware abstraction
│       └── diagnostics-module.md ← memonitor brick + status shell
├── src/
│   ├── audio/                    ← audio_system, audio_datapath, sw_codec, wifi_audio_rx
│   │   └── opus_interface/       ← Opus encoder/decoder wrapper (moved from lib/)
│   ├── modules/
│   │   ├── network/net_event_app.c ← Wi-Fi event hooks → audio start/stop
│   │   ├── ux/ux.c               ← button gestures + LED state machine
│   │   ├── audio_i2s.c/h         ← I2S PCM driver (nRF53 only)
│   │   ├── audio_usb.c/h         ← USB audio class
│   │   ├── hw_codec.c/h          ← CS47L63 codec (nRF5340 Audio DK)
│   │   └── sd_card*.c/h          ← SD card playback
│   ├── utils/                    ← board init, UICR, board_version, channel_assignment
│   └── drivers/cs47l63_comm.c    ← CS47L63 SPI driver
├── wifi_audio_gateway/main.c     ← gateway entry point
└── wifi_audio_headset/main.c     ← headset entry point
```

External zego bricks (referenced via `EXTRA_ZEPHYR_MODULES` in `CMakeLists.txt`):

```text
../zego/bricks/wifi/      ← Wi-Fi mode selector, NVS persistence, ZEGO_BANNER_APP_NAME
../zego/bricks/network/   ← Wi-Fi event dispatcher, DHCP handling, zego_on_net_event_* callbacks
../zego/bricks/button/    ← GPIO debounce, BUTTON_CHAN publish
../zego/bricks/led/       ← LED_CMD_CHAN subscriber, ROTATE/BLINK/BREATHE effects
../zego/bricks/memonitor/ ← heap and thread-stack HWM sampling, MEMONITOR_CHAN
```

West-managed third-party modules (populated by `west update`):

```text
../modules/lib/opus/      ← Opus codec source v1.5.2 (west-managed, replaces former lib/opus submodule)
```

### Workspace Setup

West workspace is driven by [west.yml](west.yml), which pins the NCS version this application is based on:

```sh
- name: sdk-nrf
  path: nrf
  revision: v3.3.0
  import: true
  remote: ncs
```

Release versions follow the NCS version with a build counter suffix: `v<ncs-version>.<build>` (e.g. `v3.3.0.1`, `v3.3.0.2`). The major/minor/patch components always match the NCS version the firmware is based on.

Use nRF Connect for VS Code or a shell initialized with the NCS toolchain.

#### Method 1 (Preferred) — Add to an existing NCS installation

If you already have NCS v3.3.0 installed, reuse it directly — no re-downloading required.

```sh
cd /opt/nordic/ncs/v3.3.0   # your existing NCS workspace root

git clone https://github.com/chshzh/nordic-wifi-audio

# Switch the workspace manifest to nordic-wifi-audio (one-time change)
west config manifest.path nordic-wifi-audio

# Sync — NCS repos already present, only new project repos are cloned
west update
```

#### Method 2 — Fresh installation as a Workspace Application

##### Option A: nRF Connect for VS Code

Follow the [custom repository guide](https://docs.nordicsemi.com/bundle/nrf-connect-vscode/page/guides/extension_custom_repo.html).

##### Option B: CLI

```sh
west init -m https://github.com/chshzh/nordic-wifi-audio --mr main <workspace-dir>
cd <workspace-dir>
west update
```

See the Nordic guide on [Workspace Application Setup](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/dev_model_and_contributions/adding_code.html#workflow_4_workspace_application_repository_recommended) for details.

### Build

> **One firmware, both modes.** The default build (with the `wifi-p2p` snippet) is a **dual-mode** image that supports **both** Wi-Fi Direct P2P **and** infrastructure STA with mDNS auto-discovery in a single binary. The active mode is stored in NVS; switch it at runtime with the `zego_wifi_mode` shell command or a Button 0 long-press. A smaller STA-only image (no snippet) is available when P2P is not needed.

```sh
# Go to app root first
cd nordic-wifi-audio
```

#### Default (dual-mode P2P + STA) — recommended

Built **with** `-Dnordic-wifi-audio_SNIPPET=wifi-p2p`. On fresh flash the Gateway boots as **P2P_GO** and the Headset as **P2P_GC** (Group Client); they pair automatically at `192.168.7.1` / `192.168.7.2` — no router needed. Switch either device to **STA** at runtime (`zego_wifi_mode sta`): both join your AP, the Gateway advertises the audio service over mDNS, and the Headset auto-discovers it via DNS-SD (no hardcoded IP).

```sh
# Gateway — nRF5340 Audio DK + nRF7002EK
west build -p -b nrf5340_audio_dk/nrf5340/cpuapp -d build_gateway_nrf5340audiodk -- \
  -DSHIELD=nrf7002ek -DEXTRA_CONF_FILE="overlay-audio-gateway.conf" \
  -Dnordic-wifi-audio_SNIPPET=wifi-p2p

# Gateway — nRF7002DK
west build -p -b nrf7002dk/nrf5340/cpuapp -d build_gateway_nrf7002dk -- \
  -DEXTRA_CONF_FILE="overlay-audio-gateway.conf" \
  -Dnordic-wifi-audio_SNIPPET=wifi-p2p

# Gateway — nRF54LM20DK + nRF7002EB2
west build -p -b nrf54lm20dk/nrf54lm20a/cpuapp -d build_gateway_nrf54lm20dk -- \
  -DSHIELD=nrf7002eb2 -DEXTRA_CONF_FILE="overlay-audio-gateway.conf" \
  -Dnordic-wifi-audio_SNIPPET=wifi-p2p

# Headset — nRF5340 Audio DK + nRF7002EK only
west build -p -b nrf5340_audio_dk/nrf5340/cpuapp -d build_headset_nrf5340audiodk -- \
  -DSHIELD=nrf7002ek -DEXTRA_CONF_FILE="overlay-audio-headset.conf" \
  -Dnordic-wifi-audio_SNIPPET=wifi-p2p
```

> The image-scoped `-Dnordic-wifi-audio_SNIPPET=wifi-p2p` applies the NCS `wifi-p2p` snippet only to the app image (not to `hci_ipc`), adding `CONFIG_NRF70_P2P_MODE=y`, `CONFIG_NRF70_AP_MODE=y`, `CONFIG_WIFI_NM_WPA_SUPPLICANT_P2P=y`. mDNS (responder + DNS-SD) comes from `prj.conf` and stays enabled. The dual-mode image fits in ~99 % of the nRF5340's 1 MB flash thanks to picolibc and the removal of the LC3/CMSIS-DSP test-tone path.

Per-role mode visibility (set in the role overlays): the Gateway exposes `sta` + `p2p_go`; the Headset exposes `sta` + `p2p_gc`. The mode-switch banner hint is printed only when more than one mode is compiled in.

#### STA-only (no P2P) — smaller image

Omit the snippet. Only STA mode is compiled in (~63 % flash). Both devices join an AP; mDNS auto-discovery works exactly as in the dual-mode STA path. Store credentials first: `wifi cred add -s <SSID> -p <pass> -k 1`.

```sh
# STA-only Gateway — nRF5340 Audio DK + nRF7002EK
west build -p -b nrf5340_audio_dk/nrf5340/cpuapp -d build_gateway_sta_nrf5340audiodk -- \
  -DSHIELD=nrf7002ek -DEXTRA_CONF_FILE="overlay-audio-gateway.conf"

# STA-only Headset — nRF5340 Audio DK + nRF7002EK only
west build -p -b nrf5340_audio_dk/nrf5340/cpuapp -d build_headset_sta_nrf5340audiodk -- \
  -DSHIELD=nrf7002ek -DEXTRA_CONF_FILE="overlay-audio-headset.conf"
```

#### Feature Overlay Builds

| Overlay / Option | Purpose |
|---------|---------|
| `-Dnordic-wifi-audio_SNIPPET=wifi-p2p` | Add P2P Wi-Fi Direct to the build (dual-mode). Omit for STA-only. |
| `overlay-opus.conf` | Opus codec (STA mode only; mutually exclusive with P2P on nRF5340) |
| `overlay-gateway-linein.conf` | LINE IN audio input on nRF5340 Audio DK |

> **Opus and P2P are mutually exclusive** on nRF5340 Audio DK — Opus is an STA-only (no-snippet) option due to flash limitation. Use one or the other, not both.

**Example — STA Gateway with Opus codec + LINE IN (no P2P snippet):**

```sh
west build -p -b nrf5340_audio_dk/nrf5340/cpuapp -d build_gateway_opus_linein -- \
  -DSHIELD=nrf7002ek \
  -DEXTRA_CONF_FILE="overlay-audio-gateway.conf;overlay-opus.conf;overlay-gateway-linein.conf"
```

### Flash

First-time flash (erases NVS — Wi-Fi credentials and mode must be re-entered after):

```sh
west flash -d build_gateway_nrf5340audiodk --erase   # nRF5340 Audio DK gateway
west flash -d build_gateway_nrf7002dk --erase        # nRF7002DK gateway
west flash -d build_gateway_nrf54lm20dk --recover    # nRF54LM20DK gateway
west flash -d build_headset_nrf5340audiodk --erase   # nRF5340 Audio DK headset
```

Subsequent flashes (preserves NVS — mode and credentials survive):

```sh
west flash -d build_gateway_nrf5340audiodk
west flash -d build_gateway_nrf7002dk
west flash -d build_gateway_nrf54lm20dk
west flash -d build_headset_nrf5340audiodk
```

### Developer Notes

- **One dual-mode firmware (default, with the `wifi-p2p` snippet).** A single image supports both P2P and STA-with-mDNS. It fits in ~99 % of the nRF5340's 1 MB flash because the C library is **picolibc** (−~15 KB flash / −~14 KB RAM vs newlib) and the LC3/CMSIS-DSP test-tone path was replaced with a local square-wave generator. An STA-only image (no snippet) is ~63 % flash.
- **Runtime mode switch:** `zego_wifi_mode [sta|p2p_go|p2p_gc]` (or Button 0 long-press) saves the mode to NVS and reboots. Per-role visibility: Gateway exposes `sta`/`p2p_go`, Headset exposes `sta`/`p2p_gc` (via `CONFIG_ZEGO_WIFI_MODE_*_ENABLED`).
- **STA mode auto-connection:** Store credentials with `wifi cred add -s <SSID> -p <pass> -k 1`, then `zego_wifi_mode sta`. The Gateway auto-connects and registers an mDNS DNS-SD service (`_nrfwifiaudio._udp.local`); the Headset (with `MDNS_RESOLVER` + `DNS_SERVER_IP_ADDRESSES`) discovers it via PTR→SRV→A and connects — no manual IP entry.
- **P2P mode auto-connection:** Gateway boots as P2P_GO; Headset (P2P_GC) auto-connects using the GO MAC in `CONFIG_ZEGO_WIFI_P2P_GC_TARGET_GO_MAC`. The GO runs a DHCP server (needs `CONFIG_NET_MAX_CONN`/`NET_MAX_CONTEXTS` ≥ 8 so its port-67 socket can bind alongside mDNS). Static IP pair: 192.168.7.1 (Gateway) / 192.168.7.2 (Headset).
- **Headset role is nRF5340 Audio DK only.** The nRF7002DK and nRF54LM20DK boards support the Gateway role only.
- **Opus and P2P are mutually exclusive on nRF5340 Audio DK.** `overlay-opus.conf` is an STA-only option; never combine with the `wifi-p2p` snippet.
- **Two separate entry points:** `wifi_audio_gateway/main.c` (Gateway) and `wifi_audio_headset/main.c` (Headset). Role is selected at build time via `CONFIG_AUDIO_GATEWAY` / `CONFIG_AUDIO_HEADSET` in the role overlay.
- **`CONFIG_LOG_BUFFER_SIZE=8192`** is set in `boards/nrf5340_audio_dk_nrf5340_cpuapp.conf`. The default 1 KB ring buffer is too small for the boot banner.
- **NVS erase:** `--erase` wipes NVS — the device boots in its default mode on next power-up (Gateway → P2P_GO, Headset → P2P_GC). Run `zego_wifi_mode sta` to switch to STA.
- **Module specs** for non-obvious behavior: [audio-pipeline.md](docs/dev-specs/audio-pipeline.md), [network-module.md](docs/dev-specs/network-module.md), [ui-module.md](docs/dev-specs/ui-module.md).

---

## Documentation

The full design documentation lives under `docs/`. Start with [docs/dev-specs/0-overview.md](docs/dev-specs/0-overview.md), which maps every PRD requirement to the spec file that implements it and provides an architecture summary.

| Document | Description |
|---|---|
| [docs/pm-prd/PRD.md](docs/pm-prd/PRD.md) | Product Requirements — user-perspective features, behavior, acceptance criteria, changelog |
| [docs/dev-specs/0-overview.md](docs/dev-specs/0-overview.md) | **Start here** — spec index, PRD-to-spec mapping, architecture summary, design decisions |
| [docs/dev-specs/1-architecture.md](docs/dev-specs/1-architecture.md) | Module map, Zbus channels, SYS_INIT boot sequence, thread budget |
| [docs/dev-specs/2-dts-partition.md](docs/dev-specs/2-dts-partition.md) | Flash partition layout per board |
| [docs/dev-specs/3-memopt.md](docs/dev-specs/3-memopt.md) | Memory budget and optimization report |
| [docs/dev-specs/audio-pipeline.md](docs/dev-specs/audio-pipeline.md) | Audio encode/decode datapath spec |
| [docs/dev-specs/network-module.md](docs/dev-specs/network-module.md) | Wi-Fi event hooks — zego-network consumption |
| [docs/dev-specs/ui-module.md](docs/dev-specs/ui-module.md) | Button gestures and LED state machine |
| [docs/dev-specs/board-init-module.md](docs/dev-specs/board-init-module.md) | Multi-board hardware abstraction |
| [zego/bricks/wifi ↗](https://github.com/chshzh/zego) | Wi-Fi mode selector, NVS persistence, `ZEGO_BANNER_APP_NAME` |
| [zego/bricks/network ↗](https://github.com/chshzh/zego) | Wi-Fi event dispatcher, DHCP handling, weak-hook callbacks |
| [zego/bricks/button ↗](https://github.com/chshzh/zego) | GPIO debounce, BUTTON_CHAN publish |
| [zego/bricks/led ↗](https://github.com/chshzh/zego) | LED_CMD_CHAN subscriber, ROTATE/BLINK/BREATHE effects |

---

## Methodology

This project was developed using the [chsh-sk-ncs-0-workflow skill](https://github.com/chshzh/claude/blob/main/skills/chsh-sk-ncs-0-workflow/SKILL.md) — a four-phase lifecycle for NCS/Zephyr IoT projects where each phase has a dedicated AI skill:

| Phase | Focus | Skill | Output |
|-------|-------|-------|--------|
| 1 — Product Definition | What the device should do, for whom, and why | `chsh-sk-ncs-1-prd` | `docs/pm-prd/PRD.md` |
| 2 — Technical Design | Translate PRD into engineering specs | `chsh-sk-ncs-2-spec` | `docs/dev-specs/*.md` |
| 3 — Implementation | Implement, debug, and optimise code from approved specs | `chsh-sk-ncs-3.1-coding` · `chsh-sk-ncs-3.2-debug` · `chsh-sk-ncs-3.3-memopt` | `src/`, passing build |
| 4 — V&V | Verify code quality (no HW), then validate on hardware against PRD criteria | `chsh-sk-ncs-4.1-verification` · `chsh-sk-ncs-4.2-validation` | `docs/qa-test/VERIFICATION-*.md` + `docs/qa-test/VALIDATION_REPORT.md` |

Each phase feeds the next: requirements drive specs, specs drive code, code drives tests. Issues loop back to the right phase — code bugs to Phase 3, spec gaps to Phase 2, new requirements to Phase 1.

---

## License

[SPDX-License-Identifier: LicenseRef-Nordic-5-Clause](LICENSE)
