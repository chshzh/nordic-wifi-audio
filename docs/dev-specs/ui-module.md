# UI Module Spec — App UX Module (Buttons, LEDs)

## Document Information

| Field | Value |
|---|---|
| Project | Nordic Wi-Fi Audio Demo |
| Version | 2026-06-22-15-18 |
| PRD Version | 2026-06-22-15-18 |
| NCS Version | v3.3.0 |
| Target Board(s) | nRF5340 Audio DK + nRF7002EK (P0); nRF7002DK, nRF54LM20DK + nRF7002EB2 (build) |
| Status | In Review |

## Changelog

| Version | Summary of changes |
|---|---|
| 2026-06-22-15-18 | Rewrite: app ux module replaces custom button_handler.c + led.c; zego bricks own hardware; mode cycle updated (STA→P2P_GO→P2P_CLIENT, no SoftAP) |
| 2026-05-27-23-14 | Initial spec derived from code (Mode C Reverse) |

---

## Overview

The UI module is split into two layers:

| Layer | Owned by | Role |
|---|---|---|
| Hardware | zego/button brick (`BUTTON_CHAN`) | GPIO debounce, gesture classification (SINGLE_CLICK, LONG_PRESS) |
| Hardware | zego/led brick (`LED_CMD_CHAN`) | LED state machine, animations (ROTATE, BLINK, ON, BREATHE) |
| Policy | `src/modules/ux/ux.c` (app) | Button gesture → mode action; APP_WIFI_STATE_CHAN → LED command |

Custom `src/modules/button_handler.c` and `src/modules/led.c` are retired in Step 3.5.
Channel assignment (`src/utils/channel_assignment.c`) is kept for I2S audio channel selection.

---

## File Locations (after refactor)

```
src/modules/ux/
├── ux.c          — button gesture handler + LED state machine
├── Kconfig       — CONFIG_APP_UX_MODULE, LED index config
└── CMakeLists.txt
src/utils/
└── channel_assignment.c/h  — L/R/GW audio channel selection (kept, audio domain)
```

---

## Button 0 Gesture Mapping

The ux module subscribes to `BUTTON_CHAN` (zego/button brick). Button 0 (`sw0`) is the
dedicated mode/state button:

| Gesture | Action |
|---|---|
| `SINGLE_CLICK` | Print current Wi-Fi mode to UART (no reboot, informational only) |
| `LONG_PRESS` (≥ 3 s) | Cycle Wi-Fi mode → save to NVS → reboot into new mode |

Mode cycle order: **STA → P2P_GO → P2P_CLIENT → STA** (wraps around; no SoftAP).

Audio-domain buttons (volume up/down, play/pause) remain on the legacy `button_chan`
path through `button_msg_sub_thread` in main.c until Step 3.5 fully consolidates.

---

## LED State Machine

The ux module subscribes to `APP_WIFI_STATE_CHAN` (published by `net_event_app.c`) and
translates states into `LED_CMD_CHAN` commands to the zego/led brick.

The ux module targets **LED index 0** (or `CONFIG_APP_UX_WIFI_LED_IDX`) as the
Wi-Fi status LED.

```
stateDiagram-v2
    [*] --> ROTATE : SYS_INIT (app_ux_ready = true)

    ROTATE : LED 0 ROTATE animation
    ROTATE --> ON : APP_WIFI_STATE_CONNECTED
    ROTATE --> BLINK_FAST : APP_WIFI_STATE_ERROR

    ON : LED 0 solid ON
    ON --> ROTATE : APP_WIFI_STATE_CONNECTING
    ON --> BLINK_FAST : APP_WIFI_STATE_ERROR

    BLINK_FAST : LED 0 fast blink (100 ms half-period)
    BLINK_FAST --> ROTATE : APP_WIFI_STATE_CONNECTING
    BLINK_FAST --> ON : APP_WIFI_STATE_CONNECTED
```

LED commands published on `LED_CMD_CHAN`:
- `ROTATE`: Wi-Fi connecting / scanning / waiting for P2P peer
- `ON` (solid): Connected and audio link active
- `BLINK` (fast): Error / disconnected unexpectedly

---

## Mode Cycle Implementation

```c
/* src/modules/ux/ux.c — long-press handler */
static void cycle_wifi_mode(void)
{
    enum zego_wifi_mode current;
    zbus_chan_read(&WIFI_MODE_CHAN, &current, K_MSEC(10));

    enum zego_wifi_mode next;
    switch (current) {
    case ZEGO_WIFI_MODE_STA:       next = ZEGO_WIFI_MODE_P2P_GO;     break;
    case ZEGO_WIFI_MODE_P2P_GO:    next = ZEGO_WIFI_MODE_P2P_CLIENT; break;
    case ZEGO_WIFI_MODE_P2P_CLIENT:next = ZEGO_WIFI_MODE_STA;        break;
    default:                       next = ZEGO_WIFI_MODE_P2P_GO;     break;
    }

    LOG_INF("Mode cycle: %s → %s",
            zego_wifi_mode_str(current), zego_wifi_mode_str(next));

    /* zego/wifi brick provides this API to save + reboot */
    zego_wifi_set_mode_and_reboot(next);
}
```

---

## Race Condition Handling

Zbus `APP_WIFI_STATE_CHAN` events can arrive before ux.c finishes its `SYS_INIT`
callback. The ux module uses an atomic `app_ux_ready` flag (set at end of
`SYS_INIT`) and defers LED work to the system workqueue via `k_work_schedule`.
State events that arrive before `app_ux_ready` are replayed once the flag is set.

---

## Zbus Integration

| Channel | Direction | Notes |
|---|---|---|
| `BUTTON_CHAN` | Subscribe | Gestures from zego/button brick (SINGLE_CLICK, LONG_PRESS) |
| `WIFI_MODE_CHAN` | Read | Read once at SINGLE_CLICK to print mode; read in mode cycle |
| `APP_WIFI_STATE_CHAN` | Subscribe | Published by net_event_app.c; drives LED state machine |
| `LED_CMD_CHAN` | Publish | LED commands to zego/led brick |

---

## Kconfig Flags

| Symbol | Description | Default |
|---|---|---|
| `CONFIG_APP_UX_MODULE` | Enable the app ux module | y |
| `CONFIG_APP_UX_INIT_PRIORITY` | SYS_INIT APPLICATION priority | 0 |
| `CONFIG_APP_UX_WIFI_LED_IDX` | LED index for Wi-Fi status (0 = first LED) | 0 |
| `CONFIG_ZEGO_BUTTON_LONG_PRESS_MS` | Long-press threshold for mode cycle | 3000 |
| `CONFIG_ZEGO_WIFI_DEFAULT_MODE_P2P_GO` | Gateway default: P2P_GO | y |
| `CONFIG_ZEGO_WIFI_DEFAULT_MODE_P2P_CLIENT` | Headset default: P2P_CLIENT | y |

Per-board button/LED counts set in `boards/*.conf`:

| Board | `CONFIG_ZEGO_BUTTON_NUM_BUTTONS` | `CONFIG_ZEGO_LED_NUM_LEDS` |
|---|---|---|
| nRF5340 Audio DK | 5 (sw0–sw4) | 9 (RGB1: 0–2, RGB2: 3–5, mono: 6–8) |
| nRF7002DK | 2 (sw0–sw1) | 2 |
| nRF54LM20DK | 3 (sw0–sw2; sw3 removed by shield) | 4 |

---

## Channel Assignment (kept)

`src/utils/channel_assignment.c` remains for audio I2S channel selection (L/R/GW).
It is not affected by the UI module refactor.

```c
void channel_assignment_get(enum audio_channel *channel);
void channel_assignment_set(enum audio_channel channel);  /* runtime mode only */
```

---

## Error Handling

| Condition | Handling |
|---|---|
| Zbus publish to `LED_CMD_CHAN` fails | `LOG_WRN`, LED state not updated; not fatal |
| `WIFI_MODE_CHAN` read timeout | `LOG_WRN`, mode cycle uses current WIFI_MODE_CHAN value (stale but safe) |
| `zego_wifi_set_mode_and_reboot` fails | `LOG_ERR`, no reboot; user must try again |

---

## Test Points

| UART log string | Expected condition |
|---|---|
| `[ux] Mode: P2P_GO` (on single-click) | Mode print on Button 0 single-click |
| `[ux] Mode cycle: P2P_GO → P2P_CLIENT` | Long-press mode cycle executed |
| `[ux] LED → ROTATE` | APP_WIFI_STATE_CONNECTING received |
| `[ux] LED → ON` | APP_WIFI_STATE_CONNECTED received |
| `[ux] LED → BLINK` | APP_WIFI_STATE_ERROR received |
