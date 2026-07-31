# Copyright (c) 2026 Nordic Semiconductor ASA
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

# Built as its own zephyr_library rather than into `app`: this file needs the
# zego LED brick's <led.h>, and the `app` target's private include path puts
# src/modules (which holds the legacy nRF5340 Audio DK led.h) ahead of
# zego/bricks/led/src. A separate library sees only the global brick include
# dirs, so <led.h> and <ux.h> resolve to the zego headers.
if(CONFIG_ZEGO_UX)
	zephyr_library_named(app_ux)
	zephyr_library_sources(${CMAKE_CURRENT_LIST_DIR}/ux.c)
	# Zephyr wires this dependency up for every library registered before
	# find_package(Zephyr) returns; libraries the application creates later
	# have to ask for it themselves, or the build races the generated
	# headers (e.g. zephyr/heap_constants.h, pulled in by kernel.h).
	add_dependencies(app_ux zephyr_generated_headers)
endif()
