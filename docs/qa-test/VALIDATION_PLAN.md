# Validation Plan — Nordic Wi-Fi Audio

## Document Information

| Field | Value |
|-------|-------|
| Project | Nordic Wi-Fi Audio Demo |
| Version | 2026-06-26-10-32 |
| PRD Version | 2026-06-26-09-55 |
| Specs Version | 2026-06-26-10-00 |
| NCS Version | v3.3.0 |
| Boards under test | nRF5340 Audio DK ×2 (Gateway + Headset) |
| ZView memory pass | No |
| Run type | Routine functional (UAC2 migration smoke) |
| Status | Approved |

---

## Changelog

| Version | Summary of changes |
|---|---|
| 2026-06-26-10-32 | Initial plan — functional smoke for the UAC1→UAC2 USB audio migration on nRF5340 Audio DK (Gateway USB source + Headset over P2P). |

---

## Boards Under Test

> From `nrfutil device list`. Both boards are nRF5340 Audio DK (PCA10121); VCOM0 carries app logs.

| Board | Serial (`--dev-id`) | VCOM port | rtscts | J-Link target | ELF path (sysbuild app sub-image) | Shell |
|-------|---------------------|-----------|--------|---------------|-----------------------------------|-------|
| nRF5340 Audio DK — **Gateway** | 1050136274 | `/dev/tty.usbmodem0010501362741` | False | `nRF5340_xxAA` | `build_gw_audiodk/nordic-wifi-audio/zephyr/zephyr.elf` | ✅ on |
| nRF5340 Audio DK — **Headset** | 1050111981 | `/dev/tty.usbmodem0010501119811` | False | `nRF5340_xxAA` | `build_hs_audiodk/nordic-wifi-audio/zephyr/zephyr.elf` | ✅ on |

> Gateway's USB-device port is cabled to the host Mac (it appears in CoreAudio). Headset
> drives the CS47L63 codec output; its USB-audio port is unused.

---

## Coverage Matrix

> Scope is the UAC2 migration. Only requirements the migration touches or depends on.

| Requirement | Acceptance criterion (short) | Type | Round | Priority |
|-------------|------------------------------|------|-------|----------|
| FR-008 | Gateway presents as a UAC2 device, class-native on the host | Common | R1 | P0 |
| FR-008 | Host CoreAudio device name changes UAC1 "nRF5340 USB Audio" → UAC2 "Nordic Wi-Fi Audio" | Common | R1 | P0 |
| FR-008 | USB OUT audio from host reaches `fifo_rx` (`USB RX first data received.`) | Common | R1 | P0 |
| FR-004 | P2P: Gateway boots P2P_GO, Headset P2P_GC auto-connects ≤ 60 s at 192.168.7.1/7.2 | Common | R1 | P0 |
| FR-001 | Audio frames stream Gateway → Headset (streaming state active, no fatal RX errors) | Common | R1 | P0 |

---

## Test Rounds

### Round R1 — UAC2 source + P2P stream · boards: both Audio DKs · [ZView: No]

- **Goal / requirements covered**: FR-008, FR-004, FR-001
- **Setup**:
  - Flash `build_gw_audiodk` → SN 1050136274 (Gateway); `build_hs_audiodk` → SN 1050111981 (Headset). Standard flash (preserve NVS).
  - Gateway USB-device port cabled to host Mac.
- **Steps** (shell-first; UART log capture via `chsh-ag-terminal`):
  1. Capture both boot banners (confirm role + PRD/Specs version `…09-55` / `…10-00`).
  2. Gateway log shows UAC2 ready: `Ready for USB host to send/receive.`
  3. Host check: `system_profiler SPAudioDataType` shows **"Nordic Wi-Fi Audio"** (was "nRF5340 USB Audio").
  4. P2P auto-connect: Gateway `P2P_GO`, Headset `P2P_GC`, IPs 192.168.7.1 / 192.168.7.2 within 60 s.
  5. Set the host's default output to "Nordic Wi-Fi Audio" and play audio; Gateway logs `USB RX first data received.` and streaming state becomes active.
  6. `wifi status` on both (P2P link up); confirm no repeated `ERR_CHK` / overrun faults.
- **Expected**:
  - UAC2 enumerates with the new product string (UAC2 proof).
  - P2P GO/GC connect; audio streams; no fatal errors over ≥ 30 s.
- **Stability**: single pass (functional smoke). Loop test deferred unless a flake appears.

---

## Approval

Plan reviewed and approved to execute: **Yes** — user, 2026-06-26 (functional smoke, Gateway=SN 1050136274).
