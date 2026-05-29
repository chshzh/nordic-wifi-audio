/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
LOG_MODULE_REGISTER(socket_utils, CONFIG_SOCKET_UTILS_LOG_LEVEL);

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/shell/shell.h>
#include <zephyr/net/dns_sd.h>
#include "socket_utils.h"
#include "net_event_mgmt.h"
#include "wifi_utils.h"

#include <zephyr/net/dns_resolve.h>

/* size of stack area used by each thread */
#define STACKSIZE 8192
/* scheduling priority used by each thread */
#define PRIORITY  3

// #define pc_port  60000
#define socket_port 60010 // UDP audio transport port

#define DNS_SD_SERVICE_TYPE         "_nrfwifiaudio"
#define DNS_SD_SERVICE_PROTO        "_udp"
#define DNS_SD_SERVICE_DOMAIN       "local"
#define DNS_SD_SERVICE_NAME         DNS_SD_SERVICE_TYPE "." DNS_SD_SERVICE_PROTO "." DNS_SD_SERVICE_DOMAIN
#define DNS_SD_DISCOVERY_TIMEOUT_MS 3000

#if defined(CONFIG_DNS_SD) && defined(CONFIG_NET_HOSTNAME)
static const char audio_service_txt[] = "\x0A"
					"codec=opus"
					"\x0C"
					"rate=320kbps"
					"\x0A"
					"channels=2"
					"\x0B"
					"latency=low";

DNS_SD_REGISTER_UDP_SERVICE(audio_service, CONFIG_NET_HOSTNAME, DNS_SD_SERVICE_TYPE,
			    DNS_SD_SERVICE_DOMAIN, audio_service_txt, socket_port);
#endif

/**********External Resources START**************/
#if IS_ENABLED(CONFIG_WIFI_NM_WPA_SUPPLICANT_AP)
extern struct k_sem station_connected_sem;
#endif
/**********External Resources END**************/
static int udp_socket;
struct sockaddr_in self_addr;
char target_addr_str[32];
struct sockaddr_in target_addr;
socklen_t target_addr_len = sizeof(target_addr);

static socket_receive_t socket_receive;

K_MSGQ_DEFINE(socket_recv_queue, sizeof(socket_receive), 1, 4);

static net_util_socket_rx_callback_t socket_rx_cb;

#if defined(CONFIG_SOCKET_ROLE_CLIENT) && defined(CONFIG_DNS_SD) && defined(CONFIG_DNS_RESOLVER)
struct dnssd_discovery_ctx {
	struct in_addr addr;
	char host[DNS_MAX_NAME_SIZE + 1];
	char instance[DNS_MAX_NAME_SIZE + 1];
	uint16_t port;
	uint16_t srv_dns_id;
	bool srv_received;
	bool addr_received;
	int status;
	struct k_sem done;
};

static void dnssd_service_cb(enum dns_resolve_status status, struct dns_addrinfo *info,
			     void *user_data)
{
	struct dnssd_discovery_ctx *ctx = user_data;

	LOG_DBG("Callback: status=%d, ai_family=%d, ai_extension=%d, ai_addrlen=%d", status,
		info->ai_family, info->ai_extension, info->ai_addrlen);

	if (status == DNS_EAI_INPROGRESS) {
		if (info->ai_extension == DNS_RESOLVE_SRV) {
			size_t len = MIN(info->ai_srv.targetlen, sizeof(ctx->host) - 1);

			memcpy(ctx->host, info->ai_srv.target, len);
			ctx->host[len] = '\0';
			ctx->port = info->ai_srv.port;
			ctx->srv_received = true;
			LOG_INF("SRV record: host=%s, port=%u", ctx->host, ctx->port);
			k_sem_give(&ctx->done);
		} else if (info->ai_extension == DNS_RESOLVE_TXT) {
			LOG_INF("DNS-SD TXT: %s", info->ai_txt.text);
		} else if (info->ai_family == AF_INET) {
			/* A record from additional records section */
			ctx->addr = net_sin(&info->ai_addr)->sin_addr;
			ctx->addr_received = true;
			LOG_INF("A record: %d.%d.%d.%d", ctx->addr.s4_addr[0], ctx->addr.s4_addr[1],
				ctx->addr.s4_addr[2], ctx->addr.s4_addr[3]);
			k_sem_give(&ctx->done);
		} else if (info->ai_family == AF_LOCAL && info->ai_addrlen > 0) {
			size_t len = MIN(info->ai_addrlen, sizeof(ctx->instance) - 1);

			memcpy(ctx->instance, info->ai_canonname, len);
			ctx->instance[len] = '\0';
			LOG_INF("Discovered service instance %s", ctx->instance);
			k_sem_give(&ctx->done);
		} else {

			LOG_WRN("Unexpected record: family=%d, extension=%d", info->ai_family,
				info->ai_extension);
		}
		return;
	}

	ctx->status = status;
	k_sem_give(&ctx->done);
}

static int dns_sd_discover_gateway(void)
{
	struct dnssd_discovery_ctx ctx = {
		.port = socket_port,
	};
	int err;
	int retries;
	char hostname[DNS_MAX_NAME_SIZE + 1];

	k_sem_init(&ctx.done, 0, 1);

	/* Step 1: Query PTR to discover service instances */
	ctx.status = 0;
	err = dns_resolve_service(dns_resolve_get_default(), DNS_SD_SERVICE_NAME, &ctx.srv_dns_id,
				  dnssd_service_cb, &ctx, DNS_SD_DISCOVERY_TIMEOUT_MS);
	if (err < 0) {
		return err;
	}

	/* Wait for PTR response with instance name */
	err = k_sem_take(&ctx.done, K_MSEC(DNS_SD_DISCOVERY_TIMEOUT_MS));
	if (err != 0 || ctx.instance[0] == '\0') {
		if (ctx.srv_dns_id != 0U) {
			(void)dns_cancel_addr_info(ctx.srv_dns_id);
		}
		return (err != 0) ? -ETIMEDOUT : (ctx.status ? ctx.status : -ENOENT);
	}

	LOG_INF("PTR query complete: instance=%s", ctx.instance);

	/* Additional records (SRV, TXT, A) from the same mDNS response arrive as
	 * subsequent callbacks within microseconds. Wait briefly so addr_received
	 * is set before we decide whether to issue a separate A query. */
	k_sleep(K_MSEC(50));

	/* Extract hostname from instance name (e.g., "audiogateway" from
	 * "audiogateway._nrfwifiaudio._udp.local") */
	const char *dot = strchr(ctx.instance, '.');
	size_t hostname_len = dot ? (size_t)(dot - ctx.instance) : strlen(ctx.instance);
	hostname_len = MIN(hostname_len, sizeof(hostname) - 7); /* Reserve space for ".local" */
	memcpy(hostname, ctx.instance, hostname_len);
	snprintf(hostname + hostname_len, sizeof(hostname) - hostname_len, ".local");

	/* Step 2: Resolve A record for the hostname directly (skip if already received as
	 * additional record in the PTR response) */
	if (!ctx.addr_received) {
		LOG_INF("Querying A record for: %s", hostname);
		ctx.status = 0;
		err = dns_get_addr_info(hostname, DNS_QUERY_TYPE_A, &ctx.srv_dns_id,
					dnssd_service_cb, &ctx, DNS_SD_DISCOVERY_TIMEOUT_MS);
		if (err < 0) {
			return err;
		}

		retries = 5;
		while (retries-- > 0 && !ctx.addr_received) {
			err = k_sem_take(&ctx.done, K_MSEC(300));
			if (err != 0) {
				break;
			}
		}

		if (!ctx.addr_received) {
			if (ctx.srv_dns_id != 0U) {
				(void)dns_cancel_addr_info(ctx.srv_dns_id);
			}
			LOG_ERR("A query for %s failed", hostname);
			return ctx.status ? ctx.status : -ENOENT;
		}
	} else {
		LOG_INF("A record already received from PTR response: %d.%d.%d.%d",
			ctx.addr.s4_addr[0], ctx.addr.s4_addr[1], ctx.addr.s4_addr[2],
			ctx.addr.s4_addr[3]);
	}

	LOG_INF("Resolved gateway: %d.%d.%d.%d:%u", ctx.addr.s4_addr[0], ctx.addr.s4_addr[1],
		ctx.addr.s4_addr[2], ctx.addr.s4_addr[3], ctx.port);

	socket_utils_set_target_ipv4(&ctx.addr);

	if (ctx.port != 0U) {
		target_addr.sin_port = htons(ctx.port);
	}

	return 0;
}
#endif /* CONFIG_SOCKET_ROLE_CLIENT && CONFIG_DNS_SD */

#if defined(CONFIG_SOCKET_ROLE_CLIENT)
volatile bool serveraddr_set_signall = false;

static socket_utils_target_ready_cb_t target_ready_cb;
static bool socket_ready;
static bool target_ready_notified;

static void socket_utils_notify_target_ready(void)
{
	if (!serveraddr_set_signall || !socket_ready || target_ready_notified ||
	    target_ready_cb == NULL) {
		return;
	}

	target_ready_notified = true;
	target_ready_cb();
}

void socket_utils_set_target_ready_callback(socket_utils_target_ready_cb_t cb)
{
	target_ready_cb = cb;
	socket_utils_notify_target_ready();
}
#endif /*#if defined(CONFIG_SOCKET_ROLE_CLIENT)*/

volatile bool socket_connected_signall = false;

void socket_utils_set_rx_callback(net_util_socket_rx_callback_t socket_rx_callback)
{
	socket_rx_cb = socket_rx_callback;

	// If any messages are waiting in the queue, forward them immediately
	socket_receive_t socket_receive;

	while (k_msgq_get(&socket_recv_queue, &socket_receive, K_NO_WAIT) == 0) {
		socket_rx_cb(socket_receive.buf, socket_receive.len);
	}
}

static void socket_utils_trigger_rx_callback_if_set(void)
{
	LOG_DBG("Socket received %d bytes", socket_receive.len);
	// LOG_HEXDUMP_DBG(socket_receive.buf, socket_receive.len, "Buffer contents(HEX):");
	if (socket_rx_cb != 0) {
		socket_rx_cb(socket_receive.buf, socket_receive.len);
	} else {
		k_msgq_put(&socket_recv_queue, &socket_receive, K_NO_WAIT);
	}
}

#if defined(CONFIG_SOCKET_ROLE_CLIENT)
bool socket_utils_is_target_set(void)
{
	return serveraddr_set_signall;
}

void socket_utils_set_target_ipv4(const struct in_addr *addr)
{
	if ((addr == NULL) || (addr->s_addr == 0U)) {
		return;
	}

	if (serveraddr_set_signall && (target_addr.sin_addr.s_addr == addr->s_addr)) {
		return;
	}

	target_addr.sin_family = AF_INET;
	target_addr.sin_port = htons(socket_port);
	target_addr.sin_addr = *addr;

	inet_ntop(target_addr.sin_family, &target_addr.sin_addr, target_addr_str,
		  sizeof(target_addr_str));
	LOG_INF("Target address set to %s:%d", target_addr_str, socket_port);
	serveraddr_set_signall = true;
	target_ready_notified = false;
	socket_utils_notify_target_ready();
}

void socket_utils_clear_target(void)
{
	serveraddr_set_signall = false;
	target_ready_notified = false;
	socket_ready = false;
	LOG_INF("Cleared socket target state");
}
#endif /* CONFIG_SOCKET_ROLE_CLIENT */

int socket_utils_tx_data(uint8_t *data, size_t length)
{
#if defined(CONFIG_SOCKET_ROLE_SERVER)
	if (!socket_connected_signall || target_addr.sin_addr.s_addr == 0) {
		errno = ENOTCONN;
		LOG_DBG("Socket target not ready, dropping %zu byte payload", length);
		return -ENOTCONN;
	}
#else
	if (!serveraddr_set_signall || target_addr.sin_addr.s_addr == 0) {
		errno = ENOTCONN;
		LOG_DBG("Socket target unknown, dropping %zu byte payload", length);
		return -ENOTCONN;
	}
#endif

	size_t chunk_size = 1024;
	ssize_t bytes_sent = 0;

	while (length > 0) {
		size_t to_send = (length >= chunk_size) ? chunk_size : length;

		bytes_sent = sendto(udp_socket, data, to_send, 0, (struct sockaddr *)&target_addr,
				    sizeof(target_addr));

		if (bytes_sent == -1) {
			perror("Sending failed");
			return bytes_sent;
		}

		data += bytes_sent;
		length -= bytes_sent;
	}
	return bytes_sent;
}

#if defined(CONFIG_SOCKET_ROLE_SERVER)
void socket_utils_softap_handle_disconnect(void)
{
	socket_connected_signall = false;
	memset(&target_addr, 0, sizeof(target_addr));
	target_addr_len = sizeof(target_addr);
	LOG_INF("SoftAP client disconnected, socket target cleared");
}
#endif

/* Thread to setup WiFi, Sockets step by step */
void socket_utils_thread(void)
{
	int ret;

	ret = init_network_events();
	if (ret) {
		LOG_ERR("Failed to initialize network events: %d", ret);
		return;
	}
	k_sem_take(&wpa_supplicant_ready_sem, K_FOREVER);

#if IS_ENABLED(CONFIG_WIFI_NM_WPA_SUPPLICANT_AP)
	/* Device in SoftAP mode */
	LOG_INF("Wi-Fi Mode: SoftAP mode");

	ret = wifi_run_softap_mode();
	if (ret) {
		LOG_ERR("Failed to setup SoftAP mode: %d", ret);
		return;
	}

	ret = k_sem_take(&ipv4_dhcp_bond_sem, K_FOREVER);
	if (ret != 0) {
		LOG_ERR("Failed to wait for SoftAP network setup: %d", ret);
		return;
	}

	wifi_print_status();

	LOG_INF("SoftAP setup complete, waiting for station to connect...");
	LOG_INF("SSID: %s", CONFIG_SOFTAP_SSID);
	LOG_INF("Password: %s", CONFIG_SOFTAP_PASSWORD);
	LOG_INF("Socket server will start once a station connects");
	LOG_INF("Headset can connect using: wifi cred add -s %s -k 1 -p %s", CONFIG_SOFTAP_SSID,
		CONFIG_SOFTAP_PASSWORD);

	ret = k_sem_take(&station_connected_sem, K_FOREVER);
	if (ret) {
		LOG_ERR("Error waiting for station connection: %d", ret);
		return;
	}

	LOG_INF("Station connected! Starting socket server...");

#else
	/* Device in station mode */
	LOG_INF("Wi-Fi Mode: Station mode");
#if IS_ENABLED(CONFIG_WIFI_CREDENTIALS_STATIC)
	LOG_INF("Static Wi-Fi credentials configured for connection.");
#elif IS_ENABLED(CONFIG_WIFI_CREDENTIALS_SHELL)
	LOG_INF("Please use \"wifi cred\" shell commands set up Wi-Fi connection.");
#else
	LOG_INF("No Proper Wi-Fi credentials configured, try to configure with "
		"CONFIG_WIFI_CREDENTIALS_STATIC or CONFIG_WIFI_CREDENTIALS_SHELL");
#endif /* IS_ENABLED(CONFIG_WIFI_CREDENTIALS_STATIC) */
#if IS_ENABLED(CONFIG_SOCKET_ROLE_CLIENT)
	int auto_ret = wifi_utils_auto_connect_stored();
	if (auto_ret && auto_ret != -EALREADY && auto_ret != -ENOTSUP) {
		LOG_WRN("Auto-connect to stored credentials failed: %d", auto_ret);
	}
#endif
	ret = conn_mgr_all_if_connect(true);
	if (ret) {
		LOG_ERR("Failed to initiate network connection: %d", ret);
		return;
	}
	ret = k_sem_take(&ipv4_dhcp_bond_sem, K_FOREVER);
	if (ret != 0) {
		LOG_ERR("Failed to wait for network connectivity: %d", ret);
		return;
	}

	LOG_INF("Network connectivity established, setting up sockets...");

#endif /* IS_ENABLED(CONFIG_WIFI_NM_WPA_SUPPLICANT_AP) */

	self_addr.sin_family = AF_INET;
	self_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	self_addr.sin_port = htons(socket_port);

	target_addr.sin_family = AF_INET;

#if defined(CONFIG_SOCKET_ROLE_CLIENT)
	if (!socket_utils_is_target_set()) {

#if defined(CONFIG_DNS_SD) && defined(CONFIG_DNS_RESOLVER)
		int dns_sd_retries = 3;
		for (int attempt = 1; attempt <= dns_sd_retries; attempt++) {
			LOG_INF("DNS-SD discovery attempt %d/%d", attempt, dns_sd_retries);
			ret = dns_sd_discover_gateway();
			if (ret == 0) {
				LOG_INF("DNS-SD discovery succeeded on attempt %d", attempt);
				break;
			}
			LOG_WRN("DNS-SD discovery attempt %d failed (err %d)", attempt, ret);
			if (attempt < dns_sd_retries) {
				LOG_INF("Retrying in 2 seconds...");
				k_sleep(K_SECONDS(2));
			}
		}
#else
		ret = -ENOTSUP;
#endif /* CONFIG_DNS_SD && CONFIG_DNS_RESOLVER */

		if (ret < 0) {
			LOG_INF("DNS-SD lookup unavailable (err %d); waiting for DHCP-based target "
				"configuration",
				ret);
			LOG_INF("Hint: Use \"socket set_target_addr 192.168.1.1:60010\" to connect "
				"with gateway manually. Replace 192.168.1.1 with the actual "
				"gateway "
				"IP shown in the gateway device log output.");
		}
	} else {
		LOG_DBG("Target address already provisioned; skipping DNS-SD lookup");
	}

	while (!serveraddr_set_signall) {
		k_sleep(K_MSEC(100));
	}
	LOG_INF("Target address is set. Initializing socket transport");
#elif defined(CONFIG_SOCKET_ROLE_SERVER)
	LOG_INF("\r\n\r\nDevice works as socket server, wait for socket client connection...\r\n");
#endif // #if defined(CONFIG_SOCKET_ROLE_CLIENT)

#if defined(CONFIG_SOCKET_ROLE_CLIENT)
	socket_ready = false;
#endif

	for (;;) {
		udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (udp_socket < 0) {
			LOG_ERR("Failed to create socket: %d", -errno);
			k_sleep(K_SECONDS(1));
			continue;
		}

		ret = bind(udp_socket, (struct sockaddr *)&self_addr, sizeof(self_addr));
		if (ret < 0) {
			LOG_ERR("bind, error: %d", -errno);
			zsock_close(udp_socket);
			k_sleep(K_SECONDS(1));
			continue;
		}

#if defined(CONFIG_SOCKET_ROLE_CLIENT)
		if (serveraddr_set_signall && !socket_ready) {
			socket_ready = true;
			socket_utils_notify_target_ready();
		}
#endif

		while (true) {
			target_addr_len = sizeof(target_addr);
			socket_receive.len =
				recvfrom(udp_socket, socket_receive.buf, BUFFER_MAX_SIZE, 0,
					 (struct sockaddr *)&target_addr, &target_addr_len);
			if (socket_receive.len <= 0) {
				break;
			}
#if defined(CONFIG_SOCKET_ROLE_CLIENT)
			if (!serveraddr_set_signall) {
				inet_ntop(target_addr.sin_family, &target_addr.sin_addr,
					  target_addr_str, sizeof(target_addr_str));
				LOG_INF("Discovered socket server at %s:%d", target_addr_str,
					ntohs(target_addr.sin_port));
				socket_utils_set_target_ipv4(&target_addr.sin_addr);
			}

			if (!socket_ready) {
				socket_ready = true;
				socket_utils_notify_target_ready();
			}

#endif
			if (!socket_connected_signall) {
				inet_ntop(target_addr.sin_family, &target_addr.sin_addr,
					  target_addr_str, sizeof(target_addr_str));
				LOG_INF("Connect socket to IP Address %s:%d\n", target_addr_str,
					ntohs(target_addr.sin_port));
				socket_connected_signall = true;
			}
			socket_utils_trigger_rx_callback_if_set();
		}

		if (socket_receive.len == -1) {
			LOG_ERR("Receiving failed");
		} else if (socket_receive.len == 0) {
			LOG_INF("Client disconnected.\n");
		}

		zsock_close(udp_socket);
		socket_connected_signall = false;
#if defined(CONFIG_SOCKET_ROLE_CLIENT)
		socket_ready = false;
		target_ready_notified = false;
#endif
		k_sleep(K_SECONDS(1));
	}
}

#if defined(CONFIG_SOCKET_ROLE_CLIENT)

static int cmd_set_target_address(const struct shell *shell, size_t argc, const char **argv)
{
	// Ensure the command is provided with exactly one argument
	if (argc != 2) {
		shell_print(shell, "Usage: socket set_target_addr <IP:Port>");
		return -1;
	}

	// Check if WiFi is connected and IP address is assigned
	if (!net_event_mgmt_is_connected()) {
		shell_error(shell, "Error: WiFi is not connected or IP address not assigned.");
		shell_print(shell, "Please connect to WiFi first using:");
		shell_print(shell, "  wifi cred add -s <SSID> -p <password> -k 1");
		shell_print(shell, "  wifi cred auto_connect");
		shell_print(
			shell,
			"Wait for 'Network DHCP bound!' message before setting target address.");
		return -ENOTCONN;
	}

	char *target_addr_str = (char *)k_malloc(22); // Allocate memory for the string

	if (target_addr_str == NULL) {
		shell_print(shell, "Memory allocation failed");
		return -1;
	}

	// Get the target address string from the command argument
	strncpy(target_addr_str, argv[1], 22); // Use strncpy instead of strcpy for safety
	target_addr_str[21] = '\0';            // Ensure null-termination

	char ip_str[INET_ADDRSTRLEN];
	int port;

	// Split into IP address and port
	if (sscanf(target_addr_str, "%[^:]:%d", ip_str, &port) != 2) {
		shell_print(shell, "Invalid format. Expected <IP>:<Port>");
		k_free(target_addr_str); // Free the allocated memory
		return -1;
	}

	// Set up the sockaddr_in structure
	target_addr.sin_family = AF_INET;
	target_addr.sin_port = htons(port); // Convert port to network byte order

	if (inet_pton(AF_INET, ip_str, &(target_addr.sin_addr)) <= 0) {
		shell_print(shell, "Invalid IP address format: %s", ip_str);
		k_free(target_addr_str);
		return -1;
	}

	shell_print(shell, "Target address set to: %s:%d", ip_str, port);
	serveraddr_set_signall = true;
	target_ready_notified = false;
	socket_utils_notify_target_ready();
	k_free(target_addr_str);
	return 0;
}

// Existing shell command definitions
SHELL_STATIC_SUBCMD_SET_CREATE(socket_cmd,
			       SHELL_CMD(set_target_addr, NULL,
					 "Get and set target address in format <IP:Port>",
					 cmd_set_target_address),
			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(socket, &socket_cmd, "Socket commands", NULL);

#endif // #if defined(CONFIG_SOCKET_ROLE_CLIENT)
