/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file audio_led.c
 * @brief Audio Streaming LED (FR-015) — separate LED indicator from the
 *        zego/bricks/ux-owned LED 0 Wi-Fi/audio-link indicator.
 *
 * Compiled straight into `app`. The `app` target's private include path puts
 * src/modules (which holds the legacy nRF5340 Audio DK led.h) ahead of
 * zego/bricks/led/src, so a plain <led.h> here would resolve to the wrong
 * header. "zego_led_include.h" is a generated wrapper (see CMakeLists.txt)
 * that #includes the brick's header via its absolute path, sidestepping -I
 * search order entirely. See docs/dev-specs/ui-module.md for the full
 * rationale.
 */

#include "zego_led_include.h"
#include <zephyr/zbus/zbus.h>

#include "audio_led.h"

/* idx 6 on nRF5340 Audio DK (idx 0-5 reserved for Wi-Fi state / role
 * indicator); idx 1 on nRF7002DK / nRF54LM20DK (idx 0 = Wi-Fi state).
 */
#if defined(CONFIG_BOARD_NRF5340_AUDIO_DK_NRF5340_CPUAPP)
#define AUDIO_STREAM_LED_IDX 6
#else
#define AUDIO_STREAM_LED_IDX 1
#endif

void audio_led_update(bool streaming, bool usb_active)
{
	struct led_msg cmd = {.led_number = AUDIO_STREAM_LED_IDX};

	if (streaming) {
		cmd.type = LED_COMMAND_BLINK;
	} else if (usb_active) {
		cmd.type = LED_COMMAND_ON;
	} else {
		cmd.type = LED_COMMAND_OFF;
	}

	zbus_chan_pub(&LED_CMD_CHAN, &cmd, K_NO_WAIT);
}
