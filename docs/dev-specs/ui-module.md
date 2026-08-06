# UI Module Spec — zego/bricks/ux Gesture Override

## Document Information

| Field | Value |
|---|---|
| Project | Nordic Wi-Fi Audio Demo |
| Version | 2026-08-06-21-23 |
| PRD Version | 2026-08-06-19-00 |
| NCS Version | v3.4.0 |
| Target Board(s) | nRF5340 Audio DK + nRF7002EK (P0); nRF7002DK, nRF54LM20DK + nRF7002EB2 (build) |
| Status | Implemented — long-press override only (everything else owned by `zego/bricks/ux`) |

## Changelog

| Version | Summary of changes |
|---|---|
| 2026-08-06-21-23 | Reversed the 21-04 fix per explicit user direction: RGB2 (idx 3-5) is Wi-Fi status again (`ROTATE_FIRST_LED`/`CONNECTED_LED`/`ERROR_LED_IDX`/`PAIRING_LED_IDX` moved back to 3/4/3/5 in `boards/nrf5340_audio_dk_nrf5340_cpuapp.conf`), RGB1 (idx 0-2) is now the role indicator (`role_led_init()` moved from idx 3/4/5 to idx 0/1/2 in `audio_led.c`). Net effect vs. the original (pre-21-04) config: same RGB2-for-Wi-Fi assignment, but the conflict is gone since the role indicator now lives on RGB1 instead of also being on RGB2. Colors unchanged (Gateway green, Headset blue, per 21-10). |
| 2026-08-06-21-10 | Swapped RGB2 role indicator colors: Gateway is now Green (was Blue), Headset is now Blue (was Green). |
| 2026-08-06-21-04 | Bug fix: `boards/nrf5340_audio_dk_nrf5340_cpuapp.conf` had `CONFIG_ZEGO_UX_ROTATE_FIRST_LED=3` / `CONFIG_ZEGO_UX_CONNECTED_LED=4`, pointing zego_ux's Wi-Fi ROTATE/CONNECTED/ERROR animation at RGB2 — the same physical LEDs `role_led_init()` (added below) owns, so the two fought over RGB2 and the role color kept getting overwritten. Moved `ROTATE_FIRST_LED`/`CONNECTED_LED`/`ERROR_LED_IDX`/`PAIRING_LED_IDX` to RGB1 (idx 0-2), matching what §2.4 always documented. See "RGB2 Role Indicator" section below. |
| 2026-08-06-20-48 | Implemented the RGB2 role indicator (nRF5340 Audio DK only) described in the PRD §2.4 since 2026-06-22 but never actually coded: `role_led_init(bool is_gateway)` in `src/modules/audio_led/audio_led.c`, called once at boot from each app's `main()` right after `zego_ux_print_banner()`. Sets RGB2 solid blue (idx 5) for gateway or solid green (idx 4) for headset, explicitly turning the other two RGB2 channels off so it isn't a mixed color. Root cause of the gap: the original implementation used a Kconfig-based `CONFIG_APP_UX_CONNECTED_LED_GREEN_ONLY` scheme in the pre-v3.4.0 custom `ux.c`, deleted wholesale when the project adopted `zego/bricks/ux` — the PRD text was never updated to match, so the feature silently disappeared. No-op on nRF7002DK/nRF54LM20DK (no RGB2 hardware). |
| 2026-08-06-19-35 | FR-015 hardware-test fix: the Headset LED was blinking even while no audio was actually flowing (command-driven `stream_state` says STREAMING while the gateway is stalled/paused). Headset now tracks a 3rd state using a new `audio_datapath_is_playing()` getter, polled every 200 ms (`audio_led_poll_work`) since playback activity changes inside the I2S callback, not through `stream_state_set()`: Solid OFF (pause sent) / Solid ON (play sent, not actually playing yet) / Blink (actually playing). Also corrected this doc: `src/modules/audio_led/audio_led.c` is **not** a separate `zephyr_library_named(...)` like `ux.c` — a real build showed that pattern never links into the final image from application-mode CMake, so it's compiled straight into `app`, with the `<led.h>` collision resolved via a `configure_file()`-generated wrapper header instead. |
| 2026-08-06-19-05 | FR-015: added `src/modules/audio_led/` — a second, audio-specific LED (idx 1 on nRF7002DK/nRF54LM20DK, idx 6 on nRF5340 Audio DK) driven by `audio_led_update(streaming, usb_active)`, called from both apps' `stream_state_set()`. Also renamed LED 0 from "Wi-Fi / audio connection state" to **Wi-Fi / Network Status LED** (FR-007) — documentation-only clarification, since LED 0 was already driven purely by `net_event_app.c`'s network hooks, never by `stream_state`. See "Audio Streaming LED (FR-015)" section below. |
| 2026-08-04-12-20 | **Bug fix (found via hardware test):** on nRF5340 Audio DK, `CONFIG_ZEGO_FACTORY_RESET_BUTTON_IDX` was left at its default (0) while `CONFIG_ZEGO_UX_BUTTON_IDX` is 4 (BTN5) — the 10 s hold on BTN5 never triggered factory reset (see [board-init-module.md](board-init-module.md) Changelog for the fix). nRF7002DK and nRF54LM20DK were unaffected (both use idx 0 for everything, so the default already matched). |
| 2026-08-04-10-58 | FR-014: enabled `CONFIG_ZEGO_BUTTON_LONGER_PRESS_MS=10000` so the mode-control button carries a second hold tier (`zego/bricks/factory_reset`, `CONFIG_ZEGO_FACTORY_RESET=y`). The existing 3 s mode-cycle long-press override (`zego_ux_on_long_press()`, unchanged) is now "guarded": it fires at release if released before 10 s, and is superseded by a factory reset if the hold continues to 10 s. See `zego/bricks/button/docs/button-spec.md` ("Two-Tier Hold Gesture") and [0-overview.md](0-overview.md). |
| 2026-07-31-14-13 | **Rewrite for the NCS v3.4.0 migration — not a duplicate of `zego/bricks/ux`'s own spec.** `zego/bricks/ux` (zego v3.4.0.2) now owns button gesture dispatch, the LED 0 Wi-Fi state machine, and the startup banner outright — all previously app-owned in `src/modules/ux/ux.c`. This app keeps exactly one strong override, `zego_ux_on_long_press()`, because the brick's default long-press cycle includes SoftAP and this project deliberately excludes it (P2P_GO already covers the zero-infrastructure role). Full generic behavior (LED state diagram, Kconfig reference, banner mechanics, single-click, double-click) is documented once in [zego/bricks/ux/docs/ux-spec.md](../../../zego/bricks/ux/docs/ux-spec.md) and not restated here. |
| 2026-06-22-15-18 | Rewrite: app ux module replaces custom button_handler.c + led.c; zego bricks own hardware; mode cycle updated (STA→P2P_GO→P2P_GC, no SoftAP) |
| 2026-05-27-23-14 | Initial spec derived from code (Mode C Reverse) |

---

## Current state

Button gestures, the LED 0 Wi-Fi state machine, and the startup banner
(`zego_ux_print_banner()`, called once from `main()`) are owned entirely by
`zego/bricks/ux` (`CONFIG_ZEGO_UX=y`) — see its own spec for the full mechanism:
[ux-spec.md](../../../zego/bricks/ux/docs/ux-spec.md).

`src/modules/network/net_event_app.c` publishes `ZEGO_UX_WIFI_STATE_CHAN` on
connectivity changes to drive the brick's LED 0 (Wi-Fi / Network Status) state
machine — see [network-module.md](network-module.md) for those hooks. LED 0's
states (ROTATE / Solid ON / Fast BLINK) reflect Wi-Fi connectivity only —
DHCP-bound, disconnect, and last-AP-client-left events — never `stream_state`;
audio state is shown on the separate Audio Streaming LED below (FR-015).

Custom `src/modules/button_handler.c` and `src/modules/led.c` were retired in Step 3.5
(pre-v3.4.0). The transition-proxy headers `src/modules/ux/{Kconfig,button.h,led.h,wifi.h}`
that briefly stood in for the zego headers during that step were deleted in the v3.4.0
migration (OI-008, resolved) — `ux.c` now includes the zego headers directly.
Channel assignment (`src/utils/channel_assignment.c`) is unrelated to this module and
kept for I2S audio channel selection.

### File locations (after v3.4.0 migration)

```
src/modules/ux/
├── ux.c          — single override: zego_ux_on_long_press() (mode cycle)
└── CMakeLists.txt

src/modules/audio_led/
├── audio_led.h   — audio_led_update(streaming, usb_active), role_led_init(is_gateway)
├── audio_led.c   — LED index selection + LED_CMD_CHAN publish (FR-015, RGB1 role indicator)
└── CMakeLists.txt
```

`ux.c` is built as its **own `zephyr_library_named(...)`**, not compiled directly
into `app`. This is a header-resolution workaround, not a design choice: the app's
own private include path puts `src/modules` (which still holds the legacy nRF5340
Audio DK `led.h`, used elsewhere for on-board hardware LED color constants) ahead
of the zego brick's `<led.h>`/`<ux.h>`. A separate library sees only the global
brick include directories, so the angle-bracket includes resolve to the zego
headers. See `src/modules/ux/CMakeLists.txt` for the full rationale, including the
`add_dependencies(app_ux zephyr_generated_headers)` needed because Zephyr only
wires that dependency up automatically for libraries registered before
`find_package(Zephyr)` returns.

`audio_led.c` hits the same `<led.h>` collision but is compiled straight into
`app` (via `target_sources(app ...)`) rather than as a separate library: a real
build showed `zephyr_library_named(...)` in application-mode CMake never gets
added to Zephyr's whole-archive link list, so its objects compiled but were never
linked (`undefined reference`). Instead, `CMakeLists.txt` uses `configure_file()`
to generate `zego_led_include.h`, a wrapper that `#include`s the zego led brick's
header via its CMake-resolved absolute path — sidestepping `-I` search order
entirely instead of depending on library separation.

---

## Audio Streaming LED (FR-015)

A second LED, independent of LED 0 (Wi-Fi / Network Status), shows USB audio
source and streaming activity:

| Board | LED idx |
|---|---|
| nRF7002DK, nRF54LM20DK + nRF7002EB2 | 1 |
| nRF5340 Audio DK + nRF7002EK (gateway and headset) | 6 |

Both apps call `audio_led_update(bool streaming, bool usb_active)` (declared in
`src/modules/audio_led/audio_led.h`) every time `stream_state_set()` runs, so the
LED always reflects the freshest state without a separate polling path:

```c
void audio_led_update(bool streaming, bool usb_active)
{
	if (streaming) {
		/* BLINK — actively streaming to a connected peer, wins over usb_active */
	} else if (usb_active) {
		/* Solid ON — USB host audio available, not yet streaming */
	} else {
		/* Solid OFF — no USB host audio */
	}
}
```

**Gateway** (`wifi_audio_gateway/main.c`): `usb_active` is
`audio_usb_host_audio_active()` (see [audio-pipeline.md](audio-pipeline.md)).
Called from `stream_state_set()` itself (covers button play/pause,
`AUDIO_START_CMD`/`AUDIO_STOP_CMD`, and client-disconnect paths) **and** once
more, unconditionally, at the end of `streamctrl_handle_usb_audio_active()` —
the latter is needed because that function can change `usb_active` without
calling `stream_state_set()` when no client is connected yet (early-return
guard), and the LED must still reflect the fresh USB signal in that case.

**Headset** (`wifi_audio_headset/main.c`): audio commands only convey intent, not
whether audio is actually arriving/playing — hardware testing showed the LED
blinking while the gateway was stalled and no audio was flowing. The headset
therefore tracks a 3-way state instead of calling `audio_led_update()` straight
from `stream_state_set()`:

```c
bool streaming_intent = (strm_state == STATE_STREAMING);
bool streaming_active = streaming_intent && audio_datapath_is_playing();
/* streaming_active -> Blink, else streaming_intent -> Solid ON, else Solid OFF */
audio_led_update(streaming_active, streaming_intent);
```

`audio_datapath_is_playing()` (new getter in `audio_datapath.c`, returns
`ctrl_blk.out.playing`) reflects real I2S output activity with the same
prebuffer/mute hysteresis the drift compensator already relies on (see
[audio-pipeline.md](audio-pipeline.md)) — it goes false while the jitter buffer
is refilling or ran dry, even though `stream_state` is still STREAMING. Since
that signal changes inside the I2S callback (not through `stream_state_set()`),
a `k_work_delayable` (`audio_led_poll_work`, 200 ms period) re-evaluates it
periodically; a dedup check (`audio_led_tri_last`) skips the `audio_led_update()`
call when the 3-way state hasn't changed, so the LED effect isn't restarted every
tick.

### Test Points

| UART observation | Expected condition |
|---|---|
| Gateway audio LED solid ON, no client connected | USB host is sending audio, no headset streaming yet |
| Gateway audio LED blinking | `stream_state_get() == STATE_STREAMING` |
| Headset audio LED solid ON | Play command sent (`stream_state == STATE_STREAMING`) but `audio_datapath_is_playing()` is false (stalled/buffering) |
| Headset audio LED blinking | `audio_datapath_is_playing()` is true — audio is actually being output |
| LED 0 state unaffected by any of the above | Confirms LED 0 remains Wi-Fi/Network-only (FR-007) |

---

## RGB1 Role Indicator (nRF5340 Audio DK only)

`role_led_init(bool is_gateway)` (`src/modules/audio_led/audio_led.c`) sets RGB1
(idx 0 red / idx 1 green / idx 2 blue, per `zephyr.dts` led0/led1/led2 aliases)
to a solid role color once at boot, called right after `zego_ux_print_banner()`
in each app's `main()`:

| Role | RGB1 color | LED command |
|---|---|---|
| Gateway | Green | ON idx 1, OFF idx 0 + idx 2 |
| Headset | Blue | ON idx 2, OFF idx 0 + idx 1 |

No-op on nRF7002DK/nRF54LM20DK (`#if defined(CONFIG_BOARD_NRF5340_AUDIO_DK_NRF5340_CPUAPP)`
guard) — those boards have no RGB1/RGB2 split (their single status LED is
zego_ux-owned only). This was documented in the PRD since 2026-06-22 but never
actually implemented until 2026-08-06 — the original Kconfig-based mechanism
(`CONFIG_APP_UX_CONNECTED_LED_GREEN_ONLY` in the pre-v3.4.0 custom `ux.c`) was
deleted wholesale when the project adopted `zego/bricks/ux`, and the PRD text
was never updated to match.

RGB2 (idx 3-5) is reserved exclusively for zego_ux's Wi-Fi status animation
(`CONFIG_ZEGO_UX_ROTATE_FIRST_LED=3`, `CONFIG_ZEGO_UX_CONNECTED_LED=4`,
`CONFIG_ZEGO_UX_ERROR_LED_IDX=3`, `CONFIG_ZEGO_UX_PAIRING_LED_IDX=5` in
`boards/nrf5340_audio_dk_nrf5340_cpuapp.conf`) — RGB1 and RGB2 must never share
responsibilities, or they fight over the same physical LEDs (a real bug found
and fixed via hardware test on 2026-08-06: the role color kept getting
overwritten by the Wi-Fi state machine's ROTATE/CONNECTED updates while both
were briefly assigned to RGB2).

### Test Points

| UART observation | Expected condition |
|---|---|
| Gateway RGB1 solid green at boot, stays green through Wi-Fi state changes | `role_led_init(true)` ran; RGB2 (not RGB1) carries ROTATE/CONNECTED/ERROR |
| Headset RGB1 solid blue at boot, stays blue through Wi-Fi state changes | `role_led_init(false)` ran; RGB2 (not RGB1) carries ROTATE/CONNECTED/ERROR |
| No RGB1/RGB2 change on nRF7002DK/nRF54LM20DK | Board guard is a no-op there |

---

## The one override: `zego_ux_on_long_press()`

The mode-control button (`CONFIG_ZEGO_UX_BUTTON_IDX` — index 4/BTN5 on nRF5340 Audio DK
so VOL− stays free; index 0 on nRF7002DK/nRF54LM20DK) carries four gestures (the fourth,
factory reset, added by `zego/bricks/factory_reset` independently of `zego/bricks/ux` —
see below). Only long-press is overridden — single-click and double-click keep the brick
defaults:

| Gesture | zego/ux default | This app's behavior | Resolution |
|---|---|---|---|
| Single-click | Log current Wi-Fi mode | *(same)* | **Not overridden** — kept at zego/ux default |
| Double-click | P2P modes: trigger WPS PBC pairing (FR-013, see [network-module.md](network-module.md#p2p-pairing-flow-fr-013)); else: BLE-prov toggle | *(same — BLE prov not enabled, so effectively a no-op outside P2P)* | **Not overridden** — kept at zego/ux default |
| Long-press (≥ 3 s, fires at release) | Cycle STA→SoftAP→P2P_GO→P2P_GC→STA, save to NVS, reboot | Cycle **STA→P2P_GO→P2P_GC→STA** (SoftAP excluded) | **Overridden** |
| Longer-press (≥ 10 s, fires while held) | — (owned by `zego/bricks/factory_reset`, not `zego/bricks/ux`) | Factory reset (FR-014) — erase Wi-Fi credentials, saved mode, P2P GO MAC; reboot; supersedes the 3 s mode-cycle gesture for that press | **Not part of ux** — `CONFIG_ZEGO_FACTORY_RESET_BUTTON` listens on the same button independently |

```c
/* src/modules/ux/ux.c — the project's only strong override */
void zego_ux_on_long_press(void)
{
    /* SoftAP deliberately excluded: P2P_GO provides the zero-infrastructure AP. */
    static const enum zego_wifi_mode mode_cycle[] = {
        ZEGO_WIFI_MODE_STA, ZEGO_WIFI_MODE_P2P_GO, ZEGO_WIFI_MODE_P2P_GC,
    };
    /* ... find current mode in mode_cycle[], advance to next ... */

    settings_save_one("app/zego_wifi_mode", &(uint8_t){next}, sizeof(uint8_t));
    sys_reboot(SYS_REBOOT_COLD);
}
```

`zego_wifi_get_mode()` and the NVS settings key (`app/zego_wifi_mode`) are unchanged
from before the migration — only the surrounding button/LED/banner plumbing moved into
the brick.

---

## Test Points

| UART log string | Expected condition |
|---|---|
| `Mode cycle: p2p_go -> p2p_gc - saving and rebooting` | Long-press mode cycle executed (app override) |

See [ux-spec.md](../../../zego/bricks/ux/docs/ux-spec.md) for single-click, double-click,
and LED test points — this app does not override those. See
[factory-reset-spec.md](../../../zego/bricks/factory_reset/docs/factory-reset-spec.md)
for the 10 s hold / `zego_factory_reset` shell command test points (FR-014) — not
overridden by this app either.
