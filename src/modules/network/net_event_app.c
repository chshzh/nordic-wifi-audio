/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file net_event_app.c
 * @brief Application-side Wi-Fi event hooks.
 *
 * Strong overrides of the zego/network brick's weak callbacks.
 * Each hook drives audio start/stop and publishes APP_WIFI_STATE_CHAN.
 *
 * Peer address resolution (mode-branched):
 *   STA:        headset discovers gateway via mDNS (existing socket_utils path)
 *   P2P_CLIENT: dhcp_bound hook sets fixed GO IP 192.168.7.1 directly
 *   P2P_GO:     gateway binds INADDR_ANY; headset's AUDIO_START_CMD triggers encode
 */

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>
#include <zephyr/posix/arpa/inet.h>

#include "messages.h"
#include "audio_system.h"
#include "socket_utils.h"
#include "streamctrl.h"

LOG_MODULE_REGISTER(net_event_app, LOG_LEVEL_INF);

/* APP_WIFI_STATE_CHAN is owned by this file. */
ZBUS_CHAN_DEFINE(APP_WIFI_STATE_CHAN, struct app_wifi_state_msg, NULL, NULL,
		 ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(.state = APP_WIFI_STATE_CONNECTING,
			       .mode = ZEGO_WIFI_MODE_P2P_GO));

/* ── Internal helper ─────────────────────────────────────────────────────── */

static void pub_wifi_state(enum app_wifi_state state, enum zego_wifi_mode mode)
{
	struct app_wifi_state_msg msg = {.state = state, .mode = mode};
	int err = zbus_chan_pub(&APP_WIFI_STATE_CHAN, &msg, K_MSEC(10));

	if (err) {
		LOG_WRN("Failed to publish APP_WIFI_STATE_CHAN: %d", err);
	}
}

/* ── Weak hook overrides ─────────────────────────────────────────────────── */

void zego_on_net_event_wifi_connect(enum zego_wifi_mode mode)
{
	LOG_INF("L2 connected (mode=%d) — waiting for IP", (int)mode);
}

/**
 * Fired when:
 *   STA:         DHCP lease assigned (ip_addr = assigned IP)
 *   P2P_CLIENT:  Static IP 192.168.7.2 ready (ip_addr = "192.168.7.2")
 *   P2P_GO:      First AP client joined (ip_addr = "192.168.7.1")
 *
 * For P2P_CLIENT: sets the fixed GO IP as socket target so the headset
 * can send AUDIO_START_CMD to the gateway and start the audio loop.
 * For other modes: audio starts via the AUDIO_START_CMD round-trip (server
 * waits for headset's command; headset triggers via socket_target_ready_handler).
 */
void zego_on_net_event_dhcp_bound(enum zego_wifi_mode mode, const char *ip_addr,
				  const char *mac_addr, const char *ssid)
{
	LOG_INF("Network ready (mode=%d ip=%s ssid=%s)", (int)mode, ip_addr, ssid);

	pub_wifi_state(APP_WIFI_STATE_CONNECTED, mode);

#if defined(CONFIG_SOCKET_ROLE_CLIENT)
	socket_utils_signal_dhcp_bound();

	if (mode == ZEGO_WIFI_MODE_P2P_CLIENT) {
		/* P2P: no mDNS on the P2P link — use fixed GO IP directly. */
		struct in_addr go_addr;

		if (zsock_inet_pton(AF_INET, "192.168.7.1", &go_addr) == 1) {
			socket_utils_set_target_ipv4(&go_addr);
			LOG_INF("P2P_CLIENT: GO target set to 192.168.7.1");
		} else {
			LOG_ERR("P2P_CLIENT: failed to parse GO IP");
		}
		/* socket_utils_set_target_ipv4 triggers socket_target_ready_handler
		 * → send_audio_command(AUDIO_START_CMD) → gateway starts encoding. */
	}
	/* STA mode: mDNS discovery in socket_utils_thread sets the target. */
#endif
}

void zego_on_net_event_wifi_disconnect(void)
{
	LOG_INF("Wi-Fi disconnected — stopping audio");

	audio_system_encoder_stop();

#if defined(CONFIG_SOCKET_ROLE_CLIENT)
	socket_utils_clear_target();
#endif

	pub_wifi_state(APP_WIFI_STATE_ERROR, ZEGO_WIFI_MODE_STA);
}

void zego_on_net_event_wifi_ap_enabled(enum zego_wifi_mode mode, const char *ip_addr,
				       const char *ssid)
{
	ARG_UNUSED(mode);
	ARG_UNUSED(ip_addr);
	ARG_UNUSED(ssid);
	LOG_INF("AP/P2P_GO enabled — waiting for client");
}

void zego_on_net_event_wifi_ap_sta_connected(int sta_count)
{
	LOG_INF("AP/P2P_GO client joined (count=%d)", sta_count);
	/* dhcp_bound fires for the first client via zego/network brick and
	 * handles audio start via the AUDIO_START_CMD path. */
}

void zego_on_net_event_wifi_ap_sta_disconnected(int station_count)
{
	LOG_INF("AP/P2P_GO client left (remaining=%d)", station_count);

	if (station_count == 0) {
		LOG_INF("Last client disconnected — stopping audio");
		audio_system_encoder_stop();

#if defined(CONFIG_SOCKET_ROLE_SERVER)
		streamctrl_handle_client_disconnect();
#endif

		pub_wifi_state(APP_WIFI_STATE_ERROR, ZEGO_WIFI_MODE_P2P_GO);
	}
}
