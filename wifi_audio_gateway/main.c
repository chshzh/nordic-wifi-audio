/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 * @brief WiFi Transceiver
 */

#define MODULE main

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/sys/byteorder.h>

#include "zbus_common.h"
#include "nrf5340_audio_dk.h"
/* led.h / button_assignments.h retired (Step 3.5) — zego bricks handle GPIO */
#include "button.h"  /* zego button brick: BUTTON_CHAN, struct button_msg (new types) */
#include "macros_common.h"
#include "audio_system.h"
#include "audio_datapath.h"

#include <wifi.h>  /* zego wifi brick: zego_wifi_print_banner() */
#include "streamctrl.h"
#include "socket_utils.h"
#include "wifi_audio_rx.h"

#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#ifdef CONFIG_HEAPS_MONITOR
#include "heaps_monitor.h"
#endif

LOG_MODULE_REGISTER(MODULE, CONFIG_MAIN_LOG_LEVEL);

extern volatile bool socket_connected_signall;

ZBUS_SUBSCRIBER_DEFINE(button_evt_sub, CONFIG_BUTTON_MSG_SUB_QUEUE_SIZE);

ZBUS_MSG_SUBSCRIBER_DEFINE(le_audio_evt_sub);

/* button_chan retired (Step 3.5) — use BUTTON_CHAN from zego/button brick */
ZBUS_CHAN_DECLARE(le_audio_chan);

ZBUS_OBS_DECLARE(sdu_ref_msg_listen);

static struct k_thread button_msg_sub_thread_data;
static struct k_thread le_audio_msg_sub_thread_data;

static k_tid_t button_msg_sub_thread_id;
static k_tid_t le_audio_msg_sub_thread_id;

K_THREAD_STACK_DEFINE(button_msg_sub_thread_stack, CONFIG_BUTTON_MSG_SUB_STACK_SIZE);
K_THREAD_STACK_DEFINE(le_audio_msg_sub_thread_stack, CONFIG_LE_AUDIO_MSG_SUB_STACK_SIZE);

static enum stream_state strm_state = STATE_PAUSED;

/* Function for handling all stream state changes */
static void stream_state_set(enum stream_state stream_state_new)
{
	strm_state = stream_state_new;
}

uint8_t stream_state_get(void)
{
	return strm_state;
}

#if defined(CONFIG_SOCKET_ROLE_SERVER)
void streamctrl_handle_client_disconnect(void)
{
	if (strm_state != STATE_STREAMING) {
		return;
	}

	LOG_INF("Station disconnected, pausing audio stream");
	audio_system_encoder_stop();
	stream_state_set(STATE_PAUSED);
}
#endif

void socket_rx_handler(uint8_t *socket_rx_buf, size_t len)
{
	if (len < 5) {
		// Not enough data for start and end sequence
		LOG_INF("Received buffer too short\n");
		return;
	}

	// Check if it starts with 0xFF 0xAA
	if (socket_rx_buf[0] == START_SEQUENCE_1 && socket_rx_buf[1] == START_SEQUENCE_2 &&
	    socket_rx_buf[2] == SEND_CMD_SIGN) {
		// Check if it ends with 0xFF 0xBB
		if (socket_rx_buf[len - 2] == END_SEQUENCE_1 &&
		    socket_rx_buf[len - 1] == END_SEQUENCE_2) {
			uint8_t command = socket_rx_buf[3]; // Command byte (third byte)

			switch (command) {
			case AUDIO_START_CMD:
				LOG_INF("STATE_STREAMING Command received");
				stream_state_set(STATE_STREAMING);
				audio_system_encoder_start();
				break;
			case AUDIO_STOP_CMD:
				LOG_INF("STATE_PAUSED Command received");
				audio_system_encoder_stop();
				stream_state_set(STATE_PAUSED);
				break;
			default:
				LOG_INF("Unknown command received: 0x%02X\n", command);
				break;
			}
		} else {
			LOG_INF("Invalid end sequence\n");
		}
	} else {
		LOG_INF("Invalid start sequence\n");
	}
}

void streamctrl_send(void const *const data, size_t size)
{
	if ((strm_state == STATE_STREAMING) && socket_connected_signall) {
		socket_utils_tx_data((uint8_t *)data, size);
	}
}

/*
 * Button number assignments (zego BUTTON_CHAN, 0-based index):
 *   0 = sw0: mode print/cycle (handled by ux.c)
 *   1 = sw1: VOL_UP (unused in gateway)
 *   2 = sw2: PLAY_PAUSE
 *   3 = sw3: BTN4 / test tone
 */
#define APP_BTN_PLAY_PAUSE 2
#define APP_BTN_TEST_TONE  3

/**
 * @brief	Handle audio button activity (BUTTON_CHAN from zego/button brick).
 */
static void button_msg_sub_thread(void)
{
	int ret;
	const struct zbus_channel *chan;

	while (1) {
		ret = zbus_sub_wait(&button_evt_sub, &chan, K_FOREVER);
		ERR_CHK(ret);

		struct button_msg msg;

		ret = zbus_chan_read(chan, &msg, ZBUS_READ_TIMEOUT_MS);
		ERR_CHK(ret);

		/* Only handle single-click gestures; long-press handled by ux.c */
		if (msg.type != BUTTON_SINGLE_CLICK) {
			continue;
		}

		LOG_DBG("Button %d single-click", msg.button_number);

		switch (msg.button_number) {
		case APP_BTN_PLAY_PAUSE:
			if (socket_connected_signall == true) {
				if (strm_state == STATE_STREAMING) {
					audio_system_encoder_stop();
					LOG_INF("STATE_PAUSED");
					stream_state_set(STATE_PAUSED);

				} else if (strm_state == STATE_PAUSED) {
					LOG_INF("STATE_STREAMING");
					stream_state_set(STATE_STREAMING);
					audio_system_encoder_start();

				} else {
					LOG_WRN("In invalid state: %d", strm_state);
				}
			} else {
				LOG_WRN("Please wait for socket client to connect.");
			}
			break;
		case APP_BTN_TEST_TONE:
			if (IS_ENABLED(CONFIG_AUDIO_TEST_TONE)) {
				if (strm_state != STATE_STREAMING) {
					LOG_WRN("Not in streaming state");
					break;
				}

				ret = audio_system_encode_test_tone_step();
				if (ret) {
					LOG_WRN("Failed to play test tone, ret: %d", ret);
				}

				break;
			}
			break;

		default:
			/* Button 0 handled by ux.c; others ignored */
			break;
		}

		STACK_USAGE_PRINT("button_msg_thread", &button_msg_sub_thread_data);
	}
}

/**
 * @brief	Handle Bluetooth LE audio events.
 */
static void le_audio_msg_sub_thread(void)
{
	int ret;
	const struct zbus_channel *chan;

	while (1) {
		struct le_audio_msg msg;

		ret = zbus_sub_wait_msg(&le_audio_evt_sub, &chan, &msg, K_FOREVER);
		ERR_CHK(ret);

		LOG_DBG("Received event = %d, current state = %d", msg.event, strm_state);

		switch (msg.event) {
		case LE_AUDIO_EVT_STREAMING:
			LOG_DBG("LE audio evt streaming");

			audio_system_encoder_start();

			if (strm_state == STATE_STREAMING) {
				LOG_DBG("Got streaming event in streaming state");
				break;
			}

			audio_system_start();
			stream_state_set(STATE_STREAMING);

			break;

		case LE_AUDIO_EVT_NOT_STREAMING:
			LOG_DBG("LE audio evt not_streaming");

			audio_system_encoder_stop();

			if (strm_state == STATE_PAUSED) {
				LOG_DBG("Got not_streaming event in paused state");
				break;
			}

			stream_state_set(STATE_PAUSED);
			audio_system_stop();

			break;

		default:
			LOG_WRN("Unexpected/unhandled le_audio event: %d", msg.event);

			break;
		}

		STACK_USAGE_PRINT("le_audio_msg_thread", &le_audio_msg_sub_thread_data);
	}
}
/**
 * @brief	Create zbus subscriber threads.
 *
 * @return	0 for success, error otherwise.
 */
static int zbus_subscribers_create(void)
{
	int ret;

	button_msg_sub_thread_id = k_thread_create(
		&button_msg_sub_thread_data, button_msg_sub_thread_stack,
		CONFIG_BUTTON_MSG_SUB_STACK_SIZE, (k_thread_entry_t)button_msg_sub_thread, NULL,
		NULL, NULL, K_PRIO_PREEMPT(CONFIG_BUTTON_MSG_SUB_THREAD_PRIO), 0, K_NO_WAIT);
	ret = k_thread_name_set(button_msg_sub_thread_id, "BUTTON_MSG_SUB");
	if (ret) {
		LOG_ERR("Failed to create button_msg thread");
		return ret;
	}

	le_audio_msg_sub_thread_id = k_thread_create(
		&le_audio_msg_sub_thread_data, le_audio_msg_sub_thread_stack,
		CONFIG_LE_AUDIO_MSG_SUB_STACK_SIZE, (k_thread_entry_t)le_audio_msg_sub_thread, NULL,
		NULL, NULL, K_PRIO_PREEMPT(CONFIG_LE_AUDIO_MSG_SUB_THREAD_PRIO), 0, K_NO_WAIT);
	ret = k_thread_name_set(le_audio_msg_sub_thread_id, "LE_AUDIO_MSG_SUB");
	if (ret) {
		LOG_ERR("Failed to create le_audio_msg thread");
		return ret;
	}

	// ret = zbus_chan_add_obs(&sdu_ref_chan, &sdu_ref_msg_listen,
	// ZBUS_ADD_OBS_TIMEOUT_MS); if (ret) { 	LOG_ERR("Failed to add timestamp listener");
	// 	return ret;
	// }

	return 0;
}

/**
 * @brief	Link zbus producers and observers.
 *
 * @return	0 for success, error otherwise.
 */
static int zbus_link_producers_observers(void)
{
	int ret;

	if (!IS_ENABLED(CONFIG_ZBUS)) {
		return -ENOTSUP;
	}

	/* BUTTON_CHAN from zego/button brick — ux.c handles btn 0; audio handles btns 2,3 */
	ret = zbus_chan_add_obs(&BUTTON_CHAN, &button_evt_sub, ZBUS_ADD_OBS_TIMEOUT_MS);
	if (ret) {
		LOG_ERR("Failed to add button sub");
		return ret;
	}

	// ret = zbus_chan_add_obs(&le_audio_chan, &le_audio_evt_sub,
	// ZBUS_ADD_OBS_TIMEOUT_MS); if (ret) { 	LOG_ERR("Failed to add le_audio sub");
	// 	return ret;
	// }

	// ret = zbus_chan_add_obs(&bt_mgmt_chan, &bt_mgmt_evt_listen,
	// ZBUS_ADD_OBS_TIMEOUT_MS); if (ret) { 	LOG_ERR("Failed to add bt_mgmt listener");
	// 	return ret;
	// }

	return 0;
}

K_THREAD_STACK_DEFINE(socket_utils_thread_stack, CONFIG_SOCKET_STACK_SIZE);
static struct k_thread socket_utils_thread_data;
static k_tid_t socket_utils_thread_id;

int socket_utils_init(void)
{
	int ret;
	/* Start thread to handle events from socket connection */
	socket_utils_thread_id = k_thread_create(
		&socket_utils_thread_data, socket_utils_thread_stack, CONFIG_SOCKET_STACK_SIZE,
		(k_thread_entry_t)socket_utils_thread, NULL, NULL, NULL,
		K_PRIO_PREEMPT(CONFIG_SOCKET_UTILS_THREAD_PRIO), 0, K_NO_WAIT);
	ret = k_thread_name_set(socket_utils_thread_id, "SOCKET");
	socket_utils_set_rx_callback(socket_rx_handler);
	return ret;
}

int main(void)
{
	int ret;
	LOG_INF("WiFi Audio Transceiver Start!");

#ifdef CONFIG_HEAPS_MONITOR
	/* Initialize heap monitoring system */
	ret = heaps_monitor_init();
	if (ret) {
		LOG_WRN("Failed to initialize heap monitoring: %d", ret);
	}
#endif /* CONFIG_HEAPS_MONITOR */

	ret = nrf5340_audio_dk_init();
	ERR_CHK(ret);

	zego_wifi_print_banner();

	LOG_INF("socket_utils_init");
	ret = socket_utils_init();
	ERR_CHK(ret);

	/* Network LED now driven by ux.c via APP_WIFI_STATE_CHAN (ROTATE = connecting) */
	LOG_INF("audio_system_init");
	ret = audio_system_init();
	ERR_CHK(ret);

	ret = audio_system_config_set(48000, 320000, 0);
	ERR_CHK_MSG(ret, "Failed to set sample and bitrate for encoder");

	audio_system_start();

	LOG_INF("zbus_subscribers_create");
	ret = zbus_subscribers_create();
	ERR_CHK_MSG(ret, "Failed to create zbus subscriber threads");

	LOG_INF("zbus_link_producers_observers");
	ret = zbus_link_producers_observers();
	ERR_CHK_MSG(ret, "Failed to link zbus producers and observers");

	return 0;
}
