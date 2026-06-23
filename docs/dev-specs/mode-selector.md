# Mode Selector Spec [RETIRED] - nordic-wifi-audio

## Document Information

| Field | Value |
|---|---|
| Project | Nordic Wi-Fi Audio Demo |
| Version | 2026-06-22-15-18 |
| PRD Version | 2026-06-22-15-18 |
| NCS Version | v3.3.0 |
| Status | Retired |

## Changelog

| Version | Summary of changes |
|---|---|
| 2026-06-22-15-18 | Created as tombstone: custom mode_selector was never implemented; mode persistence is owned by zego/wifi brick |

---

## Status: Retired / Superseded

A custom `src/net/mode_selector.c/.h` (NVS-backed STA/P2P toggle, shell command,
button long-press reboot) was developed as a WIP parallel to this refactor.
In the zego-brick architecture, that custom code is **superseded** by the zego/wifi
brick, which provides the same functionality (`WIFI_MODE_CHAN`, NVS key `app/app_wifi_mode`,
shell command `app_wifi_mode`). The custom `mode_selector.c/.h` is retired in Step 3.5.

---

## Current Implementation: zego/wifi Brick

Wi-Fi mode selection and NVS persistence are handled by `zego/bricks/wifi`:

- **Spec**: `zego/bricks/wifi/docs/wifi-spec.md`
- **Kconfig**: `CONFIG_ZEGO_WIFI=y`, `CONFIG_ZEGO_WIFI_DEFAULT_MODE_P2P_GO=y` (gateway), `CONFIG_ZEGO_WIFI_DEFAULT_MODE_P2P_CLIENT=y` (headset)
- **Channel**: `WIFI_MODE_CHAN` — published once at `SYS_INIT` (APPLICATION priority 0)
- **Persistence**: NVS key `app/app_wifi_mode` via Zephyr Settings subsystem
- **Shell command**: `app_wifi_mode [sta|p2p_go|p2p_client]` — runtime mode switch + reboot

### Mode Cycle (App-Layer, not Brick)

The application's `ux.c` module implements the long-press mode cycle:

`STA → P2P_GO → P2P_CLIENT → STA`

Mode is saved via `zego_wifi_set_mode_and_reboot()` (provided by the wifi brick).
See `docs/dev-specs/ui-module.md` for the ux module spec.

### Default Modes

| Role | Default mode | Kconfig |
|---|---|---|
| Gateway | `P2P_GO` | `CONFIG_ZEGO_WIFI_DEFAULT_MODE_P2P_GO=y` |
| Headset | `P2P_CLIENT` | `CONFIG_ZEGO_WIFI_DEFAULT_MODE_P2P_CLIENT=y` |

STA mode is selected either by long-press mode cycle or by a build overlay
(e.g., `overlay-sta.conf` setting `CONFIG_ZEGO_WIFI_DEFAULT_MODE_STA=y`).
