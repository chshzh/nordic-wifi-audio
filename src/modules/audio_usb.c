/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "audio_usb.h"

#include <zephyr/kernel.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_uac2.h>
#include <zephyr/drivers/usb/usb_buf.h>
#include <data_fifo.h>

#include "macros_common.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(audio_usb, CONFIG_MODULE_AUDIO_USB_LOG_LEVEL);

/* One UAC2 isochronous transfer carries 1 ms of stereo audio, matching one
 * data_fifo block (BLOCK_SIZE_BYTES). For 48 kHz / 16-bit / stereo this is
 * 192 bytes. The audio pipeline accumulates CONFIG_FIFO_FRAME_SPLIT_NUM of
 * these into one frame.
 */
#define USB_FRAME_SIZE_STEREO                                                                      \
	(((CONFIG_AUDIO_SAMPLE_RATE_HZ * CONFIG_AUDIO_BIT_DEPTH_OCTETS) / 1000) * 2)

/* Terminal entity IDs resolved from the UAC2 device tree node. */
#define TERMINAL_ID_OUT UAC2_ENTITY_ID(DT_NODELABEL(out_terminal)) /* host -> device (audio source) */
#define TERMINAL_ID_IN	UAC2_ENTITY_ID(DT_NODELABEL(in_terminal))  /* device -> host (mic, bidir) */

/* Absolute minimum is 2 buffers; 2 extra prevent out-of-memory errors when the
 * USB host performs rapid terminal enable/disable cycles.
 */
#define USB_BLOCKS 4

/* UDC-aligned DMA scratch blocks for USB transfers. The received block is
 * copied into the (non-UDC-aligned) data_fifo and the scratch is freed.
 * See usb_buf.h for UDC_BUF_GRANULARITY and UDC_BUF_ALIGN.
 */
K_MEM_SLAB_DEFINE_STATIC(usb_rx_slab, ROUND_UP(USB_FRAME_SIZE_STEREO, UDC_BUF_GRANULARITY),
			 USB_BLOCKS, UDC_BUF_ALIGN);
#if (CONFIG_STREAM_BIDIRECTIONAL)
K_MEM_SLAB_DEFINE_STATIC(usb_tx_slab, ROUND_UP(USB_FRAME_SIZE_STEREO, UDC_BUF_GRANULARITY),
			 USB_BLOCKS, UDC_BUF_ALIGN);
#endif /* (CONFIG_STREAM_BIDIRECTIONAL) */

static struct data_fifo *fifo_tx;
static struct data_fifo *fifo_rx;

static struct usbd_context *audio_usbd;

static uint32_t rx_num_overruns;
static bool rx_first_data;
static bool terminal_out_enabled;
#if (CONFIG_STREAM_BIDIRECTIONAL)
static uint32_t tx_num_underruns;
static bool tx_first_data;
static bool terminal_in_enabled;
#endif /* (CONFIG_STREAM_BIDIRECTIONAL) */

static void terminal_update_cb(const struct device *dev, uint8_t terminal, bool enabled,
			       bool microframes, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(microframes);
	ARG_UNUSED(user_data);

	if (terminal == TERMINAL_ID_OUT) {
		terminal_out_enabled = enabled;
	}
#if (CONFIG_STREAM_BIDIRECTIONAL)
	else if (terminal == TERMINAL_ID_IN) {
		terminal_in_enabled = enabled;
	}
#endif /* (CONFIG_STREAM_BIDIRECTIONAL) */
}

static void *get_recv_buf_cb(const struct device *dev, uint8_t terminal, uint16_t size,
			     void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(size);
	ARG_UNUSED(user_data);

	void *buf = NULL;

	if (terminal == TERMINAL_ID_OUT) {
		if (k_mem_slab_alloc(&usb_rx_slab, &buf, K_NO_WAIT) != 0) {
			buf = NULL;
		}
	}

	return buf;
}

static void data_recv_cb(const struct device *dev, uint8_t terminal, void *buf, uint16_t size,
			 void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(terminal);
	ARG_UNUSED(user_data);

	int ret;
	void *data_in;

	if (buf == NULL) {
		LOG_ERR("Received NULL buffer");
		return;
	}

	/* No consumer attached, empty transfer, or terminal disabled: discard. */
	if (fifo_rx == NULL || size == 0 || !terminal_out_enabled) {
		k_mem_slab_free(&usb_rx_slab, buf);
		return;
	}

	if (size != USB_FRAME_SIZE_STEREO) {
		/* Hosts (e.g. macOS) occasionally send a runt iso-OUT packet that is
		 * not the nominal 1 ms / USB_FRAME_SIZE_STEREO size. Drop it (one
		 * inaudible 1 ms gap) — it cannot go into the fixed-size data_fifo
		 * block. Logged at DBG since it is an expected, benign host artifact.
		 */
		LOG_DBG("Wrong length: %u", size);
		k_mem_slab_free(&usb_rx_slab, buf);
		return;
	}

	ret = data_fifo_pointer_first_vacant_get(fifo_rx, &data_in, K_NO_WAIT);

	/* RX FIFO can fill up due to retransmissions or client disconnect. */
	if (ret == -ENOMEM) {
		void *temp;
		size_t temp_size;

		if ((++rx_num_overruns % 100) == 1) {
			LOG_DBG("USB RX overrun. Num: %d", rx_num_overruns);
		}

		ret = data_fifo_pointer_last_filled_get(fifo_rx, &temp, &temp_size, K_NO_WAIT);
		if (ret == 0) {
			data_fifo_block_free(fifo_rx, temp);
		}

		ret = data_fifo_pointer_first_vacant_get(fifo_rx, &data_in, K_NO_WAIT);
	}

	if (ret != 0) {
		LOG_WRN("RX failed to get block: %d", ret);
		k_mem_slab_free(&usb_rx_slab, buf);
		return;
	}

	memcpy(data_in, buf, size);
	k_mem_slab_free(&usb_rx_slab, buf);

	ret = data_fifo_block_lock(fifo_rx, &data_in, size);
	ERR_CHK_MSG(ret, "Failed to lock block");

	if (!rx_first_data) {
		LOG_INF("USB RX first data received.");
		rx_first_data = true;
	}
}

/* SOF callback is mandatory for UAC2 — the driver invokes it on every Start-of-Frame
 * (usbd_uac2.c uac2_sof) with no NULL check. It drives the device->host (mic) TX path,
 * which is only active in bidirectional mode; in the default unidirectional build it
 * is a no-op.
 */
static void usb_send_cb(const struct device *dev, void *user_data)
{
#if (CONFIG_STREAM_BIDIRECTIONAL)
	ARG_UNUSED(user_data);

	int ret;
	void *pcm_buf;
	void *data_out;
	size_t data_out_size;

	if (fifo_tx == NULL || !terminal_in_enabled) {
		return;
	}

	ret = data_fifo_pointer_last_filled_get(fifo_tx, &data_out, &data_out_size, K_NO_WAIT);
	if (ret) {
		if ((++tx_num_underruns % 100) == 1) {
			LOG_WRN("USB TX underrun. Num: %d", tx_num_underruns);
		}
		return;
	}

	if (k_mem_slab_alloc(&usb_tx_slab, &pcm_buf, K_NO_WAIT) != 0) {
		LOG_WRN("Could not allocate pcm_buf");
		data_fifo_block_free(fifo_tx, data_out);
		return;
	}

	memcpy(pcm_buf, data_out, data_out_size);
	data_fifo_block_free(fifo_tx, data_out);

	ret = usbd_uac2_send(dev, TERMINAL_ID_IN, pcm_buf, data_out_size);
	if (ret) {
		LOG_WRN("USB TX failed, ret: %d", ret);
		k_mem_slab_free(&usb_tx_slab, pcm_buf);
		return;
	}

	if (!tx_first_data) {
		LOG_INF("USB TX first data sent.");
		tx_first_data = true;
	}
#else  /* !CONFIG_STREAM_BIDIRECTIONAL */
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);
#endif /* (CONFIG_STREAM_BIDIRECTIONAL) */
}

#if (CONFIG_STREAM_BIDIRECTIONAL)
static void send_buf_release_cb(const struct device *dev, uint8_t terminal, void *buf,
				void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(terminal);
	ARG_UNUSED(user_data);

	k_mem_slab_free(&usb_tx_slab, buf);
}
#endif /* (CONFIG_STREAM_BIDIRECTIONAL) */

static struct uac2_ops ops = {
	/* sof_cb is mandatory — must be non-NULL even in unidirectional mode. */
	.sof_cb = usb_send_cb,
	.terminal_update_cb = terminal_update_cb,
	.get_recv_buf = get_recv_buf_cb,
	.data_recv_cb = data_recv_cb,
#if (CONFIG_STREAM_BIDIRECTIONAL)
	.buf_release_cb = send_buf_release_cb,
#endif /* (CONFIG_STREAM_BIDIRECTIONAL) */
};

int audio_usb_start(struct data_fifo *fifo_tx_in, struct data_fifo *fifo_rx_in)
{
	if (audio_usbd == NULL) {
		LOG_ERR("USB device not initialized");
		return -ENOTCONN;
	}

	if (fifo_tx_in == NULL || fifo_rx_in == NULL) {
		return -EINVAL;
	}

	fifo_tx = fifo_tx_in;
	fifo_rx = fifo_rx_in;

	return 0;
}

void audio_usb_stop(void)
{
	rx_first_data = false;
#if (CONFIG_STREAM_BIDIRECTIONAL)
	tx_first_data = false;
#endif /* (CONFIG_STREAM_BIDIRECTIONAL) */
	fifo_tx = NULL;
	fifo_rx = NULL;
}

int audio_usb_disable(void)
{
	int ret;

	audio_usb_stop();

	if (audio_usbd != NULL) {
		ret = usbd_disable(audio_usbd);
		if (ret) {
			LOG_ERR("Failed to disable USB");
			return ret;
		}
	}

	return 0;
}

int audio_usb_init(void)
{
	int ret;
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(uac2_headset));

	if (!device_is_ready(dev)) {
		LOG_ERR("USB Device not ready");
		return -EIO;
	}

	usbd_uac2_set_ops(dev, &ops, NULL);

	audio_usbd = audio_usbd_init_device(NULL);
	if (audio_usbd == NULL) {
		return -ENODEV;
	}

	ret = usbd_enable(audio_usbd);
	if (ret) {
		LOG_ERR("Failed to enable USB");
		return ret;
	}

	LOG_INF("Ready for USB host to send/receive.");

	return 0;
}
