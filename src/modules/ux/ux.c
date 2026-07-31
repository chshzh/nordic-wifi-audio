/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file ux.c
 * @brief Strong overrides of zego/bricks/ux's __weak gesture hooks.
 *
 * The button gestures, the LED 0 Wi-Fi state machine and the startup banner
 * all live in zego/bricks/ux. Only the long press is overridden here, to
 * cycle STA -> P2P_GO -> P2P_GC -> STA: SoftAP is deliberately left out
 * (retired per refactor plan 0.2 -- P2P_GO covers the zero-infrastructure
 * role), whereas zego/ux's default cycle includes it.
 *
 * Single click (log the current Wi-Fi mode) and double-click (trigger WPS PBC
 * pairing in the P2P modes) keep the zego defaults.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/zbus/zbus.h>

#include <led.h>
#include <ux.h>
#include <wifi.h>

LOG_MODULE_REGISTER(app_ux, LOG_LEVEL_INF);

/* SoftAP deliberately excluded: P2P_GO provides the zero-infrastructure AP. */
static const enum zego_wifi_mode mode_cycle[] = {
	ZEGO_WIFI_MODE_STA,
	ZEGO_WIFI_MODE_P2P_GO,
	ZEGO_WIFI_MODE_P2P_GC,
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
	case ZEGO_WIFI_MODE_P2P_GC:
		return "p2p_gc";
	default:
		return "unknown";
	}
}

void zego_ux_on_long_press(void)
{
	enum zego_wifi_mode cur = zego_wifi_get_mode();
	enum zego_wifi_mode next = ZEGO_WIFI_MODE_STA;

	for (int i = 0; i < ARRAY_SIZE(mode_cycle); i++) {
		if (mode_cycle[i] == cur) {
			next = mode_cycle[(i + 1) % ARRAY_SIZE(mode_cycle)];
			break;
		}
	}

	LOG_INF("Mode cycle: %s -> %s - saving and rebooting", mode_name(cur), mode_name(next));

	uint8_t ack_led =
		(CONFIG_ZEGO_UX_ROTATE_COUNT > 0) ? (uint8_t)CONFIG_ZEGO_UX_ROTATE_FIRST_LED : 0;
	struct led_msg ack = {.type = LED_COMMAND_OFF, .led_number = ack_led};

	zbus_chan_pub(&LED_CMD_CHAN, &ack, K_NO_WAIT);
	k_sleep(K_MSEC(300));

	uint8_t val = (uint8_t)next;
	int ret = settings_save_one("app/zego_wifi_mode", &val, sizeof(val));

	if (ret) {
		LOG_ERR("settings_save_one failed (%d) - mode not saved", ret);
	}

	sys_reboot(SYS_REBOOT_COLD);
}
