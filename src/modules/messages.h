/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file messages.h
 * @brief Application-level Zbus channel and message type definitions.
 *
 * Define all app-level channels here so every module sees the same
 * declarations.  Each channel must be DEFINED (ZBUS_CHAN_DEFINE) in exactly
 * one .c file.
 */

#ifndef APP_MESSAGES_H
#define APP_MESSAGES_H

#include <zephyr/zbus/zbus.h>
#include <wifi.h> /* enum zego_wifi_mode */

/* ==========================================================================
 * APP_WIFI_STATE_CHAN — Wi-Fi connectivity state
 *
 * Publisher:  net_event_app.c (defined there via ZBUS_CHAN_DEFINE)
 * Subscriber: app_ux module (LED 0 feedback)
 * ==========================================================================
 */

/** @brief Application-level Wi-Fi state used to drive LED 0. */
enum app_wifi_state {
	/** Boot / connecting — ROTATE on LED 0. */
	APP_WIFI_STATE_CONNECTING = 0,
	/** STA or P2P link established (IP assigned / peer joined). */
	APP_WIFI_STATE_CONNECTED,
	/** SoftAP active — first client connected (kept for LED compat). */
	APP_WIFI_STATE_SOFTAP,
	/** Link lost or fatal error. */
	APP_WIFI_STATE_ERROR,
};

/** @brief Message published on APP_WIFI_STATE_CHAN. */
struct app_wifi_state_msg {
	enum app_wifi_state state;
	enum zego_wifi_mode mode;
};

ZBUS_CHAN_DECLARE(APP_WIFI_STATE_CHAN);

#endif /* APP_MESSAGES_H */
