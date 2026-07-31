/*
 *  Copyright (c) 2021, PACKETCRAFT, INC.
 *
 *  SPDX-License-Identifier: LicenseRef-PCFT
 */

#include "audio_i2s.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pinctrl.h>
#include <nrfx_i2s.h>
#include <nrfx_clock.h>
#include <hal/nrf_clock.h>

#include <zephyr/logging/log.h>

#include "audio_sync_timer.h"

LOG_MODULE_REGISTER(audio_i2s, CONFIG_MODULE_AUDIO_I2S_LOG_LEVEL);

#define I2S_NL DT_NODELABEL(i2s0)

/* Upper bound on the HFCLKAUDIO PLL lock time; it normally reports started
 * within a few ms.
 */
#define ACLK_START_TIMEOUT_MS 20

/* One I2S block is 1 ms, so the first "next buffers needed" event is at most
 * that far away; these bounds cover it with margin.
 */
#define I2S_NEXT_BUF_RETRIES        200
#define I2S_NEXT_BUF_RETRY_DELAY_US 10

enum audio_i2s_state {
	AUDIO_I2S_STATE_UNINIT,
	AUDIO_I2S_STATE_IDLE,
	AUDIO_I2S_STATE_STARTED,
};

static enum audio_i2s_state state = AUDIO_I2S_STATE_UNINIT;

PINCTRL_DT_DEFINE(I2S_NL);

#if CONFIG_AUDIO_SAMPLE_RATE_16000_HZ
#define I2S_RATIO NRF_I2S_RATIO_384X
#elif CONFIG_AUDIO_SAMPLE_RATE_24000_HZ
#define I2S_RATIO NRF_I2S_RATIO_256X
#elif CONFIG_AUDIO_SAMPLE_RATE_48000_HZ
#define I2S_RATIO NRF_I2S_RATIO_128X
#else
#error "Current AUDIO_SAMPLE_RATE_HZ setting not supported"
#endif

static nrfx_i2s_t i2s_inst = NRFX_I2S_INSTANCE(NRF_I2S0);

static nrfx_i2s_config_t cfg = {
	/* Pins are configured by pinctrl. */
	.skip_gpio_cfg = true,
	.skip_psel_cfg = true,
	.irq_priority = DT_IRQ(I2S_NL, priority),
	.mode = NRF_I2S_MODE_MASTER,
	.format = NRF_I2S_FORMAT_I2S,
	.alignment = NRF_I2S_ALIGN_LEFT,
	.prescalers =
		{
			.ratio = I2S_RATIO,
			.mck_setup = 0x66666000,
			.enable_bypass = false,
		},
#if (CONFIG_AUDIO_BIT_DEPTH_16)
	.sample_width = NRF_I2S_SWIDTH_16BIT,
#elif (CONFIG_AUDIO_BIT_DEPTH_32)
	.sample_width = NRF_I2S_SWIDTH_32BIT,
#else
#error Invalid bit depth selected
#endif /* (CONFIG_AUDIO_BIT_DEPTH_16) */
	.channels = NRF_I2S_CHANNELS_STEREO,
	.clksrc = NRF_I2S_CLKSRC_ACLK,
};

static i2s_blk_comp_callback_t i2s_blk_comp_callback;

static void i2s_comp_handler(nrfx_i2s_buffers_t const *released_bufs, uint32_t status)
{
	if ((status == NRFX_I2S_STATUS_NEXT_BUFFERS_NEEDED) && released_bufs &&
	    i2s_blk_comp_callback && (released_bufs->p_rx_buffer || released_bufs->p_tx_buffer)) {
		i2s_blk_comp_callback(audio_sync_timer_frame_start_capture_get(),
				      released_bufs->p_rx_buffer, released_bufs->p_tx_buffer);
	}
}

void audio_i2s_set_next_buf(const uint8_t *tx_buf, uint32_t *rx_buf)
{
	__ASSERT_NO_MSG(state == AUDIO_I2S_STATE_STARTED);
	if (IS_ENABLED(CONFIG_STREAM_BIDIRECTIONAL) || IS_ENABLED(CONFIG_AUDIO_GATEWAY)) {
		__ASSERT_NO_MSG(rx_buf != NULL);
	}

	if (IS_ENABLED(CONFIG_STREAM_BIDIRECTIONAL) || IS_ENABLED(CONFIG_AUDIO_HEADSET)) {
		__ASSERT_NO_MSG(tx_buf != NULL);
	}

	const nrfx_i2s_buffers_t i2s_buf = {.p_rx_buffer = rx_buf,
					    .p_tx_buffer = (uint32_t *)tx_buf,
					    .buffer_size = I2S_SAMPLES_NUM};

	int ret;

	/*
	 * nrfx_i2s_next_buffers_set() rejects the buffer with -EINPROGRESS until
	 * the peripheral has raised its first "next buffers needed" event. The
	 * very first call - made by audio_datapath_i2s_start() immediately after
	 * nrfx_i2s_start() - can land inside that window, so retry briefly.
	 *
	 * Dropping that buffer leaves nrfx single-buffered: the peripheral then
	 * replays the block it already holds instead of advancing, which is
	 * audible as a continuous ~1 kHz buzz with no audio. The failure is
	 * otherwise silent because CONFIG_ASSERT is disabled in this app, so the
	 * return value is also logged below rather than only asserted on.
	 *
	 * Calls from the block-complete callback are always inside the window and
	 * succeed on the first attempt, so the retry never runs in ISR context.
	 */
	for (int i = 0; i < I2S_NEXT_BUF_RETRIES; i++) {
		ret = nrfx_i2s_next_buffers_set(&i2s_inst, &i2s_buf);
		if (ret != -EINPROGRESS) {
			break;
		}
		k_busy_wait(I2S_NEXT_BUF_RETRY_DELAY_US);
	}

	__ASSERT_NO_MSG(ret == 0);

	if (ret != 0) {
		LOG_ERR("nrfx_i2s_next_buffers_set failed: %d", ret);
	}
}

void audio_i2s_start(const uint8_t *tx_buf, uint32_t *rx_buf)
{
	__ASSERT_NO_MSG(state == AUDIO_I2S_STATE_IDLE);
	if (IS_ENABLED(CONFIG_STREAM_BIDIRECTIONAL) || IS_ENABLED(CONFIG_AUDIO_GATEWAY)) {
		__ASSERT_NO_MSG(rx_buf != NULL);
	}

	if (IS_ENABLED(CONFIG_STREAM_BIDIRECTIONAL) || IS_ENABLED(CONFIG_AUDIO_HEADSET)) {
		__ASSERT_NO_MSG(tx_buf != NULL);
	}

	const nrfx_i2s_buffers_t i2s_buf = {.p_rx_buffer = rx_buf,
					    .p_tx_buffer = (uint32_t *)tx_buf,
					    .buffer_size = I2S_SAMPLES_NUM};

	int ret;

	/* Buffer size in 32-bit words */
	ret = nrfx_i2s_start(&i2s_inst, &i2s_buf, 0);
	__ASSERT_NO_MSG(ret == 0);

	if (ret != 0) {
		LOG_ERR("nrfx_i2s_start failed: %d", ret);
	}

	state = AUDIO_I2S_STATE_STARTED;
}

void audio_i2s_stop(void)
{
	__ASSERT_NO_MSG(state == AUDIO_I2S_STATE_STARTED);

	nrfx_i2s_stop(&i2s_inst);

	state = AUDIO_I2S_STATE_IDLE;
}

void audio_i2s_blk_comp_cb_register(i2s_blk_comp_callback_t blk_comp_callback)
{
	i2s_blk_comp_callback = blk_comp_callback;
}

void audio_i2s_init(void)
{
	__ASSERT_NO_MSG(state == AUDIO_I2S_STATE_UNINIT);

	int ret;

	nrfx_clock_hfclkaudio_config_set(HFCLKAUDIO_12_288_MHZ);

	nrf_clock_event_clear(NRF_CLOCK, NRF_CLOCK_EVENT_HFCLKAUDIOSTARTED);
	nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_HFCLKAUDIOSTART);

	/* Wait for ACLK (HFCLKAUDIO PLL) to lock before configuring I2S, which
	 * derives MCK/LRCK from it. Starting I2S against an unlocked PLL clocks
	 * the codec at the wrong rate and produces continuous periodic noise.
	 */
	for (int i = 0; i < ACLK_START_TIMEOUT_MS; i++) {
		if (nrf_clock_event_check(NRF_CLOCK, NRF_CLOCK_EVENT_HFCLKAUDIOSTARTED)) {
			break;
		}
		k_sleep(K_MSEC(1));
	}

	if (!nrf_clock_event_check(NRF_CLOCK, NRF_CLOCK_EVENT_HFCLKAUDIOSTARTED)) {
		LOG_ERR("HFCLKAUDIO did not start within %d ms - audio will be distorted",
			ACLK_START_TIMEOUT_MS);
	}

	ret = pinctrl_apply_state(PINCTRL_DT_DEV_CONFIG_GET(I2S_NL), PINCTRL_STATE_DEFAULT);
	__ASSERT_NO_MSG(ret == 0);

	IRQ_CONNECT(DT_IRQN(I2S_NL), DT_IRQ(I2S_NL, priority), nrfx_i2s_irq_handler, &i2s_inst, 0);
	irq_enable(DT_IRQN(I2S_NL));

	ret = nrfx_i2s_init(&i2s_inst, &cfg, i2s_comp_handler);
	__ASSERT_NO_MSG(ret == 0);

	if (ret != 0) {
		LOG_ERR("nrfx_i2s_init failed: %d", ret);
	}

	state = AUDIO_I2S_STATE_IDLE;
}
