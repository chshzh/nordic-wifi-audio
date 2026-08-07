/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef _SOCKET_UTILS_H_
#define _SOCKET_UTILS_H_

#include <zephyr/kernel.h>
#include <zephyr/net/net_ip.h>

/*Wi-Fi default MTU*/
#define BUFFER_MAX_SIZE 1500

typedef struct {
	uint8_t buf[BUFFER_MAX_SIZE];
	size_t len;
} socket_receive_t;

extern struct k_msgq socket_recv_queue;

typedef void (*net_util_socket_rx_callback_t)(uint8_t *data, size_t len);

void socket_utils_set_rx_callback(net_util_socket_rx_callback_t socket_rx_callback);
int socket_utils_tx_data(uint8_t *data, size_t length);
void socket_utils_thread(void);

/* Server: forget the peer learned from its first datagram (call when the client
 * leaves). Client: forget the discovered server. */
void socket_utils_clear_target(void);

#if defined(CONFIG_SOCKET_ROLE_CLIENT)
bool socket_utils_is_target_set(void);
void socket_utils_set_target_ipv4(const struct in_addr *addr);

typedef void (*socket_utils_target_ready_cb_t)(void);
void socket_utils_set_target_ready_callback(socket_utils_target_ready_cb_t cb);
void socket_utils_signal_dhcp_bound(void);
#endif

#endif
