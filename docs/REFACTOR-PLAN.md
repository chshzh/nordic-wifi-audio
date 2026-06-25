# Refactor Plan — nordic-wifi-audio → zego bricks

| Field | Value |
|-------|-------|
| Plan Version | 2026-06-22 |
| NCS Version | v3.3.0 |
| Target | Re-platform nordic-wifi-audio onto the `zego` brick library + template architecture |
| Workflow | Follows `chsh-sk-ncs-0-workflow` → Phase 1 (PRD) → Phase 2 (Specs) → Phase 3 (Coding) → Phase 4 (V&V) |
| Reference app | `zego/nordic-wifi-app-template/` (STA / SoftAP / P2P + BLE prov) |
| Reference lib | `zego/bricks/{button,led,wifi,network,memonitor,wifi_ble_prov}/` |

---

## 0. Decisions & Assumptions (confirm before Phase 3)

These are locked into the plan. Items marked **[ASSUMPTION]** are reasonable defaults — correct them if wrong.

1. **Default build = P2P autoconnect + raw PCM (opus OFF).** *(confirmed)*
   - Default image: gateway = `P2P_GO`, headset = `P2P_CLIENT` auto-connect, codec = raw PCM.
   - **Opus is overlay-gated (`overlay-opus.conf`) and only ever combined with STA.** Never build `P2P + opus` in one image (RAM/flash will not fit with the WPA-supplicant P2P heap + libopus working set).
   - This **reverses the current PRD default** (PRD v2026-06-03 made STA the default) → PRD edit required in Phase 1.

2. **"Remove SoftAP" = retire the app's SoftAP *mode/path*, NOT the AP machinery.** *(scope guardrail)*
   - P2P_GO **is** an 802.11 AP. In the zego network brick it shares the DHCP server (`wifi_setup_dhcp_server()`, static `192.168.7.1/24`), the `connected_stations[]` table, and `l2_ap_event_handler()` with SoftAP. **That shared infra stays.**
   - Remove only: SoftAP as a *selectable mode*, `CONFIG_ZEGO_WIFI_DEFAULT_MODE_SOFTAP` selection in the app, the SoftAP overlay, SoftAP SSID/password Kconfig in the app, and any SoftAP branch in the app's UX mode-cycle.

3. **zego is a pinned, read-only dependency.** *(scope guardrail)*
   - `zego` is already in `west.yml` (`path: zego`, `revision: main`). All refactor work lives **inside `nordic-wifi-audio/`** — the app *consumes* bricks via `EXTRA_ZEPHYR_MODULES` + `CONFIG_ZEGO_*`. We do **not** edit bricks.
   - If a brick gap is found (e.g. a hook or Kconfig we need does not exist), it is surfaced as a **separate decision**, not folded into this plan.

4. **Board scope (P0 vs deferred).**
   - **P0 (must work):** gateway **and** headset on **nRF5340 Audio DK + nRF7002EK** (I2S + CS47L63 codec).
   - **Deferred / "optional now":** gateway via **USB audio** on **nRF7002DK** and on **nRF54LM20DK + nRF7002EB2**. Headset on any board other than nRF5340 Audio DK.
   - The build must not *break* the deferred boards, but their USB-audio path is not a P0 deliverable.

5. **App structure: keep the two separate role apps.** **[ASSUMPTION]**
   - Keep `wifi_audio_gateway/main.c` and `wifi_audio_headset/main.c` selected by `CONFIG_AUDIO_GATEWAY` / `CONFIG_AUDIO_HEADSET` (as today). The refactor swaps the *net/UI/diagnostics* layers for zego bricks; it does not unify the two role apps. (The template is single-`main.c`; the audio domain justifies keeping role split.)

6. **P2P client GO-targeting = OUI-prefix mode.** **[ASSUMPTION]**
   - Set `CONFIG_ZEGO_WIFI_P2P_CLIENT_TARGET_GO_MAC` to an **OUI prefix** (e.g. `F4:CE:36:00:00:00`) so the headset finds *any* matching gateway via `WIFI_P2P_FIND` + RSSI selection — avoids hardcoding one board's MAC. (Exact-MAC mode is the alternative if a fixed pairing is wanted.)

7. **Peer-address resolution is branched by mode.** *(critical seam — see §4)*
   - **STA:** dynamic IPs; headset discovers gateway via mDNS (`audiogateway.local`).
   - **P2P:** static IPs (GO `192.168.7.1`, client `192.168.7.2`, no DHCP on client). Headset uses the **fixed GO IP** — do **not** rely on mDNS over the P2P link.

---

## 1. Current State vs Target

### 1.1 Module disposition (custom code → zego brick)

| Concern | Today (custom, in `nordic-wifi-audio/src/`) | Target (zego) | Action |
|---------|----------------------------------------------|---------------|--------|
| Wi-Fi mode select + NVS persistence | `src/net/mode_selector.c/.h` (`wifi_mode` shell, default P2P at line 37) | `zego/bricks/wifi` (`CONFIG_ZEGO_WIFI`, `WIFI_MODE_CHAN`, NVS key `app/zego_wifi_mode`) | **Retire custom; adopt brick** |
| Wi-Fi events / DHCP / P2P / AP | `src/net/net_event_mgmt.c/.h` (semaphores: `iface_up_sem`, `wpa_supplicant_ready_sem`, `ipv4_dhcp_bond_sem`, `p2p_peer_connected_sem`) | `zego/bricks/network` (weak-hook API + zbus) | **Retire custom; adopt brick + implement hooks** |
| STA/P2P connect logic | `src/net/wifi_utils.c/.h` (`wifi_run_p2p_go_mode`, `wifi_run_p2p_client_mode`, `wifi_utils_auto_connect_stored`) | `zego/bricks/network` + `zego/bricks/wifi` | **Retire custom; adopt brick** |
| Buttons | `src/modules/button_handler.c/.h` | `zego/bricks/button` (`CONFIG_ZEGO_BUTTON`, `BUTTON_CHAN`) | **Retire custom; adopt brick** |
| LEDs | `src/modules/led.c/.h` | `zego/bricks/led` (`CONFIG_ZEGO_LED`, `LED_CMD_CHAN`/`LED_STATE_CHAN`) | **Retire custom; adopt brick** |
| Button→action / event→LED policy | (mixed into button/led/net code) | App `ux` module, modelled on `template/src/modules/ux/ux.c` | **Add app `src/modules/ux/`** |
| Diagnostics / mem report | `src/debug/diagnostics.c/.h` | `zego/bricks/memonitor` (`CONFIG_ZEGO_MEMONITOR`, `MEMONITOR_CHAN`) + thin app `status` cmd | **Rewrite around memonitor** |
| BLE provisioning (STA creds) | (not present) | `zego/bricks/wifi_ble_prov` (optional, nRF54LM20DK) | **[ASSUMPTION] out of scope now; leave disabled** |
| Audio pipeline (capture/codec/datapath) | `src/audio/*`, `lib/opus_interface/*`, `src/drivers/cs47l63*` | (no zego equivalent — keep) | **Keep; adapt start/stop triggers to hooks** |
| UDP transport + framing | `src/net/socket_utils.c/.h` | (no zego equivalent — keep) | **Keep; add mode-branched peer resolution** |
| Board init / channel assignment | `src/utils/*` | (no zego equivalent — keep) | **Keep** |

### 1.2 Wiring change

- **`CMakeLists.txt`:** add `EXTRA_ZEPHYR_MODULES` block (before `find_package(Zephyr ...)`) pointing at `../zego/bricks/{button,led,wifi,network,memonitor}` (pattern from `template/CMakeLists.txt`). Drop `add_subdirectory(src/net)` pieces that become dead.
- **`prj.conf`:** add `CONFIG_ZEGO_{BUTTON,LED,WIFI,NETWORK,MEMONITOR}=y`, `CONFIG_APP_UX_MODULE=y`; remove `CONFIG_ZEGO_WIFI_MODE_SELECTOR` and custom button/led symbols.
- **`boards/*.conf`:** set per-board `CONFIG_ZEGO_BUTTON_NUM_BUTTONS`, `CONFIG_ZEGO_LED_NUM_LEDS`, and `CONFIG_APP_UX_*` (Audio DK uses `ROTATE_FIRST_LED=3`, `ROTATE_COUNT=3`, `CONNECTED_LED=4`, `CONNECTED_LED_GREEN_ONLY=y`).
- **`prj.conf`:** set `CONFIG_ZEGO_WIFI_DEFAULT_MODE_P2P_GO=y` (gateway) / `_P2P_CLIENT=y` (headset) as the default; STA selected via overlay.

---

## 2. The verifiable artifact — build matrix

Every cell either builds clean (Phase 3) or runs on HW (Phase 4). **`opus` only ever appears with `STA`.**

| # | Role | Board | Mode | Codec | Priority | Verify |
|---|------|-------|------|-------|----------|--------|
| 1 | gateway | nRF5340 Audio DK + EK | **P2P_GO** | PCM | **P0 (default)** | build + HW |
| 2 | headset | nRF5340 Audio DK + EK | **P2P_CLIENT** | PCM | **P0 (default)** | build + HW |
| 3 | gateway | nRF5340 Audio DK + EK | STA | opus (overlay) | **P0** | build + HW |
| 4 | headset | nRF5340 Audio DK + EK | STA | opus (overlay) | **P0** | build + HW |
| 5 | gateway | nRF5340 Audio DK + EK | STA | PCM | P1 | build |
| 6 | headset | nRF5340 Audio DK + EK | STA | PCM | P1 | build |
| 7 | gateway | nRF7002DK | P2P_GO | PCM | P2 (no audio I/O parity) | build |
| 8 | gateway | nRF7002DK (USB audio) | P2P_GO | PCM | **Deferred** | — |
| 9 | gateway | nRF54LM20DK + EB2 (USB audio) | P2P_GO | PCM | **Deferred** | — |
| ✗ | any | any | **P2P + opus** | — | **NEVER BUILT** | — |

> If any "optional now" board cannot build at all (not just lacks audio I/O), `log()` it as a known gap rather than silently dropping it.

---

## 3. Phase 1 — PRD update  (`skill: chsh-sk-ncs-1-prd`)

Edit `docs/pm-prd/PRD.md`; add a Changelog row `YYYY-MM-DD-HH-MM`, bump `Version`.

1. **Reverse the default:** P2P autoconnect is the **default** mode; STA is the **optional** mode (carries the opus codec test). Update Problem Statement §1 and the mode FR accordingly.
2. **Adopt zego explicitly:** add a requirement/section that the device UI + Wi-Fi connectivity are built on the shared `zego` brick library (button, led, wifi, network, memonitor), matching the template.
3. **Codec gating (NFR):** state that the Opus codec is an **opt-in overlay** and is supported only in STA mode; P2P + Opus is explicitly **out of scope** for a single image (memory constraint). Reference the memory NFR.
4. **Board scope:** P0 = nRF5340 Audio DK (gateway + headset). nRF7002DK and nRF54LM20DK+EB2 gateway **USB-audio** = "optional / future". Headset = nRF5340 Audio DK only.
5. **Remove SoftAP** from any remaining user-facing language (already mostly done in v2026-06-03).
6. Re-check acceptance criteria for FR-013 (P2P persistent pairing), FR-014 (per-mode audio profiles), FR-015 (auto-recovery): mark as **stretch / not in this refactor** unless explicitly pulled in — current code does not implement them and the refactor should not silently drop them.

---

## 4. Phase 2 — Specs update  (`skill: chsh-sk-ncs-2-spec`)

Set each spec's `PRD Version` to the new PRD timestamp; add Changelog rows. **Per-spec disposition:**

| Spec | Disposition | Notes |
|------|-------------|-------|
| `dev-specs/overview.md` | **Update** | New module map: app = audio + transport + ux + board-init; connectivity/UI = zego bricks. List zbus channels actually used (`BUTTON_CHAN`, `LED_CMD_CHAN`, `WIFI_MODE_CHAN`, `MEMONITOR_CHAN`, app `APP_WIFI_STATE_CHAN`). |
| `dev-specs/architecture.md` | **Update** | Boot sequence rewritten: SYS_INIT brick priorities (wifi 41 → network 42; led/button 45) **replace** the semaphore handshake. Document the weak-hook → audio start/stop flow. Refresh memory budget with the P2P+PCM default and STA+opus variant. |
| `dev-specs/audio-pipeline.md` | **Update** | Codec abstraction unchanged; add **peer-address resolution by mode** (STA = mDNS `audiogateway.local`; P2P = fixed GO IP `192.168.7.1`). State opus = STA-only. |
| `dev-specs/network-module.md` | **Rewrite** | Becomes "network = zego-network consumption". Document the six weak hooks the app overrides (`zego_on_net_event_wifi_connect/dhcp_bound/wifi_disconnect/wifi_ap_enabled/wifi_ap_sta_connected/wifi_ap_sta_disconnected`) and how each drives audio + `APP_WIFI_STATE_CHAN`. |
| `dev-specs/mode-selector.md` | **Retire** | Superseded by zego wifi brick. Replace with a short pointer to `zego/bricks/wifi/docs/wifi-spec.md` + the app's default-mode Kconfig choice. |
| `dev-specs/ui-module.md` | **Rewrite** | App `ux` module: button gestures (single=print mode, long-press=cycle mode→NVS→reboot) and LED states, modelled on `template/src/modules/ux/ux.c`. Mode cycle = STA → P2P_GO → P2P_CLIENT (no SoftAP). |
| `dev-specs/diagnostics-module.md` | **Rewrite** | Around `memonitor` brick (`MEMONITOR_CHAN`, `memonitor_get_heaps/threads()`); thin `status` shell command consumes it. |
| `dev-specs/board-init-module.md` | **Update** | Reflect per-board `CONFIG_ZEGO_*` button/LED counts + `CONFIG_APP_UX_*`; note deferred USB-audio boards. |

---

## 5. Phase 3 — Coding  (`skill: chsh-sk-ncs-3.1-coding`)

Ordered steps; each ends with a clean build of the relevant matrix cell.

**Step 3.1 — Wire the bricks (no behaviour change yet).**
- `CMakeLists.txt`: add `EXTRA_ZEPHYR_MODULES` for the 5 bricks (template pattern). `prj.conf`: enable `CONFIG_ZEGO_*`. Per-board `boards/*.conf`: button/LED counts + UX LED mapping.
- Verify: project configures and builds with bricks compiled in (old code still present).

**Step 3.2 — Add the app `ux` module.**
- Create `src/modules/ux/` (ux.c, Kconfig, CMakeLists) from the template; define `APP_WIFI_STATE_CHAN` in an app `messages.h`. Map `BUTTON_CHAN` gestures → mode cycle (STA → P2P_GO → P2P_CLIENT, NVS save, reboot) and `APP_WIFI_STATE_CHAN` → `LED_CMD_CHAN`.
- Verify: builds; buttons/LEDs respond (HW smoke).

**Step 3.3 — Rewrite `main.c` (gateway + headset) onto weak-hooks. [HIGHEST RISK]**
- Delete the semaphore handshake (`iface_up_sem`, `wpa_supplicant_ready_sem`, `ipv4_dhcp_bond_sem`, `p2p_peer_connected_sem`).
- Implement `zego_on_net_event_*` strong overrides in `src/modules/network/net_event_app.c`. Start/stop the audio pipeline + UDP transport **from the hooks** (`dhcp_bound`/`ap_sta_connected` → start; `wifi_disconnect`/`ap_sta_disconnected` → stop) and publish `APP_WIFI_STATE_CHAN`.
- Verify: gateway (P2P_GO) and headset (P2P_CLIENT) reach "connected" and the hook fires.

**Step 3.4 — Branch peer-address resolution by mode (transport).**
- In `socket_utils.c`: STA → resolve `audiogateway.local` via mDNS (existing path); P2P_CLIENT → use fixed GO IP `192.168.7.2`→`192.168.7.1` (no mDNS). Gateway binds its static/known IP per mode.
- Verify: matrix cells 1–2 stream audio over P2P; cells 3–4 stream over STA.

**Step 3.5 — Retire dead custom code.**
- Remove `src/net/mode_selector.*`, `src/net/net_event_mgmt.*`, custom `wifi_utils` mode logic now covered by the brick, `src/modules/button_handler.*`, `src/modules/led.*`, and the SoftAP-only paths. Remove orphaned includes / `add_subdirectory` / Kconfig.
- **Keep** audio (`src/audio/*`, `lib/opus_interface/*`), transport framing (`socket_utils`), drivers, board init, channel assignment.
- Verify: clean build, no unused-symbol warnings; matrix cells 1–6 build.

**Step 3.6 — Overlay & default hygiene.**
- `prj.conf` default = P2P (gateway `_P2P_GO`, headset `_P2P_CLIENT`). `overlay-opus.conf` keeps `CONFIG_SW_CODEC_OPUS=y` (STA-only). Provide/verify a STA overlay. Delete the SoftAP overlay + SoftAP Kconfig in the app.
- Verify: `P2P+PCM` (default) and `STA+opus` (overlay) both build; confirm **no** build combines P2P+opus.

---

## 6. Phase 4 — Verification & Validation

- **4.1 Verification** (`chsh-sk-ncs-4.1-verification`): code review, clean build of all P0/P1 matrix cells, doc-consistency audit (PRD ↔ specs ↔ code).
- **4.2 Validation** (`chsh-sk-ncs-4.2-validation`): on nRF5340 Audio DK pairs —
  - P2P autoconnect: power both → gateway GO, headset client auto-connects → audio streams (cells 1–2).
  - STA + opus: both join router → mDNS discovery → audio streams (cells 3–4).
  - Mode switch via long-press → NVS persist → reboot into new mode.
  - Capture peak thread/heap watermarks (ZView + memonitor) → feed `chsh-sk-ncs-3.3-memopt`; confirm P2P+PCM and STA+opus each fit.

---

## 7. Risks & open items

1. **main.c rewrite (Step 3.3)** is the highest-risk change — the semaphore→weak-hook conversion touches boot ordering for both role apps. Land it behind the brick wiring (3.1–3.2) so bricks are proven before main.c depends on them.
2. **Memory:** P2P+PCM default and STA+opus must each be measured on nRF5340 Audio DK (Step 3.6 / Phase 4.2). The "never P2P+opus" rule is the mitigation.
3. **Brick gap risk:** if a needed hook/Kconfig is absent in zego (e.g. an audio-specific connected event), surface as a separate decision — do **not** edit bricks inline.
4. **Deferred boards (nRF7002DK / nRF54LM20DK USB audio):** keep them building (config-only) even though USB-audio I/O is out of P0 scope; `log()` any that cannot build.
5. **FR-013/014/015** (persistent pairing, per-mode profiles, auto-recovery) are specced but uncoded. This refactor neither implements nor deletes them — Phase 1 marks them stretch.
