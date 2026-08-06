# Copyright (c) 2026 Nordic Semiconductor ASA
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

# Built straight into `app` (unlike src/modules/ux's separate zephyr_library
# workaround, which a real build showed never gets linked into the final
# image: zephyr_library_named() in application-mode CMake isn't added to
# Zephyr's whole-archive list, so its .a is compiled but never consumed).
# The `app` target's private include path puts src/modules (which holds the
# legacy nRF5340 Audio DK led.h) ahead of zego/bricks/led/src, so a plain
# <led.h> here would resolve to the wrong header. Side-stepped with a
# generated wrapper header (zego_led_include.h) that #includes the brick's
# header via its CMake-resolved absolute path, instead of relying on -I
# search order.
configure_file(${CMAKE_CURRENT_LIST_DIR}/zego_led_include.h.in
                ${CMAKE_CURRENT_BINARY_DIR}/zego_led_include.h @ONLY)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/audio_led.c)
