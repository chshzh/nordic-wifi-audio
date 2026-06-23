/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
 * Transition proxy — found first by #include "led.h" in src/modules/ux/.
 * Provides the zego LED brick API. Remove when src/modules/led.h is retired
 * (Step 3.5).
 *
 * Content copied from zego/bricks/led/src/led.h.
 */

#ifndef ZEGO_LED_H
#define ZEGO_LED_H

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/zbus/zbus.h>

enum led_msg_type {
	LED_COMMAND_ON,
	LED_COMMAND_OFF,
	LED_COMMAND_TOGGLE,
	LED_COMMAND_BLINK,
	LED_COMMAND_BREATHE,
	LED_COMMAND_ROTATE,
};

struct led_msg {
	enum led_msg_type type;
	uint8_t led_number;
	uint16_t period_ms;
	uint8_t rotate_count;
	uint8_t rotate_indices[CONFIG_ZEGO_LED_NUM_LEDS];
};

struct led_state_msg {
	uint8_t led_number;
	bool is_on;
	enum led_msg_type command;
};

ZBUS_CHAN_DECLARE(LED_CMD_CHAN);
ZBUS_CHAN_DECLARE(LED_STATE_CHAN);

int led_get_state(uint8_t led_number, bool *state);

#endif /* ZEGO_LED_H */
