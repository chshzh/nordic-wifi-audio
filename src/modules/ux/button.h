/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
 * Transition proxy — found first by #include "button.h" in src/modules/ux/.
 * Provides the zego button brick API. Remove when src/modules/button_handler.h
 * is retired (Step 3.5).
 *
 * Content copied from zego/bricks/button/src/button.h.
 */

#ifndef ZEGO_BUTTON_H
#define ZEGO_BUTTON_H

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/zbus/zbus.h>

enum button_msg_type {
	BUTTON_PRESSED,
	BUTTON_RELEASED,
	BUTTON_SINGLE_CLICK,
	BUTTON_DOUBLE_CLICK,
	BUTTON_LONG_PRESS,
};

struct button_msg {
	enum button_msg_type type;
	uint8_t button_number;
	uint32_t duration_ms;
	uint32_t press_count;
	uint32_t timestamp;
};

ZBUS_CHAN_DECLARE(BUTTON_CHAN);

#endif /* ZEGO_BUTTON_H */
