# UI Module Spec (Buttons, LEDs, Channel Assignment)

## Document Information

| Field          | Value                        |
|----------------|------------------------------|
| Project        | Nordic Wi-Fi Opus Audio Demo |
| NCS Version    | v3.3.0                       |
| PRD Version    | 2026-05-27-23-14             |
| Latest Version | 2026-05-27-23-14             |

## Changelog

| Version          | Summary of changes                                         |
|------------------|------------------------------------------------------------|
| 2026-05-27-23-14 | Initial spec derived from code (Mode C Reverse)            |

---

## Overview

Three source files handle all user-facing I/O (buttons, LEDs, channel selection):

| File                             | Role                                                   |
|----------------------------------|--------------------------------------------------------|
| `src/modules/button_handler.c`   | GPIO debounce, publishes `button_chan` Zbus message     |
| `src/modules/led.c`              | RGB + mono LED control via GPIO                        |
| `src/utils/channel_assignment.c` | L/R/GW audio channel selection (compile-time or UICR) |

---

## File Locations

```
src/modules/
├── button_handler.c/h      — GPIO interrupt + debounce, Zbus publish
├── led.c/h                 — LED unit API, blink timer
└── button_assignments.h    — button pin aliases (DT_ALIAS sw0..sw4)
src/utils/
└── channel_assignment.c/h  — channel get/set, UICR-backed persistence
```

---

## Button Assignments

Buttons are mapped via DTS aliases. The `enum button_pin_names` in
`button_assignments.h` maps alias names to GPIO pin numbers at compile time:

| Alias | `button_pin_names` enum  | Default action (gateway)   | Default action (headset)    |
|-------|--------------------------|----------------------------|-----------------------------|
| sw0   | `BUTTON_VOLUME_DOWN`     | Volume down                | Volume down                 |
| sw1   | `BUTTON_VOLUME_UP`       | Volume up                  | Volume up                   |
| sw2   | `BUTTON_PLAY_PAUSE`      | Start/stop stream          | Start/stop stream           |
| sw3   | `BUTTON_4`               | Mute toggle                | Channel select (L/R)        |
| sw4   | `BUTTON_5`               | (board-specific secondary) | (board-specific secondary)  |

### Board-Specific Alias Remapping

Because not all boards have 5 physical buttons, DTS overlays remap aliases:

| Board             | Physical buttons | sw2 mapped to | sw3 mapped to | sw4 mapped to |
|-------------------|-----------------|---------------|---------------|---------------|
| nRF5340 Audio DK  | 4 (sw0–sw3)      | button2       | button3       | (no sw4)      |
| nRF7002DK         | 2 (sw0–sw1)      | button1       | button0       | button1       |
| nRF54LM20DK       | 3 (sw0–sw2; sw3 deleted by nRF7002EB2 shield) | button2 | button1 | button0 |

The switch statement in `main.c` requires `BUTTON_PLAY_PAUSE` pin ≠ `BUTTON_4`
pin (both are `case` labels — duplicate values cause a compile error).

---

## Zbus Integration

| Channel       | Direction | Message type         | Publisher condition        |
|---------------|-----------|----------------------|----------------------------|
| `button_chan`  | Publish   | `struct button_msg`  | On each debounced press    |
| `volume_chan`  | (main publishes after button_chan consumed) | `struct volume_msg` | — |

`button_handler` publishes `{button_pin, BUTTON_PRESS}` to `button_chan`.
`main`'s `button_msg_sub_thread` consumes it and dispatches:
- `BUTTON_VOLUME_DOWN/UP` → publishes `volume_chan`
- `BUTTON_PLAY_PAUSE` → calls `stream_state_set()`
- `BUTTON_4` → mute toggle or channel assignment

---

## LED Unit Definitions

Defined in `src/modules/led.h`:

| Constant        | Type  | Physical location                     |
|-----------------|-------|---------------------------------------|
| `LED_APP_RGB`   | RGB   | App status RGB LED                    |
| `LED_NET_RGB`   | RGB   | Network status RGB LED                |
| `LED_APP_1_BLUE`| Mono  | Blue indicator LED 1                  |
| `LED_APP_2_GREEN`| Mono | Green indicator LED 2                 |
| `LED_APP_3_GREEN`| Mono | Green indicator LED 3 (blinks at boot)|

### LED State Scheme (implemented in `nrf5340_audio_dk.c` / `nrf54l_init.c`)

| State           | LED_APP_RGB       | LED_APP_3_GREEN  |
|-----------------|-------------------|------------------|
| Initializing    | —                 | Blinking          |
| Boot complete   | Solid green       | —                 |
| Streaming       | Solid green       | —                 |
| Error           | Solid red         | —                 |

---

## Kconfig Flags

| Symbol                            | Description                                   | Default  |
|-----------------------------------|-----------------------------------------------|----------|
| `CONFIG_BUTTON_DEBOUNCE_MS`       | Button debounce time (ms)                     | 50       |
| `CONFIG_BUTTON_MSG_SUB_QUEUE_SIZE`| Zbus button subscriber queue depth            | 4        |
| `CONFIG_BUTTON_MSG_SUB_STACK_SIZE`| Button subscriber thread stack size (bytes)   | 1024     |
| `CONFIG_BUTTON_MSG_SUB_THREAD_PRIO` | Button subscriber thread priority           | 5        |
| `CONFIG_BUTTON_PUBLISH_STACK_SIZE`| Button publish thread stack size              | 1024     |
| `CONFIG_BUTTON_PUBLISH_THREAD_PRIO` | Button publish thread priority              | 5        |
| `CONFIG_AUDIO_HEADSET_CHANNEL`    | Headset audio channel selection mode         | compile-time |
| `CONFIG_AUDIO_HEADSET_CHANNEL_COMPILE_TIME` | Fix channel at compile time        | y        |
| `CONFIG_AUDIO_HEADSET_CHANNEL_RUNTIME` | Allow channel change at runtime       | n        |

---

## API / Public Interface

### `button_handler.h`
```c
int button_handler_init(void);
int button_pressed(gpio_pin_t button_pin, bool *button_pressed);
```

### `led.h`
```c
int led_init(void);
int led_blink(uint8_t led_unit, ...);   /* vararg: color for RGB */
int led_on(uint8_t led_unit, ...);
int led_off(uint8_t led_unit);
```

### `channel_assignment.h`
```c
void channel_assignment_get(enum audio_channel *channel);
void channel_assignment_set(enum audio_channel channel);  /* runtime mode only */
```

---

## Error Handling

| Condition                         | Handling                                              |
|-----------------------------------|-------------------------------------------------------|
| `gpio` driver not found           | `button_handler_init()` returns `-ENODEV`             |
| LED module not initialized        | `led_*()` returns `-EPERM`                            |
| Invalid LED color                 | Returns `-EINVAL`                                     |
| Wrong core's LED unit accessed    | Returns 0 silently (no-op)                            |

---

## Test Points

| UART log string                     | Expected condition                        |
|-------------------------------------|-------------------------------------------|
| `Button handler initialized`        | `button_handler_init()` success           |
| `LED initialized`                   | `led_init()` success                      |
| `Button pressed: pin=<n>`           | Debounced button press detected           |
| `Volume: <n>`                       | Volume change processed                   |
| `Stream state: STREAMING`           | PLAY_PAUSE button activated stream        |
| `Channel: HL` / `HR` / `GW`        | Audio channel assignment confirmed        |
