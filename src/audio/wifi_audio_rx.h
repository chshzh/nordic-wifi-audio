/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef _WIFI_AUDIO_RX_H_
#define _WIFI_AUDIO_RX_H_

#define START_SEQUENCE_1   0xFF
#define START_SEQUENCE_2   0xAA
#define END_SEQUENCE_1     0xFF
#define END_SEQUENCE_2     0xBB
#define SEND_CMD_SIGN      0x00
#define SEND_DATA_SIGN     0x01
/* Marks the 2nd UDP datagram of a frame split across two sends. Audio payload bytes
 * (raw PCM or Opus) can coincidentally match SEND_DATA_SIGN, so the tail needs its own
 * explicit, sender-written tag rather than being inferred from "has no header".
 */
#define SEND_DATA_TAIL_SIGN 0x02
#define REQ_PLAY_CMD  0x00
#define REQ_PAUSE_CMD 0x01
/* Client->server liveness ping. Carries no state change; its only job is to keep
 * uplink traffic flowing so the AP does not disassociate the (otherwise
 * receive-only) client for inactivity, and to re-teach the server the client's
 * address after the server's socket has been torn down and rebound.
 */
#define KEEP_ALIVE_CMD 0x02
/* Server->client reply to KEEP_ALIVE_CMD, confirming the gateway actually saw it. */
#define KEEP_ALIVE_ACK_CMD 0x03

void send_audio_command(uint8_t audio_command);

/**
 * @brief Send KEEP_ALIVE_CMD or KEEP_ALIVE_ACK_CMD with a sequence number, so
 *        the receiver can log/correlate which ping an ACK answers.
 */
void send_keepalive_command(uint8_t audio_command, uint8_t seq);

void send_audio_frame(uint8_t *audio_data, size_t data_length);

/**
 * @brief Number of complete audio frames handed to the datapath since boot.
 *
 * Used by the headset to detect a silently stalled stream (frame count frozen
 * while the stream is supposed to be running).
 */
uint32_t wifi_audio_rx_frame_count(void);

/**
 * @brief Data handler when audio data has been received through WiFi.
 *
 * @param[in] p_data		Pointer to the received data.
 * @param[in] data_size		Size of the received data.
 * @param[in] bad_frame		Bad frame flag. (I.e. set for missed ISO data).
 * @param[in] sdu_ref		SDU reference timestamp.
 * @param[in] channel_index	Which channel is received.
 * @param[in] desired_data_size	The expected data size.
 *
 * @return 0 if successful, error otherwise.
 */
// void wifi_audio_rx_data_handler(uint8_t const *const p_data, size_t data_size, bool bad_frame,
// 			      uint32_t sdu_ref, enum audio_channel channel_index,
// 			      size_t desired_data_size);

void wifi_audio_rx_data_handler(uint8_t *p_data, size_t data_size);
/**
 * @brief Initialize the receive audio path.
 *
 * @return 0 if successful, error otherwise.
 */
int wifi_audio_rx_init(void);

/**
 * @brief	Encoded audio data and information.
 *
 * @note	Container for SW codec (typically LC3) compressed audio data.
 */
struct le_audio_encoded_audio {
	uint8_t const *const data;
	size_t size;
	uint8_t num_ch;
};

#endif /* _WIFI_AUDIO_RX_H_ */
