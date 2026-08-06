#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

# Register zego bricks: button, led, wifi, network, memonitor, ux, factory_reset.
# Use ZEPHYR_BASE (set by zephyr-env.sh) to locate the NCS workspace root.
# This is robust for both normal checkouts and git worktrees (which have a
# different CMAKE_CURRENT_SOURCE_DIR depth).
get_filename_component(_NCS_ROOT "$ENV{ZEPHYR_BASE}/.." ABSOLUTE)
get_filename_component(ZEGO_BUTTON_DIR    "${_NCS_ROOT}/zego/bricks/button"    REALPATH)
get_filename_component(ZEGO_LED_DIR       "${_NCS_ROOT}/zego/bricks/led"       REALPATH)
get_filename_component(ZEGO_WIFI_DIR      "${_NCS_ROOT}/zego/bricks/wifi"      REALPATH)
get_filename_component(ZEGO_NETWORK_DIR   "${_NCS_ROOT}/zego/bricks/network"   REALPATH)
get_filename_component(ZEGO_MEMONITOR_DIR "${_NCS_ROOT}/zego/bricks/memonitor" REALPATH)
get_filename_component(ZEGO_UX_DIR        "${_NCS_ROOT}/zego/bricks/ux"        REALPATH)
get_filename_component(ZEGO_FACTORY_RESET_DIR "${_NCS_ROOT}/zego/bricks/factory_reset" REALPATH)
list(APPEND EXTRA_ZEPHYR_MODULES
  ${ZEGO_BUTTON_DIR}
  ${ZEGO_LED_DIR}
  ${ZEGO_WIFI_DIR}
  ${ZEGO_NETWORK_DIR}
  ${ZEGO_MEMONITOR_DIR}
  ${ZEGO_UX_DIR}
  ${ZEGO_FACTORY_RESET_DIR}
)

# Flag which defines whether application is compiled as gateway/dongle or headset
add_compile_definitions(HEADSET=1)
add_compile_definitions(GATEWAY=2)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(wifi_audio_transceiver)

# Override the banner app name set by the zego wifi brick (which defaults to
# the source folder name "nordic-wifi-audio") with a role-specific name.
# The brick sets it with zephyr_compile_definitions(), i.e. on the global
# zephyr_interface target, so the existing definition has to be filtered out
# there before appending ours - otherwise the macro is defined twice.
if(CONFIG_AUDIO_GATEWAY)
        set(_zego_wifi_banner "nordic-wifi-audio(Gateway)")
elseif(CONFIG_AUDIO_HEADSET)
        set(_zego_wifi_banner "nordic-wifi-audio(Headset)")
endif()
if(DEFINED _zego_wifi_banner)
        get_target_property(_zephyr_defs zephyr_interface INTERFACE_COMPILE_DEFINITIONS)
        if(_zephyr_defs)
                list(FILTER _zephyr_defs EXCLUDE REGEX "^ZEGO_BANNER_APP_NAME=")
                list(APPEND _zephyr_defs "ZEGO_BANNER_APP_NAME=\"${_zego_wifi_banner}\"")
                set_target_properties(zephyr_interface PROPERTIES
                        INTERFACE_COMPILE_DEFINITIONS "${_zephyr_defs}")
        endif()
endif()

string(TIMESTAMP NRF5340_AUDIO_CORE_APP_COMP_DATE "%a %b %d %H:%M:%S %Y")
message(NRF5340_WIFI_AUDIO_COMP_DATE="${NRF5340_AUDIO_CORE_APP_COMP_DATE}")

# Generate fw_info_app.c
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/utils/fw_info_app.c.in"
               "${CMAKE_BINARY_DIR}/fw_info_app.c"
               @ONLY)

# Target sources below are specific to the nRF5340 Audio DK HW
target_sources(app PRIVATE
    ${CMAKE_BINARY_DIR}/fw_info_app.c
    )
if (CONFIG_AUDIO_HEADSET)
	add_subdirectory(wifi_audio_headset)
endif()

if (CONFIG_AUDIO_GATEWAY)
	add_subdirectory(wifi_audio_gateway)
endif()
# target_include_directories(app PUBLIC
zephyr_library_include_directories(
        include
        src
        src/audio
        src/modules/network
        src/drivers
        src/modules
        src/modules/audio_led
        src/utils
        src/utils/macros
        src/debug
        # ${ZEPHYR_NRF_MODULE_DIR}/boards/arm/nrf5340_audio_dk_nrf5340
        )

zephyr_library_include_directories(app PRIVATE
${ZEPHYR_NRF_MODULE_DIR}/boards/arm/nrf5340_audio_dk_nrf5340)

add_subdirectory(src/audio)
add_subdirectory(src/drivers)
add_subdirectory(src/modules)
add_subdirectory(src/utils)

# zego-brick app modules (added in refactor Step 3.2)
# network module: zego/network weak-hook overrides (net_event_app.c) + UDP transport (socket_utils.c)
add_subdirectory(src/modules/network)
add_subdirectory(src/modules/ux)
add_subdirectory(src/modules/audio_led)

## Cirrus Logic
if (CONFIG_HW_CODEC_CIRRUS_LOGIC)
    if (ZEPHYR_CIRRUS_LOGIC_MODULE_DIR)
        add_subdirectory(${ZEPHYR_CIRRUS_LOGIC_MODULE_DIR} cirrus_logic_bin_dir)
    else()
        message(FATAL_ERROR "Cirrus Logic/sdk-mcu-drivers repository not found\n")
    endif()
endif()

# build counter
# Set a cached variable for the build count (default is 0)
if(NOT DEFINED WIFI_AUDIO_CMAKE_BUILD_COUNT)
    set(WIFI_AUDIO_CMAKE_BUILD_COUNT 0 CACHE STRING "Number of times the project has been built")
endif()
# Increment the build count
math(EXPR WIFI_AUDIO_CMAKE_BUILD_COUNT "${WIFI_AUDIO_CMAKE_BUILD_COUNT} + 1")
# Update the cached value
set(WIFI_AUDIO_CMAKE_BUILD_COUNT ${WIFI_AUDIO_CMAKE_BUILD_COUNT} CACHE STRING "Number of times the project has been built" FORCE)
# Print the current build count to the console
message(STATUS "Build count: ${WIFI_AUDIO_CMAKE_BUILD_COUNT}")