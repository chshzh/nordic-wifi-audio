/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _NET_EVENT_APP_H_
#define _NET_EVENT_APP_H_

#if defined(CONFIG_SOCKET_ROLE_SERVER)
/**
 * @brief Register this app's P2P/AP station tracking (MAC capture + liveness
 *        eviction). Call once at boot, before any client can connect.
 */
void net_event_app_init(void);

/**
 * @brief Notify that a command frame was received from the connected client.
 *
 * Resets the client liveness timer. If nothing is received for
 * CLIENT_LIVENESS_TIMEOUT_SEC, the station is force-disconnected rather than
 * waiting on the P2P_GO's own (unreliable, see zego/patches/hostap/README.md)
 * inactivity timeout.
 */
void net_event_app_client_seen(void);
#endif /* CONFIG_SOCKET_ROLE_SERVER */

#endif /* _NET_EVENT_APP_H_ */
