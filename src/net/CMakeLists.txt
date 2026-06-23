#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

# net_event_mgmt.c retired in Step 3.3 — zego/network brick owns net_mgmt callbacks.
# The app's net_event_app.c (src/modules/network/) provides weak-hook overrides instead.
# Add socket utilities
target_sources_ifdef(CONFIG_SOCKET_UTILS app PRIVATE
                     ${CMAKE_CURRENT_SOURCE_DIR}/socket_utils.c)
# wifi_utils.c retired (Step 3.3) — zego/network brick provides these internally.
# Custom SoftAP/mode functions no longer needed; brick handles connectivity.

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
