#
# Copyright (c) 2022 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

target_sources(app PRIVATE
	       ${CMAKE_CURRENT_SOURCE_DIR}/channel_assignment.c
	       ${CMAKE_CURRENT_SOURCE_DIR}/error_handler.c
)

# uicr.c uses nrfx_nvmc which is only available on nRF53 (NVMC-based UICR).
# Other SoCs (e.g. nRF54LM20A with RRAMC) use a no-op stub.
target_sources_ifdef(CONFIG_SOC_SERIES_NRF53 app PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/uicr.c)
if(NOT CONFIG_SOC_SERIES_NRF53)
  target_sources(app PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/uicr_stub.c)
endif()
target_sources_ifdef(CONFIG_SOC_SERIES_NRF53 app PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/nrf5340_audio_dk.c)
target_sources_ifdef(CONFIG_BOARD_NRF5340_AUDIO_DK_NRF5340_CPUAPP app PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/board_version.c)

# nRF54L-series init: provides nrf5340_audio_dk_init() without NVMC/RTC0/clock-divider
if(NOT CONFIG_SOC_SERIES_NRF53)
  target_sources(app PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/nrf54l_init.c)
endif()
