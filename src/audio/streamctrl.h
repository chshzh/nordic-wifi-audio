/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _STREAMCTRL_H_
#define _STREAMCTRL_H_

#include <stddef.h>
#include <zephyr/kernel.h>

/* State machine states for peer or stream. */
enum stream_state {
	STATE_STREAMING,
	STATE_PAUSED,
};

/* What the user (local button, or a REQ_PLAY_CMD/REQ_PAUSE_CMD command from
 * the peer) has asked for - independent of whether audio is actually
 * flowing right now. See gateway_reevaluate_stream() in
 * wifi_audio_gateway/main.c and wifi_audio_headset/main.c.
 */
enum audio_user_request {
	REQ_PLAY,
	REQ_PAUSE,
};

/**
 * @brief Get the current streaming state.
 *
 * @return      strm_state enum value.
 */
uint8_t stream_state_get(void);

/**
 * @brief Send audio data over the stream.
 *
 * @param data		Data to send.
 * @param size		Size of data.
 * @param num_ch	Number of audio channels.
 */
void streamctrl_send(void const *const data, size_t size);

#if defined(CONFIG_SOCKET_ROLE_SERVER)
void streamctrl_handle_client_disconnect(void);

/**
 * @brief Notify streamctrl whether the USB host is actively sending audio.
 *
 * Called by the USB audio driver when it detects the host has started or
 * stopped sending non-silent PCM data, so the Wi-Fi stream can be paused
 * while the host isn't playing anything and resumed once it is again.
 *
 * @param active	true if the host is sending audio, false if idle/silent.
 */
void streamctrl_handle_usb_audio_active(bool active);
#endif

#if defined(CONFIG_SOCKET_ROLE_CLIENT)
/**
 * @brief Handle a command received from the gateway, keeping the headset's
 *        user-request state in sync with the gateway's.
 *
 * @param cmd	REQ_PLAY_CMD, REQ_PAUSE_CMD, or KEEP_ALIVE_ACK_CMD (wifi_audio_rx.h).
 * @param seq	Sequence number echoed back for KEEP_ALIVE_ACK_CMD; 0 otherwise.
 */
void streamctrl_handle_gateway_command(uint8_t cmd, uint8_t seq);
#endif

#endif /* _STREAMCTRL_H_ */
