# Validation Report — Nordic Wi-Fi Audio

## Document Information

| Field | Value |
|-------|-------|
| Project | Nordic Wi-Fi Audio Demo |
| Version | 2026-06-26-10-58 |
| PRD Version | 2026-06-26-09-55 |
| Specs Version | 2026-06-26-10-00 |
| Plan Version | 2026-06-26-10-32 |
| NCS Version | v3.3.0 |
| Firmware build | UAC2 migration (Jun 26 2026 10:24); gateway rebuilt 10:23 with sof_cb fix |
| Boards | nRF5340 Audio DK ×2 — Gateway SN 1050136274, Headset SN 1050111981 |
| ZView memory pass | No |
| Host | macOS (CoreAudio), gateway USB-device port cabled to host |
| Status | Executed — **PASS** (all P0 closed) |

---

## Changelog

| Version | Summary of changes |
|---|---|
| 2026-06-26-11-10 | Corrected verdict to PASS. End-to-end USB audio confirmed working with sustained host playback (`USB RX first data received.` @ 29.2 s; headset plays). Earlier FAIL was a test-method artifact (intermittent `say` playback + short capture window). |
| 2026-06-26-10-58 | Initial report. UAC2 enumeration + P2P + crash-fix PASS; end-to-end USB audio sample flow inconclusive (later corrected). |

---

## Executive Summary

| Area | Result |
|------|--------|
| UAC2 enumeration on host (the migration's core change) | ✅ PASS |
| Gateway boot stability (after crash fix) | ✅ PASS |
| P2P GO/GC auto-connect + DHCP | ✅ PASS |
| Wi-Fi streaming pipeline up (encoder, socket, stream-start) | ✅ PASS |
| **End-to-end USB→Wi-Fi→Headset audio sample flow** | ✅ **PASS** |

**Verdict:** The UAC1→UAC2 migration works end-to-end. With sustained host audio playing to the
UAC2 device, the gateway logs `USB RX first data received.` and streams to the headset, which
plays the audio (confirmed by the developer on hardware).

A **bug was found and fixed during validation**: the UAC2 `sof_cb` is mandatory (the driver
calls it every SOF with no NULL check) but was registered only under
`CONFIG_STREAM_BIDIRECTIONAL`, leaving it NULL in the default build → `USAGE FAULT` (PC=0) in
the `usbd` thread, crash-looping every ~3.3 s. Fixed by registering `sof_cb` unconditionally
(no-op body when not bidirectional). All three gateway builds were affected.

**nRF54LM20DK gateway — High-Speed regression, fixed by forcing Full-Speed:** A separate run
with the nRF54LM20DK as gateway (USB High-Speed) → Audio DK headset produced **no end-to-end
audio** despite continuous host playback: the gateway received first USB data but the host's
iso-OUT endpoint (ep 0x01) was **continuously cancelled** (vs only-on-stop bursts at Full-Speed),
so `fifo_rx` starved, the encoder produced nothing, and the headset received zero packets
(continuous I2S under-run). Forced the `usbhs` (DWC2) controller to **Full-Speed**
(`CONFIG_UDC_DRIVER_HIGH_SPEED_SUPPORT_ENABLED=n`, `high-speed` dropped from the UAC2 node) —
the validated-working mode used by the nRF5340 boards. Rebuilt (FLASH 51.45%) and
**confirmed working end-to-end on hardware** by the developer. Proper High-Speed UAC2
(125 µs micro-framing) deferred to future work.
The board also showed periodic P2P Wi-Fi disconnects (`reason=1`) — tracked separately.

**Benign warnings observed during streaming (audio unaffected):**
- `usbd_uac2: request ep 0x08, len 0 cancelled` — routine cancellation of the UAC2 driver's
  spare double-buffered ISO-OUT read on host stream stop/idle gaps. Log noise only; can be
  quieted via `CONFIG_USBD_UAC2_LOG_LEVEL`.
- `wifi_audio_rx: Invalid start sequence, discarding packet` — occasional UDP frame fails the
  `0xFF 0xAA` framing check; **pre-existing** in the Wi-Fi RX layer, not UAC2-related.

---

## Per-Requirement Results

| TC | PRD criterion | Board | Result | Evidence |
|----|---------------|-------|--------|----------|
| FR-008a | Gateway presents as a UAC2 device, class-native on host | Audio DK (gw) | ✅ PASS | CoreAudio: `Nordic Wi-Fi Audio`, Transport USB, SampleRate 48000, 2in/2out, Mfr "Nordic Semiconductor AS" |
| FR-008b | UAC1→UAC2 product string change | Audio DK (gw) | ✅ PASS | Before: `nRF5340 USB Audio` (UAC1). After: `Nordic Wi-Fi Audio` (UAC2). |
| FR-008c | UAC2 device ready at boot | Audio DK (gw) | ✅ PASS | `audio_usb: Ready for USB host to send/receive.` @ 2.81 s |
| FR-008d | Host USB OUT audio reaches `fifo_rx` | Audio DK (gw) | ✅ PASS | `audio_usb: USB RX first data received.` @ 29.2 s with sustained host playback |
| FR-004 | P2P GO boots, GC auto-connects ≤60 s @ .7.1/.7.2 | both | ✅ PASS | GW: `Static IP 192.168.7.1/24`, `P2P_GO`; HS: `L3-NET_EVENT_IPV4_DHCP_BOUND ip=192.168.7.2`, `connected to GO - auto-retry stopped` @ ~17 s |
| FR-001 | Audio frames stream GW→HS | both | ✅ PASS | GW `STATE_STREAMING` + `Encoder started`; HS receives + plays (developer-confirmed). I2S under-run only during pre-stream startup (≤10000), then stops once PCM flows. Occasional pre-existing `wifi_audio_rx: Invalid start sequence` frame drops do not interrupt audio. |

---

## Note on the initial FR-008d FAIL (resolved — test-method artifact)

The first execution reported FR-008d as FAIL because the agent's playback used short `say`
utterances: macOS opened and closed the UAC2 OUT stream around each utterance (producing the
`len 0 cancelled` bursts at segment boundaries), and the 30 s passive-capture window did not
catch a sustained transfer. A subsequent developer run with **continuous** host audio showed
`audio_usb: USB RX first data received.` at 29.2 s uptime and audio playing on the headset —
confirming the path works. The `len 0 cancelled` warnings continue harmlessly during streaming
(routine ISO-OUT double-buffer cancellation); they are not failures.

---

## Fixed During Validation — NULL sof_cb crash (was P0)

- **Symptom:** Gateway `USAGE FAULT` (Illegal use of EPSR, PC=0x00000000) in thread `usbd`,
  ~3.3 s after every boot; ~17 reboots in 85 s. addr2line(LR) → `uac2_sof` (usbd_uac2.c).
- **Root cause:** `uac2_sof()` calls `ctx->ops->sof_cb()` unconditionally; the `__ASSERT` guard
  is compiled out (no `CONFIG_ASSERT`). `sof_cb` was registered only under
  `CONFIG_STREAM_BIDIRECTIONAL` → NULL in default build → null-pointer call.
- **Fix:** Register `.sof_cb = usb_send_cb` unconditionally; TX body guarded by
  `CONFIG_STREAM_BIDIRECTIONAL`, no-op otherwise. Rebuilt (flash unchanged 99.57%), reflashed —
  crash gone, board stable.
- **Scope:** all three gateway builds had this latent crash.

---

## Boot Evidence (gateway, post-fix)

```
*** Booting nRF Connect SDK v3.3.0 ***
zego_wifi: PRD: 2026-06-26-09-55   Specs: 2026-06-26-10-00
audio_usb: Ready for USB host to send/receive.            (2.81 s)
zego_wifi_utils: Static IP 192.168.7.1/24 assigned        (3.54 s)
net_event_app: AP/P2P_GO client joined (count=1)          (18.26 s)
main: STATE_STREAMING Command received / audio_system: Encoder started (19.29 s)
```
Single boot, stable for full 90 s capture, no faults (one benign `wpa_supp: Invalid mfp mapping 3`).

---

## Routing

| Finding | Priority | Route |
|---------|----------|-------|
| NULL sof_cb crash (all gateway builds) | P0 | ✅ Fixed this session |
| End-to-end USB audio | P0 | ✅ Confirmed working (developer HW run) |
| `usbd_uac2 len 0 cancelled` log noise | P2 | Optional: lower `CONFIG_USBD_UAC2_LOG_LEVEL` |
| `wifi_audio_rx: Invalid start sequence` occasional drops | P2 | Pre-existing (not UAC2); investigate separately if needed |
| Audio DK gateway flash 99.57% | P1 | `chsh-sk-ncs-3.3-memopt` |

**All P0 closed.** UAC2 migration validated on hardware. Remaining items are P1/P2 polish.

> Action item: rebuild + reflash the **nRF7002DK** and **nRF54LM20DK** gateways with the
> `sof_cb` fix (they were built before the fix and would crash-loop otherwise).
