# Network Module Spec - nordic-wifi-audio

## Document Information

| Field | Value |
|---|---|
| Project | Nordic Wi-Fi Audio Demo |
| Version | 2026-06-25-13-35 |
| PRD Version | 2026-06-25-13-30 |
| NCS Version | v3.3.0 |
| Target Board(s) | nRF5340 Audio DK + nRF7002EK (P0); nRF7002DK, nRF54LM20DK + nRF7002EB2 (build) |
| Status | In Review |

## Changelog

| Version | Summary of changes |
|---|---|
| 2026-06-25-13-35 | Updated to PRD v2026-06-25-13-30: dual-mode firmware; P2P_GO DHCP server needs NET_MAX_CONN/CONTEXTS=8; P2P_GC auto-connect by exact GO MAC (not OUI); STA mDNS discovery via MDNS_RESPONDER (gateway) + MDNS_RESOLVER/DNS_SERVER_IP_ADDRESSES (headset); _nrfwifiaudio._udp.local service |
| 2026-06-22-15-18 | Rewrite: zego-network consumption pattern replaces semaphore-based design; weak hooks documented; P2P peer resolution added; SoftAP mode retired |
| 2026-05-27-23-14 | Initial spec derived from code (Mode C Reverse) |

---

## Overview

The firmware is a **single dual-mode image**: one build supports both P2P
(Wi-Fi Direct) and STA (infrastructure + mDNS) operation. The default build
selects P2P via `-Dnordic-wifi-audio_SNIPPET=wifi-p2p`; the active Wi-Fi mode is
NVS-persisted and runtime-switchable (no reflash needed to move between P2P and
STA, or between roles within a build's allowed set).

The network module has two parts:

| File | Role |
|---|---|
| `src/modules/network/net_event_app.c` | Strong overrides of zego/network weak hooks → audio start/stop + `APP_WIFI_STATE_CHAN` |
| `src/modules/network/socket_utils.c` | UDP socket lifecycle, TX/RX thread, mode-branched peer resolution (STA mDNS / P2P fixed IP) |

The `zego/network` brick owns all Wi-Fi lifecycle (WPA supplicant sequencing,
P2P_GO auto-start, P2P_GC peer scan and connect, DHCP, AP station tracking).
The app provides application-specific behavior by overriding the brick's weak hooks.

Custom `net_event_mgmt.c` and `wifi_utils.c` (SoftAP/mode logic) are retired.

---

## Weak Hook API

`zego/bricks/network` defines six weak functions. The app provides strong overrides
in `src/modules/network/net_event_app.c`:

| Hook function | Fired when (per zego network-spec.md) | App action |
|---|---|---|
| `zego_on_net_event_wifi_connect()` | L2 connected; IP not yet ready (STA, P2P_GC) | Optional: log link up; no audio yet |
| `zego_on_net_event_dhcp_bound(mode, ip, mac, ssid)` | STA: DHCP_BOUND event; P2P_GC: CONNECT_RESULT (static 192.168.7.2); **P2P_GO: first AP_STA_CONNECTED** | **Start audio pipeline + socket; publish CONNECTED** |
| `zego_on_net_event_wifi_disconnect()` | Link lost (STA/P2P_GC disconnect result) | **Stop audio pipeline; publish ERROR** |
| `zego_on_net_event_wifi_ap_enabled()` | P2P_GO AP ready (before clients connect) | Optional: log AP up |
| `zego_on_net_event_wifi_ap_sta_connected(station_count, ip, mac)` | P2P_GO: each client joined | Track station count; audio already started by `dhcp_bound` on first client |
| `zego_on_net_event_wifi_ap_sta_disconnected(station_count)` | P2P_GO: client left | If station_count==0: **stop audio; publish ERROR** |

**Key confirmed behavior (zego network-spec.md, changelog 2026-06-14):**
- `dhcp_bound` is the **unified "network ready" hook for all modes**: fires from DHCP_BOUND (STA, router-assigned lease), from CONNECT_RESULT with static IP (P2P_GC), and from first AP_STA_CONNECTED (P2P_GO).
- STA: `ip_addr` = the DHCP-leased address from the infrastructure router. The headset does **not** use a fixed gateway IP here — it resolves the gateway via mDNS (see Peer Address Resolution). The fixed-GO-IP path (192.168.7.1) applies **only to P2P_GC**.
- P2P_GC: `ip_addr` = "192.168.7.2" (client's static IP, handed out by the P2P_GO DHCP server). The gateway GO is always at "192.168.7.1".
- P2P_GO: `ip_addr` = "192.168.7.1" (GO's own static IP). No brick gap — OI-003 resolved.

---

## Hook Implementation Pattern

```c
/* src/modules/network/net_event_app.c */

/* Channel definition — owned by this file */
ZBUS_CHAN_DEFINE(APP_WIFI_STATE_CHAN, struct app_wifi_state_msg,
    NULL, NULL, ZBUS_OBSERVERS_EMPTY,
    ZBUS_MSG_INIT(.state = APP_WIFI_STATE_CONNECTING, .mode = ZEGO_WIFI_MODE_P2P_GO));

/* Called by zego/network when STA gets DHCP lease, OR P2P_GC gets static IP */
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
    if (mode == ZEGO_WIFI_MODE_P2P_GC) {
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

| Kconfig | Role | Behaviour |
|---|---|---|
| `CONFIG_SOCKET_ROLE_SERVER=y` | Gateway | Binds UDP socket on port 60010; waits for first RX packet |
| `CONFIG_SOCKET_ROLE_CLIENT=y` | Headset | Resolves peer address (mode-branched); sends AUDIO_START_CMD |

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

| Mode | How headset finds gateway | Where implemented |
|---|---|---|
| STA | mDNS DNS-SD resolution of service `_nrfwifiaudio._udp.local` → gateway IP + port | `socket_utils.c` (`dns_sd_discover_gateway()`) |
| P2P_GC | Fixed GO IP `192.168.7.1` — set in `zego_on_net_event_dhcp_bound` | `net_event_app.c` hook |
| P2P_GO | Gateway binds to its own static IP `192.168.7.1`; no discovery needed | `socket_utils.c` server bind |

mDNS discovery is **STA-only** and requires NO hardcoded gateway IP. In P2P mode,
mDNS multicast is not used over the P2P link; the fixed IP pair is the correct approach.

### STA mDNS auto-discovery

The headset finds the gateway with no hardcoded IP, in STA (infrastructure) mode:

- **Gateway (advertiser)** — advertises the audio service via `CONFIG_MDNS_RESPONDER=y`
  + `CONFIG_DNS_SD=y` + `CONFIG_MDNS_RESPONDER_DNS_SD=y` (prj.conf). The
  `DNS_SD_REGISTER_UDP_SERVICE` macro in `socket_utils.c` registers service
  `_nrfwifiaudio._udp.local`, instance/hostname `audiogateway`
  (`CONFIG_NET_HOSTNAME`), port `60010`, with TXT records
  (`codec=opus`, `rate=320kbps`, `channels=2`, `latency=low`).
- **Headset (resolver)** — resolves the service via `CONFIG_MDNS_RESOLVER=y`
  + `CONFIG_DNS_SERVER_IP_ADDRESSES=y` (overlay-audio-headset.conf), on top of
  `CONFIG_DNS_RESOLVER=y` (prj.conf). `CONFIG_DNS_SERVER_IP_ADDRESSES` is **required**:
  it makes `dns_resolve_init_default()` register the mDNS multicast server
  (224.0.0.251:5353) into the default resolver's server list. Without it, `.local`
  queries have no server and `dns_resolve_service()` returns `-ENOENT` synchronously.
- **Resolution flow** (`dns_sd_discover_gateway()`): `dns_resolve_service()` PTR →
  service instance → SRV (host + port) → A (gateway IP) → `socket_utils_set_target_ipv4()`
  → connect to `gateway:60010` → auto-start stream.

---

## Zbus Integration

| Channel | Direction | Notes |
|---|---|---|
| `APP_WIFI_STATE_CHAN` | Publish | Published in each hook; subscribers: ux.c (LED) |

---

## Kconfig Flags

| Symbol | Description | Default |
|---|---|---|
| `CONFIG_SOCKET_UTILS` | Enable socket_utils module | y |
| `CONFIG_SOCKET_ROLE_SERVER` | UDP server (gateway) | in overlay |
| `CONFIG_SOCKET_ROLE_CLIENT` | UDP client (headset) | in overlay |
| `CONFIG_SOCKET_STACK_SIZE` | Socket thread stack size (bytes) | 6144 |
| `CONFIG_SOCKET_UTILS_THREAD_PRIO` | Socket thread priority | 6 |
| `CONFIG_ZEGO_NETWORK` | Enable zego/network brick | y |
| `CONFIG_MDNS_RESPONDER` / `CONFIG_DNS_SD` / `CONFIG_MDNS_RESPONDER_DNS_SD` | Gateway advertises `_nrfwifiaudio._udp.local` (STA mDNS auto-discovery) | y (prj.conf) |
| `CONFIG_DNS_RESOLVER` | DNS-SD PTR→A resolution support (STA) | y (prj.conf) |
| `CONFIG_MDNS_RESOLVER` | Headset multicasts `.local` queries to 224.0.0.251 (STA discovery) | y (headset overlay) |
| `CONFIG_DNS_SERVER_IP_ADDRESSES` | Required so `dns_resolve_init_default()` seeds the mDNS server (224.0.0.251:5353); without it `.local` queries return `-ENOENT` | y (headset overlay) |
| `CONFIG_ZEGO_WIFI_MODE_STA_ENABLED` | Expose STA mode on this role | y (gateway + headset) |
| `CONFIG_ZEGO_WIFI_MODE_P2P_GO_ENABLED` | Expose P2P_GO mode on this role | y gateway / n headset |
| `CONFIG_ZEGO_WIFI_MODE_P2P_GC_ENABLED` | Expose P2P_GC mode on this role | n gateway / y headset |
| `CONFIG_ZEGO_WIFI_DEFAULT_MODE_P2P_GO` | Default mode: P2P_GO (gateway) | y (gateway overlay) |
| `CONFIG_ZEGO_WIFI_DEFAULT_MODE_P2P_GC` | Default mode: P2P_GC (headset) | y (headset overlay) |
| `CONFIG_ZEGO_WIFI_P2P_GC_TARGET_GO_MAC` | **Full** GO MAC for P2P_GC auto-connect (NOT an OUI prefix). The brick does a direct BSS scan + associate by exact MAC, because an autonomous P2P_GO beacons but never enters P2P listen state, so prefix-based `P2P_FIND` discovery returns 0 peers. Set to the gateway's actual MAC. | `F4:CE:36:00:15:F2` (example) |
| `CONFIG_NET_DHCPV4_SERVER` | P2P_GO runs a Zephyr DHCPv4 server, assigning 192.168.7.2 to the client (gateway static IP 192.168.7.1) | y (gateway overlay) |
| `CONFIG_NET_DHCPV4_SERVER_ADDR_COUNT` | DHCP server address-pool size | 3 (gateway overlay) |
| `CONFIG_NET_MAX_CONN` / `CONFIG_NET_MAX_CONTEXTS` | **Must be 8** (defaults 4/6). The mDNS + hostap + app sockets exhaust the connection table, so the P2P_GO DHCP server's port-67 bind fails with `-ENOENT` (`net_conn: Not enough connection contexts`) and the client never gets an IP | 8 (prj.conf) |

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

| Condition | Handling |
|---|---|
| WPA supplicant timeout (30 s) | Logged by zego/network; hook not called; device stays in CONNECTING state |
| P2P peer not found (90 s) | zego/network retries after 15 s; no hook called |
| Brick gap: hook not fired for P2P_GC | Escalate as separate decision; do not inline |
| Socket create failure | `LOG_ERR`, sleep 1 s, retry loop |
| RX error (`len == -1`) | `LOG_ERR`, close + reconnect |
| Wi-Fi disconnect (STA) | `zego_on_net_event_wifi_disconnect()` called → audio stops; APP_WIFI_STATE_CHAN → ERROR |

---

## Test Points

| UART log string | Expected condition |
|---|---|
| `Network ready (mode=... ip=... ssid=...)` | `dhcp_bound` hook fired (STA lease / P2P_GC static IP / P2P_GO first client) |
| `Wi-Fi disconnected — stopping audio` | Disconnect hook fired |
| `AP/P2P_GO client joined (count=1)` | P2P_GO: first client joined |
| `P2P_GC: GO target set to 192.168.7.1` | P2P_GC: headset set fixed GO target |
| `Connect socket to IP Address ...:60010` | UDP socket connected for TX/RX |
| `Resolved gateway: <ip>:60010` | Headset mDNS DNS-SD resolve succeeded (STA mode only) |
