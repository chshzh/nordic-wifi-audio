# Mode Selector Spec [RETIRED] - nordic-wifi-audio

## Document Information

| Field | Value |
|---|---|
| Project | Nordic Wi-Fi Audio Demo |
| Version | 2026-06-25-13-35 |
| PRD Version | 2026-06-25-13-30 |
| NCS Version | v3.3.0 |
| Status | Retired |

## Changelog

| Version | Summary of changes |
|---|---|
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

Wi-Fi mode selection and NVS persistence are handled by `zego/bricks/wifi`:

- **Spec**: `zego/bricks/wifi/docs/wifi-spec.md`
- **Kconfig**: `CONFIG_ZEGO_WIFI=y`, `CONFIG_ZEGO_WIFI_DEFAULT_MODE_P2P_GO=y` (gateway), `CONFIG_ZEGO_WIFI_DEFAULT_MODE_P2P_GC=y` (headset)
- **Channel**: `WIFI_MODE_CHAN` — published once at `SYS_INIT` (APPLICATION priority 0)
- **Persistence**: NVS key `app/zego_wifi_mode` via Zephyr Settings subsystem
- **Shell command**: `zego_wifi_mode [sta|p2p_go|p2p_gc]` (plus `softap` when SoftAP is enabled) — saves the mode to NVS and cold-reboots; the listed options are gated by the `ZEGO_WIFI_MODE_*_ENABLED` symbols

### Single Dual-Mode Firmware

The default build (with `-Dnordic-wifi-audio_SNIPPET=wifi-p2p`) compiles in **both** P2P and STA
support; the active mode is NVS-persisted and runtime-switchable (no separate STA-only vs P2P-only
firmware). Which modes a given build exposes — in the boot banner and in the `zego_wifi_mode` shell
command — is gated by per-mode enable Kconfigs in the wifi brick:

| Kconfig | Default | Notes |
|---|---|---|
| `CONFIG_ZEGO_WIFI_MODE_STA_ENABLED` | `y` | STA is always available |
| `CONFIG_ZEGO_WIFI_MODE_P2P_GO_ENABLED` | `y` if `NRF70_P2P_MODE` | Group Owner |
| `CONFIG_ZEGO_WIFI_MODE_P2P_GC_ENABLED` | `y` if `NRF70_P2P_MODE` | Group Client (renamed from P2P Client) |
| `CONFIG_ZEGO_WIFI_MODE_SOFTAP_ENABLED` | `y` if `NRF70_AP_MODE && !NRF70_P2P_MODE` | Renamed from `ZEGO_WIFI_SOFTAP_ENABLED` |

**Per-role mode visibility** is set by the role overlays on top of the shared dual-mode firmware:

- **Gateway** overlay sets `CONFIG_ZEGO_WIFI_MODE_P2P_GC_ENABLED=n` → exposes **STA + P2P_GO**.
- **Headset** overlay sets `CONFIG_ZEGO_WIFI_MODE_P2P_GO_ENABLED=n` → exposes **STA + P2P_GC**.

**Banner mode-switch hint**: the boot banner prints the mode-switch hint only when more than one
mode is compiled in. This is a compile-time `_ZEGO_WIFI_MODE_COUNT > 1` guard that sums `IS_ENABLED`
over the four `MODE_*_ENABLED` symbols; a single-mode build prints no hint.

### Mode Cycle (App-Layer, not Brick)

The application's `ux.c` module implements the Button 0 long-press mode cycle:

`STA → P2P_GO → P2P_GC → STA`

Mode is saved via `zego_wifi_set_mode_and_reboot()` (provided by the wifi brick).
See `docs/dev-specs/ui-module.md` for the ux module spec.

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
