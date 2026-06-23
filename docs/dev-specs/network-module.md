# Network Module Spec

## Document Information

| Field          | Value                                                                            |
|----------------|----------------------------------------------------------------------------------|
| Project        | Nordic Wi-Fi Audio Demo                                                          |
| Version        | 2026-06-22-15-18                                                                 |
| PRD Version    | 2026-06-22-15-18                                                                 |
| NCS Version    | v3.3.0                                                                           |
| Target Board(s)| nRF5340 Audio DK + nRF7002EK (P0); nRF7002DK, nRF54LM20DK + nRF7002EB2 (build) |
| Status         | In Review                                                                        |

## Changelog

| Version          | Summary of changes                                                              |
|------------------|---------------------------------------------------------------------------------|
| 2026-06-22-15-18 | Rewrite: zego-network consumption pattern replaces semaphore-based design; weak hooks documented; P2P peer resolution added; SoftAP mode retired |
| 2026-05-27-23-14 | Initial spec derived from code (Mode C Reverse)                                 |

---

## Overview

The network module has two parts:

| File                                  | Role                                                              |
|---------------------------------------|-------------------------------------------------------------------|
| `src/modules/network/net_event_app.c` | Strong overrides of zego/network weak hooks → audio start/stop + `APP_WIFI_STATE_CHAN` |
| `src/net/socket_utils.c`             | UDP socket lifecycle, TX/RX thread, mode-branched peer resolution |

The `zego/network` brick owns all Wi-Fi lifecycle (WPA supplicant sequencing,
P2P_GO auto-start, P2P_CLIENT peer scan and connect, DHCP, AP station tracking).
The app provides application-specific behavior by overriding the brick's weak hooks.

Custom `net_event_mgmt.c` and `wifi_utils.c` (SoftAP/mode logic) are retired.

---

## Weak Hook API

`zego/bricks/network` defines six weak functions. The app provides strong overrides
in `src/modules/network/net_event_app.c`:

| Hook function                                 | Fired when (per zego network-spec.md)             | App action                                        |
|-----------------------------------------------|---------------------------------------------------|---------------------------------------------------|
| `zego_on_net_event_wifi_connect()`            | L2 connected; IP not yet ready (STA, P2P_CLIENT)  | Optional: log link up; no audio yet               |
| `zego_on_net_event_dhcp_bound(mode, ip, mac, ssid)` | STA: DHCP_BOUND event; P2P_CLIENT: CONNECT_RESULT (static 192.168.7.2); **P2P_GO: first AP_STA_CONNECTED** | **Start audio pipeline + socket; publish CONNECTED** |
| `zego_on_net_event_wifi_disconnect()`         | Link lost (STA/P2P_CLIENT disconnect result)      | **Stop audio pipeline; publish ERROR**            |
| `zego_on_net_event_wifi_ap_enabled()`         | P2P_GO AP ready (before clients connect)          | Optional: log AP up                               |
| `zego_on_net_event_wifi_ap_sta_connected(station_count, ip, mac)` | P2P_GO: each client joined | Track station count; audio already started by `dhcp_bound` on first client |
| `zego_on_net_event_wifi_ap_sta_disconnected(station_count)` | P2P_GO: client left | If station_count==0: **stop audio; publish ERROR** |

**Key confirmed behavior (zego network-spec.md, changelog 2026-06-14):**
- `dhcp_bound` is the **unified "network ready" hook for all modes**: fires from DHCP_BOUND (STA), from CONNECT_RESULT with static IP (P2P_CLIENT), and from first AP_STA_CONNECTED (P2P_GO).
- P2P_CLIENT: `ip_addr` = "192.168.7.2" (client's static IP). Gateway GO is always at "192.168.7.1".
- P2P_GO: `ip_addr` = "192.168.7.1" (GO's own IP). No brick gap — OI-003 resolved.

---

## Hook Implementation Pattern

```c
/* src/modules/network/net_event_app.c */

/* Channel definition — owned by this file */
ZBUS_CHAN_DEFINE(APP_WIFI_STATE_CHAN, struct app_wifi_state_msg,
    NULL, NULL, ZBUS_OBSERVERS_EMPTY,
    ZBUS_MSG_INIT(.state = APP_WIFI_STATE_CONNECTING, .mode = ZEGO_WIFI_MODE_P2P_GO));

/* Called by zego/network when STA gets DHCP lease, OR P2P_CLIENT gets static IP */
void zego_on_net_event_dhcp_bound(enum zego_wifi_mode mode,
                                   const struct in_addr *ip,
                                   const uint8_t *mac,
                                   const char *ssid)
{
    LOG_INF("Connected (%s) — starting audio", zego_wifi_mode_str(mode));

    /* Start audio pipeline */
    audio_system_encoder_start();           /* gateway: encode + transmit */
    /* wifi_audio_rx already init'd in main(); socket ready after target set */

    /* Notify ux module */
    struct app_wifi_state_msg msg = {
        .state = APP_WIFI_STATE_CONNECTED,
        .mode  = mode,
    };
    zbus_chan_pub(&APP_WIFI_STATE_CHAN, &msg, K_MSEC(10));

    /* Set peer address for socket transport (mode-branched — see socket_utils spec) */
    if (mode == ZEGO_WIFI_MODE_P2P_CLIENT) {
        /* Fixed GO IP — no mDNS on P2P link */
        struct in_addr go_addr;
        zsock_inet_pton(AF_INET, "192.168.7.1", &go_addr);
        socket_utils_set_target_ipv4(&go_addr);
    }
    /* STA mode: headset uses mDNS discovery (existing path in socket_utils) */
}

void zego_on_net_event_wifi_disconnect(void)
{
    LOG_INF("Disconnected — stopping audio");
    audio_system_encoder_stop();
    struct app_wifi_state_msg msg = {
        .state = APP_WIFI_STATE_ERROR,
    };
    zbus_chan_pub(&APP_WIFI_STATE_CHAN, &msg, K_MSEC(10));
}

/* P2P_GO: subsequent clients (audio already started by dhcp_bound on first) */
void zego_on_net_event_wifi_ap_sta_connected(int station_count,
                                              const struct in_addr *ip,
                                              const uint8_t *mac)
{
    LOG_INF("P2P client connected (%d total)", station_count);
    /* dhcp_bound handles audio start on first client; nothing more needed here */
}

/* P2P_GO: client left */
void zego_on_net_event_wifi_ap_sta_disconnected(int station_count)
{
    if (station_count == 0) {
        LOG_INF("All P2P clients gone — stopping audio");
        audio_system_encoder_stop();
        struct app_wifi_state_msg msg = {
            .state = APP_WIFI_STATE_ERROR,
        };
        zbus_chan_pub(&APP_WIFI_STATE_CHAN, &msg, K_MSEC(10));
    }
}
```

---

## Socket Roles

Selected at build time — mutually exclusive:

| Kconfig                     | Role     | Behaviour                                                        |
|-----------------------------|----------|------------------------------------------------------------------|
| `CONFIG_SOCKET_ROLE_SERVER=y` | Gateway | Binds UDP socket on port 60010; waits for first RX packet       |
| `CONFIG_SOCKET_ROLE_CLIENT=y` | Headset | Resolves peer address (mode-branched); sends AUDIO_START_CMD    |

The `socket_utils_thread()` loop:
1. Create UDP socket
2. Bind to local port
3. Wait for target address (P2P: set in hook; STA: mDNS discovery)
4. Enter RX loop — posts frames to `socket_recv_queue`
5. On disconnect/error: `zsock_close()`, sleep 1 s, retry

**The socket thread no longer calls `k_sem_take()` on `wpa_supplicant_ready_sem`,
`ipv4_dhcp_bond_sem`, or `station_connected_sem`**. Those semaphores are retired.
Instead the thread waits for `socket_utils_is_target_set()` to become true (set by
the hook) or for a direct call to `socket_utils_set_target_ipv4()`.

---

## Peer Address Resolution (Mode-Branched)

| Mode         | How headset finds gateway                                          | Where implemented        |
|--------------|--------------------------------------------------------------------|--------------------------|
| STA          | mDNS DNS-SD resolution of `audiogateway.local` → IP               | `socket_utils.c` (existing path) |
| P2P_CLIENT   | Fixed GO IP `192.168.7.1` — set in `zego_on_net_event_dhcp_bound` | `net_event_app.c` hook   |
| P2P_GO       | Gateway binds to its own static IP `192.168.7.1`; no discovery needed | `socket_utils.c` server bind |

mDNS discovery is **STA-only**. In P2P mode, mDNS multicast is not reliable over the
P2P link; the fixed IP pair is the correct approach.

---

## Zbus Integration

| Channel              | Direction | Notes                                         |
|----------------------|-----------|-----------------------------------------------|
| `APP_WIFI_STATE_CHAN` | Publish  | Published in each hook; subscribers: ux.c (LED) |

---

## Kconfig Flags

| Symbol                            | Description                                            | Default      |
|-----------------------------------|--------------------------------------------------------|--------------|
| `CONFIG_SOCKET_UTILS`             | Enable socket_utils module                             | y            |
| `CONFIG_SOCKET_ROLE_SERVER`       | UDP server (gateway)                                   | in overlay   |
| `CONFIG_SOCKET_ROLE_CLIENT`       | UDP client (headset)                                   | in overlay   |
| `CONFIG_SOCKET_STACK_SIZE`        | Socket thread stack size (bytes)                       | 6144         |
| `CONFIG_SOCKET_UTILS_THREAD_PRIO` | Socket thread priority                                 | 6            |
| `CONFIG_ZEGO_NETWORK`             | Enable zego/network brick                              | y            |
| `CONFIG_ZEGO_NETWORK_MDNS`        | Enable mDNS responder (STA mode: `audiogateway.local`) | y (STA overlay) |
| `CONFIG_ZEGO_WIFI_DEFAULT_MODE_P2P_GO` | Default mode: P2P_GO (gateway)                  | y (gateway prj.conf) |
| `CONFIG_ZEGO_WIFI_DEFAULT_MODE_P2P_CLIENT` | Default mode: P2P_CLIENT (headset)           | y (headset prj.conf) |
| `CONFIG_ZEGO_WIFI_P2P_CLIENT_TARGET_GO_MAC` | OUI prefix for P2P_CLIENT target GO        | `F4:CE:36:00:00:00` (example; set to actual GW OUI) |

---

## API

### `net_event_app.c` (app-owned)
```c
/* Zbus channel — defined here, declared extern in messages.h */
extern const struct zbus_channel APP_WIFI_STATE_CHAN;

/* No public functions — all interaction is via weak hooks and the zbus channel */
```

### `socket_utils.h` (kept)
```c
void socket_utils_set_rx_callback(net_util_socket_rx_callback_t cb);
int  socket_utils_tx_data(uint8_t *data, size_t length);
void socket_utils_thread(void);
bool socket_utils_is_target_set(void);
void socket_utils_set_target_ipv4(const struct in_addr *addr);
void socket_utils_clear_target(void);
void socket_utils_set_target_ready_callback(socket_utils_target_ready_cb_t cb);
```

---

## Error Handling

| Condition                          | Handling                                          |
|------------------------------------|---------------------------------------------------|
| WPA supplicant timeout (30 s)      | Logged by zego/network; hook not called; device stays in CONNECTING state |
| P2P peer not found (90 s)          | zego/network retries after 15 s; no hook called   |
| Brick gap: hook not fired for P2P_CLIENT | Escalate as separate decision; do not inline | 
| Socket create failure              | `LOG_ERR`, sleep 1 s, retry loop                  |
| RX error (`len == -1`)             | `LOG_ERR`, close + reconnect                      |
| Wi-Fi disconnect (STA)             | `zego_on_net_event_wifi_disconnect()` called → audio stops; APP_WIFI_STATE_CHAN → ERROR |

---

## Test Points

| UART log string                                   | Expected condition                         |
|---------------------------------------------------|--------------------------------------------|
| `[net_event_app] Connected — starting audio`      | `dhcp_bound` or `ap_sta_connected` hook fired |
| `[net_event_app] Disconnected — stopping audio`   | Disconnect hook fired                      |
| `[net_event_app] P2P client connected (1 total)`  | P2P_GO: first client joined                |
| `Socket ready` / `Socket connected`               | UDP socket ready for TX/RX                 |
| `Resolving audiogateway.local...`                 | Headset mDNS resolve (STA mode only)       |
