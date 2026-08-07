/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file net_event_app.c
 * @brief Application-side Wi-Fi event hooks.
 *
 * Strong overrides of the zego/network brick's weak callbacks.
 * Each hook drives audio start/stop and publishes ZEGO_UX_WIFI_STATE_CHAN
 * (consumed by zego/bricks/ux to drive the LED 0 state machine).
 *
 * Peer address resolution (mode-branched):
 *   STA:        headset discovers gateway via mDNS (existing socket_utils path)
 *   P2P_GC: dhcp_bound hook sets fixed GO IP 192.168.7.1 directly
 *   P2P_GO:     gateway binds INADDR_ANY; headset's REQ_PLAY_CMD triggers encode
 */

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>
#include <zephyr/posix/arpa/inet.h>
#if defined(CONFIG_SOCKET_ROLE_SERVER)
#include <string.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#endif

#include <ux.h> /* zego ux brick: ZEGO_UX_WIFI_STATE_CHAN */

#include "audio_system.h"
#include "socket_utils.h"
#include "streamctrl.h"
#include "net_event_app.h"

LOG_MODULE_REGISTER(net_event_app, LOG_LEVEL_INF);

/* ── Internal helper ─────────────────────────────────────────────────────── */

static void pub_wifi_state(enum zego_ux_wifi_state state, enum zego_wifi_mode mode)
{
	struct zego_ux_wifi_state_msg msg = {.state = state, .mode = mode};
	int err = zbus_chan_pub(&ZEGO_UX_WIFI_STATE_CHAN, &msg, K_MSEC(10));

	if (err) {
		LOG_WRN("Failed to publish ZEGO_UX_WIFI_STATE_CHAN: %d", err);
	}
}

/* ── Client liveness eviction (P2P_GO / AP) ─────────────────────────────────
 * The nRF70 AP driver's own station-inactivity accounting does not reliably
 * reset on real client traffic (hardware-confirmed - see
 * zego/patches/hostap/README.md), so a client that goes silent isn't purged
 * for up to CONFIG_WIFI_NM_WPA_SUPPLICANT P2P_GO_MAX_INACTIVITY (default
 * 300 s). The headset's stream watchdog calls net_event_app_client_seen() by
 * sending a command frame every 5 s UNCONDITIONALLY, including while
 * streaming - that uplink is the only proof the gateway has that the client
 * still exists, since a power-cut peer sends no deauth and nothing local to
 * the gateway (socket_connected_signall, strm_state) can tell it apart from a
 * healthy one.
 * On timeout the station is force-disconnected via
 * NET_REQUEST_WIFI_AP_STA_DISCONNECT, which drives the normal
 * AP_STA_DISCONNECTED flow (audio stop, WPS PBC re-arm) in seconds instead of
 * minutes. */
#if defined(CONFIG_SOCKET_ROLE_SERVER)
#define CLIENT_LIVENESS_TIMEOUT_SEC 15

static uint8_t client_mac[WIFI_MAC_ADDR_LEN];
static bool have_client_mac;
static struct k_work_delayable client_liveness_work;
static struct net_mgmt_event_callback ap_sta_mac_cb;

static void client_liveness_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (!have_client_mac) {
		return;
	}

	struct net_if *iface = net_if_get_first_wifi();

	LOG_WRN("No client command in %d s - force-disconnecting stale station",
		CLIENT_LIVENESS_TIMEOUT_SEC);

	if (!iface || net_mgmt(NET_REQUEST_WIFI_AP_STA_DISCONNECT, iface, client_mac,
			       sizeof(client_mac))) {
		LOG_WRN("Failed to request station disconnect");
	}
}

void net_event_app_client_seen(void)
{
	if (!have_client_mac) {
		return;
	}
	k_work_reschedule(&client_liveness_work, K_SECONDS(CLIENT_LIVENESS_TIMEOUT_SEC));
}

static void ap_sta_mac_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
				     struct net_if *iface)
{
	ARG_UNUSED(iface);

	const struct wifi_ap_sta_info *sta = (const struct wifi_ap_sta_info *)cb->info;

	if (mgmt_event == NET_EVENT_WIFI_AP_STA_CONNECTED) {
		memcpy(client_mac, sta->mac, sizeof(client_mac));
		have_client_mac = true;
		net_event_app_client_seen();
	} else if (mgmt_event == NET_EVENT_WIFI_AP_STA_DISCONNECTED) {
		have_client_mac = false;
		k_work_cancel_delayable(&client_liveness_work);
	}
}

void net_event_app_init(void)
{
	k_work_init_delayable(&client_liveness_work, client_liveness_handler);
	net_mgmt_init_event_callback(&ap_sta_mac_cb, ap_sta_mac_event_handler,
				     NET_EVENT_WIFI_AP_STA_CONNECTED |
					     NET_EVENT_WIFI_AP_STA_DISCONNECTED);
	net_mgmt_add_event_callback(&ap_sta_mac_cb);
}
#endif /* CONFIG_SOCKET_ROLE_SERVER */

/* ── Weak hook overrides ─────────────────────────────────────────────────── */

// void zego_on_net_event_wifi_connect(enum zego_wifi_mode mode)
// {
// 	LOG_INF("L2 connected (mode=%d) - waiting for IP", (int)mode);
// }

/**
 * Fired when:
 *   STA:         DHCP lease assigned (ip_addr = assigned IP)
 *   P2P_GC:      Static IP 192.168.7.2 ready (ip_addr = "192.168.7.2")
 *   P2P_GO:      First AP client joined (ip_addr = "192.168.7.1")
 *
 * For P2P_GC: sets the fixed GO IP as socket target so the headset
 * can send REQ_PLAY_CMD to the gateway and start the audio loop.
 * For other modes: audio starts via the REQ_PLAY_CMD round-trip (server
 * waits for headset's command; headset triggers via socket_target_ready_handler).
 */
void zego_on_net_event_dhcp_bound(enum zego_wifi_mode mode, const char *ip_addr,
				  const char *mac_addr, const char *ssid)
{
	LOG_INF("Network ready (mode=%d ip=%s ssid=%s)", (int)mode, ip_addr, ssid);

	pub_wifi_state(ZEGO_UX_WIFI_STATE_CONNECTED, mode);

#if defined(CONFIG_SOCKET_ROLE_CLIENT)
	socket_utils_signal_dhcp_bound();

	if (mode == ZEGO_WIFI_MODE_P2P_GC) {
		/* P2P: no mDNS on the P2P link - use fixed GO IP directly. */
		struct in_addr go_addr;

		if (zsock_inet_pton(AF_INET, "192.168.7.1", &go_addr) == 1) {
			socket_utils_set_target_ipv4(&go_addr);
			LOG_INF("P2P_GC: GO target set to 192.168.7.1");
		} else {
			LOG_ERR("P2P_GC: failed to parse GO IP");
		}
		/* socket_utils_set_target_ipv4 triggers socket_target_ready_handler
		 * → send_audio_command(REQ_PLAY_CMD) → gateway starts encoding. */
	}
	/* STA mode: mDNS discovery in socket_utils_thread sets the target. */
#endif
}

/*
 * will_retry is ignored on purpose: any disconnect tears the audio stream down,
 * so the LED shows the ERROR fast-blink regardless of whether the Wi-Fi stack
 * will keep retrying on its own.
 */
void zego_on_net_event_wifi_disconnect(bool will_retry)
{
	ARG_UNUSED(will_retry);

	LOG_INF("Wi-Fi disconnected - stopping audio");

	audio_system_encoder_stop();

#if defined(CONFIG_SOCKET_ROLE_CLIENT)
	socket_utils_clear_target();
#endif

	pub_wifi_state(ZEGO_UX_WIFI_STATE_ERROR, ZEGO_WIFI_MODE_STA);
}

void zego_on_net_event_wifi_ap_enabled(enum zego_wifi_mode mode, const char *ip_addr,
				       const char *ssid)
{
	ARG_UNUSED(mode);
	ARG_UNUSED(ip_addr);
	ARG_UNUSED(ssid);
	LOG_INF("AP/P2P_GO enabled, waiting for client");
}

/* zego_on_net_event_wifi_ap_sta_connected() is NOT overridden here - zego/network's
 * own __weak default already publishes ZEGO_UX_WIFI_STATE_CONNECTED with the
 * correct active mode (fixed to use active_mode instead of a hardcoded SOFTAP
 * value), so LED 0 leaves ROTATE on the P2P_GO/SoftAP side without app help.
 * Audio start itself is unaffected either way, since it's driven by the
 * headset's REQ_PLAY_CMD over the socket. */

void zego_on_net_event_wifi_ap_sta_disconnected(int station_count)
{
	LOG_INF("AP/P2P_GO client left (remaining=%d)", station_count);

	if (station_count == 0) {
		LOG_INF("Last client disconnected - stopping audio");
		audio_system_encoder_stop();

#if defined(CONFIG_SOCKET_ROLE_SERVER)
		/* Must precede streamctrl_handle_client_disconnect(): that re-evaluates
		 * the stream against socket_connected_signall, so leaving it set here
		 * would keep strm_state at STREAMING with the encoder already stopped,
		 * and the next REQ_PLAY_CMD would be a no-op.
		 */
		socket_utils_clear_target();
		streamctrl_handle_client_disconnect();
#endif

		pub_wifi_state(ZEGO_UX_WIFI_STATE_ERROR, ZEGO_WIFI_MODE_P2P_GO);
	}
}
