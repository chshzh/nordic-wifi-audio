/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
 * Board init for nRF54L-series targets (e.g. nRF54LM20DK).
 * Provides nrf5340_audio_dk_init() — same symbol called from main.c.
 *
 * LED and button initialization removed in Step 3.5 — zego/led and
 * zego/button bricks initialize via SYS_INIT before main() runs.
 */

#include "nrf5340_audio_dk.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(nrf54l_init, CONFIG_MODULE_NRF5340_AUDIO_DK_LOG_LEVEL);

int nrf5340_audio_dk_init(void)
{
	/* LED and button init removed — zego bricks handle via SYS_INIT */
	return 0;
}
