/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _AUDIO_LED_H_
#define _AUDIO_LED_H_

#include <stdbool.h>

/**
 * @brief Drive the Audio Streaming LED (FR-015).
 *
 * Priority: @p streaming always wins (BLINK). Otherwise the LED reflects
 * @p usb_active (Solid ON) or not (Solid OFF). Headset callers have no
 * USB-availability concept and should always pass usb_active = false.
 *
 * @param streaming   true if audio is actively streaming to/from a peer.
 * @param usb_active  Gateway only: true if USB host audio is available.
 */
void audio_led_update(bool streaming, bool usb_active);

/**
 * @brief Set the RGB1 role indicator: solid green (gateway) or blue (headset),
 *        set once at boot. No-op on boards without RGB1 (nRF7002DK, nRF54LM20DK).
 *
 * @param is_gateway true for the gateway role, false for headset.
 */
void role_led_init(bool is_gateway);

#endif /* _AUDIO_LED_H_ */
