# Copyright (c) 2026 Nordic Semiconductor ASA
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

if(CONFIG_APP_UX_MODULE)
	target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/ux.c)
endif()
