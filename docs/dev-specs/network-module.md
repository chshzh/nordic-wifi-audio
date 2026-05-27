# Network Module Spec

## Document Information

| Field          | Value                        |
|----------------|------------------------------|
| Project        | Nordic Wi-Fi Opus Audio Demo |
| NCS Version    | v3.3.0                       |
| PRD Version    | 2026-05-27-23-14             |
| Latest Version | 2026-05-27-23-14             |

## Changelog

| Version          | Summary of changes                                         |
|------------------|------------------------------------------------------------|
| 2026-05-27-23-14 | Initial spec derived from code (Mode C Reverse)            |

---

## Overview

The network module handles all Wi-Fi and socket operations. Three source files:

| File                         | Role                                                        |
|------------------------------|-------------------------------------------------------------|
| `src/net/socket_utils.c`     | UDP socket lifecycle, TX/RX thread, server/client framing   |
| `src/net/wifi_utils.c`       | Wi-Fi management: SoftAP, STA connect, credentials, SSID    |
| `src/net/net_event_mgmt.c`   | `net_mgmt` event callbacks; exports semaphores for main     |

---

## File Locations

```
src/net/
├── socket_utils.c/h    — UDP socket (server/client), RX queue, TX
├── wifi_utils.c/h      — SoftAP mode, connect, credentials, SSID utils
└── net_event_mgmt.c/h  — net_mgmt callbacks, exported semaphores
```

---

## Socket Roles

Selected at build time — mutually exclusive:

| Kconfig                     | Role     | Behaviour                                              |
|-----------------------------|----------|--------------------------------------------------------|
| `CONFIG_SOCKET_ROLE_SERVER=y` | Gateway | Binds UDP socket; waits for client to connect          |
| `CONFIG_SOCKET_ROLE_CLIENT=y` | Headset | Resolves `audiogateway.local` via mDNS; connects to server |

The socket thread runs continuously in `socket_utils_thread()`:
1. Create UDP socket
2. Bind to local port (server) or resolve + set target address (client)
3. Enter RX loop — posts received frames to `socket_recv_queue` (k_msgq)
4. On disconnect/error: `zsock_close()`, sleep 1 s, retry from step 1

---

## Zbus Integration

The network module does not publish or subscribe to Zbus directly.
It communicates with the application via:
- `socket_recv_queue` (k_msgq) — RX frames posted here, consumed by `wifi_audio_rx`
- `net_util_socket_rx_callback_t` — optional RX callback registered by `wifi_audio_rx_init()`
- `socket_connected_signall` / `serveraddr_set_signall` — volatile bool signals

---

## Network Event Semaphores

`net_event_mgmt.c` exports these semaphores (taken by `main()` during boot):

| Semaphore                    | Given when                                      |
|------------------------------|-------------------------------------------------|
| `iface_up_sem`               | Network interface UP event received             |
| `wpa_supplicant_ready_sem`   | WPA supplicant ready event received             |
| `ipv4_dhcp_bond_sem`         | DHCP lease assigned (STA mode)                  |
| `station_connected_sem`      | Station joined the SoftAP (SoftAP/server mode)  |

`net_event_mgmt_is_connected()` returns true when WiFi is connected with DHCP IP assigned.

---

## Wi-Fi Modes

### STA Mode (default)
- Uses `CONFIG_WIFI_CREDENTIALS` (persistent credential store).
- `wifi_utils_ensure_gateway_softap_credentials()` seeds default `GatewayAP` WPA2 credentials.
- `wifi_utils_auto_connect_stored()` triggers `NET_REQUEST_WIFI_CONNECT_STORED`.
- Static credentials: `overlay-wifi-cred-static.conf` (dev/test only).

### SoftAP Mode (`overlay-gateway-softap.conf`)
- `wifi_run_softap_mode()` starts AP with configured SSID/channel/band.
- Default: 5 GHz channel 165 (configurable: `CONFIG_SOFTAP_BAND_5_GHZ`, `CONFIG_SOFTAP_CHANNEL`).
- SSID: `CONFIG_SOFTAP_SSID` (default `"GatewayAP"`), password: `CONFIG_SOFTAP_PASSWORD`.
- `wifi_softap_has_connected_stations()` / `wifi_softap_wait_for_station()` for client detection.

---

## mDNS / DNS-SD

**Gateway** (server role):
- `CONFIG_MDNS_RESPONDER=y`, `CONFIG_DNS_SD=y`, `CONFIG_NET_HOSTNAME="audiogateway"`
- Advertises audio service via DNS-SD so headset can discover by name.

**Headset** (client role):
- `CONFIG_MDNS_RESOLVER=y`, `CONFIG_DNS_RESOLVER=y`, `CONFIG_NET_HOSTNAME="audioheadset"`
- Resolves `audiogateway.local` to gateway's IP via mDNS multicast (224.0.0.251).
- `CONFIG_NET_SOCKETS_DNS_TIMEOUT=1000` (1 s per query — mDNS is fast on local network).
- `CONFIG_NET_IPV4_IGMP=y` required for multicast group membership.

---

## Kconfig Flags

| Symbol                            | Description                                      | Default |
|-----------------------------------|--------------------------------------------------|---------|
| `CONFIG_SOCKET_UTILS`             | Enable socket_utils module                       | y       |
| `CONFIG_SOCKET_ROLE_SERVER`       | UDP server (gateway)                             | in overlay |
| `CONFIG_SOCKET_ROLE_CLIENT`       | UDP client (headset)                             | in overlay |
| `CONFIG_SOCKET_TYPE_UDP`          | Transport type (currently only UDP supported)    | y       |
| `CONFIG_SOCKET_STACK_SIZE`        | Socket thread stack size (bytes)                 | 4096    |
| `CONFIG_SOCKET_UTILS_THREAD_PRIO` | Socket thread priority                           | 6       |
| `CONFIG_CONNECT_WITH_WIFI`        | Enable WiFi connectivity (vs raw/injection mode) | y       |
| `CONFIG_SOFTAP_SSID`              | SoftAP network name                              | `"GatewayAP"` |
| `CONFIG_SOFTAP_PASSWORD`          | SoftAP password                                  | `"wifi1234"` |
| `CONFIG_SOFTAP_BAND_5_GHZ`        | Use 5 GHz band for SoftAP                        | in overlay |
| `CONFIG_SOFTAP_CHANNEL`           | SoftAP channel number                            | 165 (5 GHz) |

---

## API / Public Interface

### `socket_utils.h`
```c
void socket_utils_set_rx_callback(net_util_socket_rx_callback_t cb);
int  socket_utils_tx_data(uint8_t *data, size_t length);
void socket_utils_thread(void);

/* SERVER role */
void socket_utils_softap_handle_disconnect(void);

/* CLIENT role */
bool socket_utils_is_target_set(void);
void socket_utils_set_target_ipv4(const struct in_addr *addr);
void socket_utils_clear_target(void);
void socket_utils_set_target_ready_callback(socket_utils_target_ready_cb_t cb);

/* SoftAP helpers */
bool wifi_softap_has_connected_stations(void);
int  wifi_softap_wait_for_station(k_timeout_t timeout);
```

### `wifi_utils.h`
```c
int         wifi_run_softap_mode(void);
int         wifi_print_status(void);
void        wifi_print_dhcp_ip(struct net_mgmt_event_callback *cb);
const char *wifi_utils_get_last_ssid(void);
int         wifi_utils_ensure_gateway_softap_credentials(void);
int         wifi_utils_auto_connect_stored(void);
int         wifi_set_reg_domain(void);
int         wifi_set_channel(int channel);
int         wifi_set_mode(int mode);
int         wifi_set_tx_injection_mode(void);
```

### `net_event_mgmt.h`
```c
int  init_network_events(void);
bool net_event_mgmt_is_connected(void);
extern struct k_sem iface_up_sem;
extern struct k_sem wpa_supplicant_ready_sem;
extern struct k_sem ipv4_dhcp_bond_sem;
extern struct k_sem station_connected_sem;  /* SoftAP mode only */
```

---

## Error Handling

| Condition                        | Handling                                               |
|----------------------------------|--------------------------------------------------------|
| Socket create failure            | `LOG_ERR`, sleep 1 s, retry loop                       |
| `bind()` failure                 | `LOG_ERR`, `zsock_close()`, sleep 1 s, retry           |
| RX error (`len == -1`)           | `LOG_ERR`, exit inner loop, close + reconnect          |
| Client disconnected (`len == 0`) | `LOG_INF`, `zsock_close()`, reset signal flags         |
| TX failure                       | Return negative errno to caller                        |
| mDNS resolution timeout          | DNS resolver retries per `CONFIG_DNS_RESOLVER_ADDITIONAL_QUERIES` |
| Wi-Fi connect failure            | `net_event_mgmt` logs; `ipv4_dhcp_bond_sem` never given |

---

## Test Points

| UART log string                         | Expected condition                    |
|-----------------------------------------|---------------------------------------|
| `Network events initialized`            | `init_network_events()` success       |
| `WiFi connected`                        | `NET_EVENT_WIFI_CONNECT_RESULT`       |
| `DHCP IP: <addr>`                       | DHCP lease assigned                   |
| `Socket ready`                          | UDP socket bound (server) or target set (client) |
| `Station connected`                     | SoftAP mode: client joined            |
| `Client disconnected`                   | Server: client left, stream paused    |
| `Resolving audiogateway.local...`       | Headset mDNS resolve in progress      |
