# UI Module Spec — zego/bricks/ux Gesture Override

## Document Information

| Field | Value |
|---|---|
| Project | Nordic Wi-Fi Audio Demo |
| Version | 2026-07-31-14-13 |
| PRD Version | 2026-07-31-14-13 |
| NCS Version | v3.4.0 |
| Target Board(s) | nRF5340 Audio DK + nRF7002EK (P0); nRF7002DK, nRF54LM20DK + nRF7002EB2 (build) |
| Status | Implemented — long-press override only (everything else owned by `zego/bricks/ux`) |

## Changelog

| Version | Summary of changes |
|---|---|
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
connectivity changes to drive the brick's LED state machine — see
[network-module.md](network-module.md) for those hooks.

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
```

`ux.c` is built as its **own `zephyr_library_named(app_ux)`**, not compiled directly
into `app`. This is a header-resolution workaround, not a design choice: the app's own
private include path puts `src/modules` (which still holds the legacy nRF5340 Audio DK
`led.h`, used elsewhere for on-board hardware LED color constants) ahead of the zego
brick's `<led.h>`/`<ux.h>`. A separate library sees only the global brick include
directories, so the angle-bracket includes resolve to the zego headers. See
`src/modules/ux/CMakeLists.txt` for the full rationale, including the
`add_dependencies(app_ux zephyr_generated_headers)` needed because Zephyr only wires
that dependency up automatically for libraries registered before `find_package(Zephyr)`
returns.

---

## The one override: `zego_ux_on_long_press()`

The mode-control button (`CONFIG_ZEGO_UX_BUTTON_IDX` — index 4/BTN5 on nRF5340 Audio DK
so VOL− stays free; index 0 on nRF7002DK/nRF54LM20DK) carries three gestures. Only
long-press is overridden — single-click and double-click keep the brick defaults:

| Gesture | zego/ux default | This app's behavior | Resolution |
|---|---|---|---|
| Single-click | Log current Wi-Fi mode | *(same)* | **Not overridden** — kept at zego/ux default |
| Double-click | P2P modes: trigger WPS PBC pairing (FR-013, see [network-module.md](network-module.md#p2p-pairing-flow-fr-013)); else: BLE-prov toggle | *(same — BLE prov not enabled, so effectively a no-op outside P2P)* | **Not overridden** — kept at zego/ux default |
| Long-press (≥ 3 s) | Cycle STA→SoftAP→P2P_GO→P2P_GC→STA, save to NVS, reboot | Cycle **STA→P2P_GO→P2P_GC→STA** (SoftAP excluded) | **Overridden** |

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
and LED test points — this app does not override those.
