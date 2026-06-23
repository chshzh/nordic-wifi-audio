#
# Copyright (c) 2022 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

# audio_i2s.c uses nrfx_i2s and NRF_I2S0 which are only available on nRF5340
target_sources_ifdef(CONFIG_NRFX_I2S app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/audio_i2s.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/audio_usb.c)
# button_handler.c and led.c retired in Step 3.5 — zego/button and zego/led bricks replace them
# audio_sync_timer.c uses NRF_RTC0 and RTC0_IRQn; only compile on SoCs with RTC
target_sources_ifdef(CONFIG_AUDIO_SYNC_TIMER_USES_RTC app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/audio_sync_timer.c)

target_sources_ifdef(CONFIG_NRF5340_AUDIO_CS47L63_DRIVER app PRIVATE
                     ${CMAKE_CURRENT_SOURCE_DIR}/hw_codec.c)
target_sources_ifdef(CONFIG_NRF5340_AUDIO_POWER_MEASUREMENT app PRIVATE
                     ${CMAKE_CURRENT_SOURCE_DIR}/power_meas.c)
target_sources_ifdef(CONFIG_NRF5340_AUDIO_SD_CARD_MODULE app PRIVATE
                     ${CMAKE_CURRENT_SOURCE_DIR}/sd_card.c)
target_sources_ifdef(CONFIG_SD_CARD_PLAYBACK app PRIVATE
                     ${CMAKE_CURRENT_SOURCE_DIR}/sd_card_playback.c)
