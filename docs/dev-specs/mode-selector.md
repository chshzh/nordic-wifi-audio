# Mode Selector Spec [RETIRED] - nordic-wifi-audio

## Document Information

| Field | Value |
|---|---|
| Project | Nordic Wi-Fi Audio Demo |
| Version | 2026-07-31-14-13 |
| PRD Version | 2026-07-31-14-13 |
| NCS Version | v3.4.0 |
| Status | Retired |

## Changelog

| Version | Summary of changes |
|---|---|
| 2026-07-31-14-13 | Trimmed to remove duplication with [zego/bricks/wifi's own spec](../../../zego/bricks/wifi/docs/wifi-spec.md) — generic Kconfig/shell/banner mechanics now live there only. Fixed a stale claim: the mode cycle (`src/modules/ux/ux.c`) persists via `settings_save_one()` + `sys_reboot()` directly, not a `zego_wifi_set_mode_and_reboot()` helper (no such function exists in the current brick). NCS v3.4.0. |
| 2026-06-25-13-35 | Updated to PRD v2026-06-25-13-30: per-mode ZEGO_WIFI_MODE_*_ENABLED Kconfigs; dual-mode firmware with per-role mode visibility (gateway STA+P2P_GO, headset STA+P2P_GC); banner hint only when >1 mode; P2P Client→P2P_GC rename |
| 2026-06-22-15-18 | Created as tombstone: custom mode_selector was never implemented; mode persistence is owned by zego/wifi brick |

---

## Status: Retired / Superseded

A custom `src/net/mode_selector.c/.h` (NVS-backed STA/P2P toggle, shell command,
button long-press reboot) was developed as a WIP parallel to this refactor.
In the zego-brick architecture, that custom code is **superseded** by the zego/wifi
brick, which provides the same functionality (`WIFI_MODE_CHAN`, NVS key `app/zego_wifi_mode`,
shell command `zego_wifi_mode`). The custom `mode_selector.c/.h` is retired in Step 3.5.

---

## Current Implementation: zego/wifi Brick

Wi-Fi mode selection and NVS persistence are owned entirely by `zego/bricks/wifi`
(`CONFIG_ZEGO_WIFI=y`) — see its own spec for the shell command, `WIFI_MODE_CHAN`,
NVS persistence mechanics, and the full `ZEGO_WIFI_MODE_*_ENABLED` / banner-hint
Kconfig reference, none of which are restated here:
[wifi-spec.md](../../../zego/bricks/wifi/docs/wifi-spec.md).

This section covers only what's specific to this project: which modes each role
exposes, and the app-layer mode cycle.

**Per-role mode visibility** (role overlays on top of the shared dual-mode firmware):

- **Gateway** overlay sets `CONFIG_ZEGO_WIFI_MODE_P2P_GC_ENABLED=n` → exposes **STA + P2P_GO**.
- **Headset** overlay sets `CONFIG_ZEGO_WIFI_MODE_P2P_GO_ENABLED=n` → exposes **STA + P2P_GC**.

### Mode Cycle (App-Layer, not Brick)

`src/modules/ux/ux.c` overrides `zego_ux_on_long_press()` (a `zego/bricks/ux` weak hook —
see [ui-module.md](ui-module.md)) to implement the button long-press mode cycle:

`STA → P2P_GO → P2P_GC → STA` (SoftAP excluded — see [ui-module.md](ui-module.md))

Mode is persisted directly via `settings_save_one("app/zego_wifi_mode", ...)` followed by
`sys_reboot(SYS_REBOOT_COLD)` — there is no `zego_wifi_set_mode_and_reboot()` helper in the
current brick.

### Default Modes

The default mode applied on a fresh flash (empty NVS):

| Role | Default mode | Kconfig |
|---|---|---|
| Gateway | `P2P_GO` | `CONFIG_ZEGO_WIFI_DEFAULT_MODE_P2P_GO=y` |
| Headset | `P2P_GC` | `CONFIG_ZEGO_WIFI_DEFAULT_MODE_P2P_GC=y` |

Each `CONFIG_ZEGO_WIFI_DEFAULT_MODE_*` choice option now depends on the matching
`ZEGO_WIFI_MODE_*_ENABLED` symbol, so a role can only default to a mode it exposes.

STA mode is selected either by the Button 0 long-press mode cycle or by a build overlay
(e.g., `overlay-sta.conf` setting `CONFIG_ZEGO_WIFI_DEFAULT_MODE_STA=y`).
