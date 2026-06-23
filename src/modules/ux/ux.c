/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file ux.c
 * @brief Application UX module — Button 0 gestures and LED 0 Wi-Fi feedback.
 *
 * Button 0 gesture → action mapping:
 *   SINGLE_CLICK  Print current Wi-Fi mode to UART log.
 *   LONG_PRESS    Cycle Wi-Fi mode STA → P2P_GO → P2P_CLIENT → STA,
 *                 save to NVS via settings, reboot.
 *
 * LED 0 state machine driven by APP_WIFI_STATE_CHAN:
 *   CONNECTING  →  ROTATE  (starts at boot via SYS_INIT)
 *   CONNECTED   →  Solid ON
 *   SOFTAP      →  ROTATE  (AP up, no clients)
 *   ERROR       →  Fast BLINK  (100 ms half-period)
 *
 * Inputs:  BUTTON_CHAN, APP_WIFI_STATE_CHAN
 * Outputs: LED_CMD_CHAN
 *
 * Note: SoftAP is NOT included in the mode cycle for this project
 * (retired per refactor plan §0.2). P2P_GO replaces SoftAP's
 * zero-infrastructure role.
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

#include "button.h"
#include "led.h"
#include "wifi.h"
#include "../messages.h"

LOG_MODULE_REGISTER(app_ux, LOG_LEVEL_INF);

/* ── Wi-Fi mode cycle ──────────────────────────────────────────────────── */

/* SoftAP deliberately excluded: P2P_GO provides zero-infrastructure AP. */
static const enum zego_wifi_mode mode_cycle[] = {
	ZEGO_WIFI_MODE_STA,
	ZEGO_WIFI_MODE_P2P_GO,
	ZEGO_WIFI_MODE_P2P_CLIENT,
};

static const char *mode_name(enum zego_wifi_mode m)
{
	switch (m) {
	case ZEGO_WIFI_MODE_STA:
		return "sta";
	case ZEGO_WIFI_MODE_SOFTAP:
		return "softap";
	case ZEGO_WIFI_MODE_P2P_GO:
		return "p2p_go";
	case ZEGO_WIFI_MODE_P2P_CLIENT:
		return "p2p_client";
	default:
		return "unknown";
	}
}

static void do_mode_cycle(void)
{
	enum zego_wifi_mode cur = zego_wifi_get_mode();
	enum zego_wifi_mode next = ZEGO_WIFI_MODE_STA;

	for (int i = 0; i < ARRAY_SIZE(mode_cycle); i++) {
		if (mode_cycle[i] == cur) {
			next = mode_cycle[(i + 1) % ARRAY_SIZE(mode_cycle)];
			break;
		}
	}

	LOG_INF("Mode cycle: %s → %s — saving and rebooting", mode_name(cur), mode_name(next));

	uint8_t ack_led =
		(CONFIG_APP_UX_ROTATE_COUNT > 0) ? (uint8_t)CONFIG_APP_UX_ROTATE_FIRST_LED : 0;
	struct led_msg ack = {.type = LED_COMMAND_OFF, .led_number = ack_led};

	zbus_chan_pub(&LED_CMD_CHAN, &ack, K_NO_WAIT);
	k_sleep(K_MSEC(300));

	uint8_t val = (uint8_t)next;
	int ret = settings_save_one("app/app_wifi_mode", &val, sizeof(val));

	if (ret) {
		LOG_ERR("settings_save_one failed (%d) — mode not saved", ret);
	}

	sys_reboot(SYS_REBOOT_COLD);
}

/* ── LED helpers ───────────────────────────────────────────────────────── */

static void led_set(enum led_msg_type type, uint16_t period_ms)
{
	struct led_msg msg = {
		.type = type,
		.led_number = 0,
		.period_ms = period_ms,
	};

	zbus_chan_pub(&LED_CMD_CHAN, &msg, K_NO_WAIT);
}

static void led_rotate(void)
{
	struct led_msg msg = {
		.type = LED_COMMAND_ROTATE,
		.period_ms = 0,
	};

#if CONFIG_APP_UX_ROTATE_COUNT > 0
	msg.rotate_count = CONFIG_APP_UX_ROTATE_COUNT;
	for (uint8_t i = 0; i < (uint8_t)CONFIG_APP_UX_ROTATE_COUNT; i++) {
		msg.rotate_indices[i] = (uint8_t)(CONFIG_APP_UX_ROTATE_FIRST_LED + i);
	}
#endif

	zbus_chan_pub(&LED_CMD_CHAN, &msg, K_NO_WAIT);
}

static void led_connected(void)
{
	struct led_msg on = {
		.type = LED_COMMAND_ON,
		.led_number = CONFIG_APP_UX_CONNECTED_LED,
	};

	zbus_chan_pub(&LED_CMD_CHAN, &on, K_NO_WAIT);

	if (IS_ENABLED(CONFIG_APP_UX_CONNECTED_LED_GREEN_ONLY)) {
		if (CONFIG_APP_UX_CONNECTED_LED > 0) {
			struct led_msg off_r = {
				.type = LED_COMMAND_OFF,
				.led_number = CONFIG_APP_UX_CONNECTED_LED - 1,
			};

			zbus_chan_pub(&LED_CMD_CHAN, &off_r, K_NO_WAIT);
		}
		struct led_msg off_b = {
			.type = LED_COMMAND_OFF,
			.led_number = CONFIG_APP_UX_CONNECTED_LED + 1,
		};

		zbus_chan_pub(&LED_CMD_CHAN, &off_b, K_NO_WAIT);
	}
}

static void apply_wifi_state_led(enum app_wifi_state state)
{
	switch (state) {
	case APP_WIFI_STATE_CONNECTING:
		led_rotate();
		break;
	case APP_WIFI_STATE_CONNECTED:
		led_connected();
		break;
	case APP_WIFI_STATE_SOFTAP:
		led_rotate();
		break;
	case APP_WIFI_STATE_ERROR:
		led_set(LED_COMMAND_BLINK, 100);
		break;
	}
}

/* ── Deferred LED work ─────────────────────────────────────────────────── */

static enum app_wifi_state last_wifi_state = APP_WIFI_STATE_CONNECTING;
static enum app_wifi_state pending_wifi_state;
static atomic_t app_ux_ready = ATOMIC_INIT(0);

static void app_ux_led_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);
	if (!atomic_get(&app_ux_ready)) {
		return;
	}
	apply_wifi_state_led(pending_wifi_state);
}

static K_WORK_DEFINE(app_ux_led_work, app_ux_led_work_fn);

/* ── Button 0 listener ─────────────────────────────────────────────────── */

static void btn_listener_cb(const struct zbus_channel *chan)
{
	const struct button_msg *msg = zbus_chan_const_msg(chan);

	if (msg->button_number != 0) {
		return;
	}

	switch (msg->type) {
	case BUTTON_SINGLE_CLICK:
		LOG_INF("Wi-Fi mode: %s", mode_name(zego_wifi_get_mode()));
		break;

	case BUTTON_LONG_PRESS:
		do_mode_cycle();
		break;

	default:
		break;
	}
}

ZBUS_LISTENER_DEFINE(app_btn_listener, btn_listener_cb);
ZBUS_CHAN_ADD_OBS(BUTTON_CHAN, app_btn_listener, 0);

/* ── APP_WIFI_STATE_CHAN listener → LED ────────────────────────────────── */

static void wifi_state_listener_cb(const struct zbus_channel *chan)
{
	const struct app_wifi_state_msg *msg = zbus_chan_const_msg(chan);

	last_wifi_state = msg->state;
	pending_wifi_state = msg->state;
	k_work_submit(&app_ux_led_work);
}

ZBUS_LISTENER_DEFINE(app_wifi_state_listener, wifi_state_listener_cb);
ZBUS_CHAN_ADD_OBS(APP_WIFI_STATE_CHAN, app_wifi_state_listener, 0);

/* ── SYS_INIT ──────────────────────────────────────────────────────────── */

static int app_ux_init(void)
{
	led_rotate();
	atomic_set(&app_ux_ready, 1);

	if (last_wifi_state != APP_WIFI_STATE_CONNECTING) {
		pending_wifi_state = last_wifi_state;
		k_work_submit(&app_ux_led_work);
	}

	return 0;
}

SYS_INIT(app_ux_init, APPLICATION, CONFIG_APP_UX_INIT_PRIORITY);
