/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <nrfx_clock.h>

/* led.h / button_handler.h retired in Step 3.5 — zego bricks handle GPIO */
#include "board_version.h"
#include "channel_assignment.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(nrf5340_audio_dk, CONFIG_MODULE_NRF5340_AUDIO_DK_LOG_LEVEL);

static struct board_version board_rev;

int nrf5340_audio_dk_init(void)
{
	int ret;

	/* LED and button init removed — zego/led and zego/button bricks initialize via SYS_INIT */

#if IS_ENABLED(CONFIG_BOARD_NRF5340_AUDIO_DK_NRF5340_CPUAPP)
	ret = board_version_valid_check();
	if (ret) {
		return ret;
	}

	ret = board_version_get(&board_rev);
	if (ret) {
		return ret;
	}
#endif /* CONFIG_BOARD_NRF5340_AUDIO_DK_NRF5340_CPUAPP */

	/* Use this to turn on 128 MHz clock for cpu_app */
	ret = nrfx_clock_divider_set(NRF_CLOCK_DOMAIN_HFCLK, NRF_CLOCK_HFCLK_DIV_1);
	if (ret) {
		LOG_ERR("Failed to set HFCLK divider: %d", ret);
		return ret;
	}

	return 0;
}
