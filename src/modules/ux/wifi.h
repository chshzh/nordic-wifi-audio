/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
 * Transition proxy — found first by #include "wifi.h" in src/modules/ux/.
 * Provides the zego wifi brick API.
 *
 * Content copied from zego/bricks/wifi/src/wifi.h.
 */

#ifndef ZEGO_WIFI_H_
#define ZEGO_WIFI_H_

#include <zephyr/zbus/zbus.h>

enum zego_wifi_mode {
	ZEGO_WIFI_MODE_STA        = 0,
	ZEGO_WIFI_MODE_SOFTAP     = 1,
	ZEGO_WIFI_MODE_P2P_GO     = 2,
	ZEGO_WIFI_MODE_P2P_CLIENT = 3,
};

struct wifi_mode_msg {
	enum zego_wifi_mode mode;
};

ZBUS_CHAN_DECLARE(WIFI_MODE_CHAN);

enum zego_wifi_mode zego_wifi_get_mode(void);

#endif /* ZEGO_WIFI_H_ */
