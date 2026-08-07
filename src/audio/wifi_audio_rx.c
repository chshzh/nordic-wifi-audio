/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <zephyr/kernel.h>
#include <nrfx_clock.h>

#include "streamctrl.h"
#include "audio_datapath.h"
#include "macros_common.h"
#include "audio_system.h"
#include "audio_sync_timer.h"
#include "wifi_audio_rx.h"
#include "socket_utils.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wifi_audio_rx, CONFIG_WIFI_AUDIO_RX_LOG_LEVEL);

struct ble_iso_data {
	// uint8_t data[251];
	uint8_t data[1920];
	size_t data_size;
	bool bad_frame;
	uint32_t sdu_ref;
	uint32_t recv_frame_ts;
} __packed;

struct rx_stats {
	uint32_t recv_cnt;
	uint32_t bad_frame_cnt;
	uint32_t data_size_mismatch_cnt;
};

static bool initialized;
static struct k_thread audio_datapath_thread_data;
static k_tid_t audio_datapath_thread_id;
K_THREAD_STACK_DEFINE(audio_datapath_thread_stack, CONFIG_AUDIO_DATAPATH_STACK_SIZE);

struct audio_pcm_data_t {
	size_t size;
	uint8_t data[1920];
};

#define CONFIG_BUF_WIFI_RX_PACKET_NUM 10

DATA_FIFO_DEFINE(wifi_audio_rx, CONFIG_BUF_WIFI_RX_PACKET_NUM, sizeof(struct audio_pcm_data_t));

#define CONFIG_CODEC_OPUS

#ifdef CONFIG_CODEC_OPUS

#endif
static int16_t rx_data_continute_count = 0;
static uint32_t rx_fifo_overruns;
void audio_data_frame_process(uint8_t *p_data, size_t data_size)
{
	int ret;
	uint32_t blocks_alloced_num, blocks_locked_num;
	struct audio_pcm_data_t *data_received = NULL;
	// static struct rx_stats rx_stats[AUDIO_CH_NUM];

	if (!initialized) {
		ERR_CHK_MSG(-EPERM, "Data received but wifi_audio_rx is not initialized");
	}

	// /* Capture timestamp of when audio frame is received */
	// uint32_t recv_frame_ts = audio_sync_timer_capture();

	// rx_stats[channel_index].recv_cnt++;

	// if (data_size != desired_data_size) {
	// 	/* A valid frame should always be equal to desired_data_size, set bad_frame
	// 	 * if that is not the case
	// 	 */
	// 	bad_frame = true;
	// 	rx_stats[channel_index].data_size_mismatch_cnt++;
	// }

	// if (bad_frame) {
	// 	rx_stats[channel_index].bad_frame_cnt++;
	// }

	// if ((rx_stats[channel_index].recv_cnt % 100) == 0 && rx_stats[channel_index].recv_cnt) {
	// 	/* NOTE: The string below is used by the Nordic CI system */
	// 	LOG_DBG("ISO RX SDUs: Ch: %d Total: %d Bad: %d Size mismatch %d", channel_index,
	// 		rx_stats[channel_index].recv_cnt, rx_stats[channel_index].bad_frame_cnt,
	// 		rx_stats[channel_index].data_size_mismatch_cnt);
	// }

	// if (stream_state_get() != STATE_STREAMING) {
	// 	/* Throw away data */
	// 	num_thrown++;
	// 	if ((num_thrown % 100) == 1) {
	// 		LOG_WRN("Not in streaming state (%d), thrown %d packet(s). Please press "
	// 			"play button.",
	// 			stream_state_get(), num_thrown);
	// 	}
	// 	return;
	// }

	// if (channel_index != AUDIO_CH_L && IS_ENABLED(CONFIG_AUDIO_GATEWAY)) {
	// 	/* Only left channel RX data in use on gateway */
	// 	return;
	// }

	ret = data_fifo_num_used_get(&wifi_audio_rx, &blocks_alloced_num, &blocks_locked_num);
	ERR_CHK(ret);

	if (blocks_alloced_num >= CONFIG_BUF_WIFI_RX_PACKET_NUM) {
		/* FIFO buffer is full, swap out oldest frame for a new one */

		void *stale_data;
		size_t stale_size;

		/* Drops a whole 10 ms frame, and it is invisible to the RX frame
		 * accounting because the frame was received correctly. */
		rx_fifo_overruns++;

		ret = data_fifo_pointer_last_filled_get(&wifi_audio_rx, &stale_data, &stale_size,
							K_NO_WAIT);
		ERR_CHK(ret);

		data_fifo_block_free(&wifi_audio_rx, stale_data);
		rx_data_continute_count = 0;
	}

	ret = data_fifo_pointer_first_vacant_get(&wifi_audio_rx, (void *)&data_received, K_NO_WAIT);
	ERR_CHK_MSG(ret, "Unable to get FIFO pointer");

	if (data_size > ARRAY_SIZE(data_received->data)) {
		ERR_CHK_MSG(-ENOMEM, "Data size too large for buffer");
		return;
	}

	// memcpy(iso_received->data, p_data+2, data_size-2);
	memcpy(data_received->data, p_data, data_size);
	// iso_received->bad_frame = bad_frame;
	data_received->size = data_size;
	// iso_received->sdu_ref = sdu_ref;
	// iso_received->recv_frame_ts = recv_frame_ts;

	ret = data_fifo_block_lock(&wifi_audio_rx, (void *)&data_received,
				   sizeof(struct audio_pcm_data_t));
	ERR_CHK_MSG(ret, "Failed to lock block");
}

#define TOTAL_PACKET_SIZE (1024 + 896) // Total size of the two packets to be assembled

#define MAX_AUDIO_FRAME_SIZE 1920
/* [0,1] start seq, [2] identifier, [3,4] payload length (big endian). The length makes
 * framing content-independent; scanning the payload for an end marker misfires whenever
 * PCM audio happens to contain those bytes, which truncates the frame and drops its tail.
 */
#define HEADER_SIZE      5
#define TAIL_HEADER_SIZE 3
#define FULL_FRAME_SIZE  (HEADER_SIZE + MAX_AUDIO_FRAME_SIZE)
#define AUDIO_CHUNK_SIZE 1024

static uint32_t rx_frames_ok;
static uint32_t rx_lost_head;
static uint32_t rx_lost_tail;

void wifi_audio_rx_data_handler(uint8_t *p_data, size_t data_size)
{
	/* Each audio frame spans up to two UDP datagrams (send_audio_frame() fragments at
	 * AUDIO_CHUNK_SIZE). Head and tail are each explicitly tagged by the sender — never
	 * inferred from payload content, which can coincidentally match either tag.
	 */
	static uint8_t frame_buffer[FULL_FRAME_SIZE];
	static size_t current_frame_size;
	size_t payload_len;
	bool is_head;
	bool is_tail;

	/* Command frames (REQ_PLAY_CMD/REQ_PAUSE_CMD/KEEP_ALIVE_ACK_CMD from the
	 * gateway) use the same start/cmd/end framing as gateway-bound commands,
	 * not the audio head/tail framing below - recognize and discard them here
	 * so they don't get counted as a desync. 6 bytes (no seq) or 7 bytes
	 * (KEEP_ALIVE_ACK_CMD's echoed seq, see send_keepalive_command()).
	 */
	if ((data_size == 6 || data_size == 7) && p_data[0] == START_SEQUENCE_1 &&
	    p_data[1] == START_SEQUENCE_2 && p_data[2] == SEND_CMD_SIGN &&
	    p_data[data_size - 2] == END_SEQUENCE_1 && p_data[data_size - 1] == END_SEQUENCE_2) {
#if defined(CONFIG_SOCKET_ROLE_CLIENT)
		uint8_t seq = (data_size == 7) ? p_data[4] : 0;

		streamctrl_handle_gateway_command(p_data[3], seq);
#endif
		return;
	}

	is_head = (data_size >= HEADER_SIZE && p_data[0] == START_SEQUENCE_1 &&
		   p_data[1] == START_SEQUENCE_2 && p_data[2] == SEND_DATA_SIGN);
	is_tail = (!is_head && data_size >= TAIL_HEADER_SIZE && p_data[0] == START_SEQUENCE_1 &&
		   p_data[1] == START_SEQUENCE_2 && p_data[2] == SEND_DATA_TAIL_SIGN);

	if (is_head) {
		if (data_size > FULL_FRAME_SIZE) {
			rx_lost_tail++;
			current_frame_size = 0;
			return;
		}

		memcpy(frame_buffer, p_data, data_size);
		current_frame_size = data_size;
	} else if (is_tail) {
		if (current_frame_size == 0) {
			/* Tail with no head in progress: the head datagram was lost. */
			rx_lost_head++;
			return;
		}

		p_data += TAIL_HEADER_SIZE;
		data_size -= TAIL_HEADER_SIZE;

		if (current_frame_size + data_size > FULL_FRAME_SIZE) {
			rx_lost_tail++;
			current_frame_size = 0;
			return;
		}

		memcpy(frame_buffer + current_frame_size, p_data, data_size);
		current_frame_size += data_size;
	} else {
		/* Neither an expected head nor a tail we're waiting for — desynced. */
		rx_lost_head++;
		current_frame_size = 0;
		return;
	}

	if (current_frame_size < HEADER_SIZE) {
		return;
	}

	payload_len = ((size_t)frame_buffer[3] << 8) | (size_t)frame_buffer[4];

	if (payload_len > MAX_AUDIO_FRAME_SIZE) {
		LOG_ERR("Invalid frame length %u", payload_len);
		current_frame_size = 0;
		return;
	}

	if (current_frame_size < (HEADER_SIZE + payload_len)) {
		/* Tail datagram still outstanding. */
		return;
	}

	audio_data_frame_process(frame_buffer + HEADER_SIZE, payload_len);

	/* ~100 frames/s, so this is a 5 s summary. */
	if ((++rx_frames_ok % 500) == 0) {
		LOG_DBG("RX frames ok=%u lost_head=%u lost_tail=%u fifo_ovr=%u", rx_frames_ok,
			rx_lost_head, rx_lost_tail, rx_fifo_overruns);
	}

	current_frame_size = 0;
}

uint32_t wifi_audio_rx_frame_count(void)
{
	return rx_frames_ok;
}

/**
 * @brief	Receive data from BLE through a k_fifo and send to USB or audio datapath.
 */
static void audio_datapath_thread(void *dummy1, void *dummy2, void *dummy3)
{
	int ret;
	struct audio_pcm_data_t *iso_received = NULL;
	size_t size_received;

	while (1) {
		ret = data_fifo_pointer_last_filled_get(&wifi_audio_rx, (void *)&iso_received,
							&size_received, K_FOREVER);
		ERR_CHK(ret);

		if (IS_ENABLED(CONFIG_AUDIO_SOURCE_USB) && IS_ENABLED(CONFIG_AUDIO_GATEWAY)) {
			// ret = audio_system_decode(iso_received->data, iso_received->data_size,
			//                          iso_received->bad_frame);
			ERR_CHK(ret);
		} else {
			audio_datapath_stream_out(iso_received->data, iso_received->size);
		}
		data_fifo_block_free(&wifi_audio_rx, (void *)iso_received);

		STACK_USAGE_PRINT("audio_datapath_thread", &audio_datapath_thread_data);
	}
}

static int audio_datapath_thread_create(void)
{
	int ret;

	audio_datapath_thread_id = k_thread_create(
		&audio_datapath_thread_data, audio_datapath_thread_stack,
		CONFIG_AUDIO_DATAPATH_STACK_SIZE, (k_thread_entry_t)audio_datapath_thread, NULL,
		NULL, NULL, K_PRIO_PREEMPT(CONFIG_AUDIO_DATAPATH_THREAD_PRIO), 0, K_NO_WAIT);
	ret = k_thread_name_set(audio_datapath_thread_id, "AUDIO_DATAPATH");
	if (ret) {
		LOG_ERR("Failed to create audio_datapath thread");
		return ret;
	}

	return 0;
}

int wifi_audio_rx_init(void)
{
	int ret;

	if (initialized) {
		return -EALREADY;
	}

	ret = data_fifo_init(&wifi_audio_rx);
	if (ret) {
		LOG_ERR("Failed to set up ble_rx FIFO");
		return ret;
	}

	ret = audio_datapath_thread_create();
	if (ret) {
		return ret;
	}

	initialized = true;

	return 0;
}

void send_audio_command(uint8_t audio_command)
{
	// Define the command packet with placeholders for start, command, and end
	uint8_t command_packet[] = {
		START_SEQUENCE_1, // 0xFF
		START_SEQUENCE_2, // 0xAA
		SEND_CMD_SIGN,    // 0x00 command; 0x01 data
		audio_command,    // Command: Variable (e.g., REQ_PLAY_CMD or REQ_PAUSE_CMD)
		END_SEQUENCE_1,   // 0xFF
		END_SEQUENCE_2    // 0xBB
	};

	size_t packet_size = sizeof(command_packet); // Calculate packet size
	socket_utils_tx_data((uint8_t *)command_packet, packet_size);
}

void send_keepalive_command(uint8_t audio_command, uint8_t seq)
{
	uint8_t command_packet[] = {
		START_SEQUENCE_1, START_SEQUENCE_2, SEND_CMD_SIGN, audio_command, seq,
		END_SEQUENCE_1,   END_SEQUENCE_2
	};

	socket_utils_tx_data((uint8_t *)command_packet, sizeof(command_packet));
}

void send_audio_frame(uint8_t *audio_data, size_t data_length)
{
	/* Only ever called from the encoder thread, so static buffers are safe and avoid
	 * a k_malloc/k_free pair 100 times a second.
	 */
	static uint8_t head_packet[AUDIO_CHUNK_SIZE];
	static uint8_t tail_packet[TAIL_HEADER_SIZE + MAX_AUDIO_FRAME_SIZE];
	size_t head_payload_len;
	size_t tail_payload_len;

	if (data_length > MAX_AUDIO_FRAME_SIZE) {
		LOG_ERR("Audio frame too large: %u", data_length);
		return;
	}

	head_packet[0] = START_SEQUENCE_1;
	head_packet[1] = START_SEQUENCE_2;
	head_packet[2] = SEND_DATA_SIGN;
	head_packet[3] = (uint8_t)(data_length >> 8);
	head_packet[4] = (uint8_t)(data_length & 0xFF);

	head_payload_len = MIN(data_length, sizeof(head_packet) - HEADER_SIZE);
	memcpy(head_packet + HEADER_SIZE, audio_data, head_payload_len);
	socket_utils_tx_data(head_packet, HEADER_SIZE + head_payload_len);

	tail_payload_len = data_length - head_payload_len;
	if (tail_payload_len == 0) {
		return;
	}

	/* Explicit tail tag instead of a header-less fragment — encoded/PCM payload bytes
	 * can coincidentally match the head tag, desyncing the receiver's reassembly.
	 */
	tail_packet[0] = START_SEQUENCE_1;
	tail_packet[1] = START_SEQUENCE_2;
	tail_packet[2] = SEND_DATA_TAIL_SIGN;
	memcpy(tail_packet + TAIL_HEADER_SIZE, audio_data + head_payload_len, tail_payload_len);
	socket_utils_tx_data(tail_packet, TAIL_HEADER_SIZE + tail_payload_len);
}
