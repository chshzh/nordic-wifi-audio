#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

# Application network module:
#   net_event_app.c — strong overrides of the zego/network brick weak hooks
#   socket_utils.c  — UDP socket transport (server/client) + mDNS discovery
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/net_event_app.c)
target_sources_ifdef(CONFIG_SOCKET_UTILS app PRIVATE
                     ${CMAKE_CURRENT_SOURCE_DIR}/socket_utils.c)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
