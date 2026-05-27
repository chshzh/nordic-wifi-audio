/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
 * Board init for nRF54L-series targets (e.g. nRF54LM20DK).
 * Provides nrf5340_audio_dk_init() — same symbol called from main.c — so
 * the rest of the audio application code does not need to be aware of
 * which SoC family is used.
 *
 * Differences from the nRF5340 Audio DK version:
 *  - No board-version ADC detection (nRF5340 Audio DK specific).
 *  - No nrfx_clock_divider_set() — nRF54LM20A clock is configured via DTS
 *    (&hfpll { clock-frequency = <128000000>; }).
 */

#include "nrf5340_audio_dk.h"

#include "led.h"
#include "button_handler.h"
#include "button_assignments.h"
#include "channel_assignment.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(nrf54l_init, CONFIG_MODULE_NRF5340_AUDIO_DK_LOG_LEVEL);

static int leds_set(void)
{
	int ret;

	/* Blink LED 3 to indicate that APP core is running */
	ret = led_blink(LED_APP_3_GREEN);
	if (ret) {
		return ret;
	}

#if (CONFIG_AUDIO_GATEWAY)
	ret = led_on(LED_APP_RGB, LED_COLOR_GREEN);
	if (ret) {
		return ret;
	}
#endif

	return 0;
}

int nrf5340_audio_dk_init(void)
{
	int ret;

	ret = led_init();
	if (ret) {
		LOG_ERR("Failed to initialize LED module");
		return ret;
	}

	ret = button_handler_init();
	if (ret) {
		LOG_ERR("Failed to initialize button handler");
		return ret;
	}

	ret = leds_set();
	if (ret) {
		LOG_ERR("Failed to set LEDs");
		return ret;
	}

	return 0;
}
