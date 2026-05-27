/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
 * Stub UICR implementation for SoCs that do not have NVMC-based UICR
 * (e.g. nRF54LM20A which uses RRAMC). Channel assignment via UICR is
 * nRF5340 Audio DK-specific; on other platforms it is not used.
 */

#include "uicr.h"

uint8_t uicr_channel_get(void)
{
	return 0;
}

int uicr_channel_set(uint8_t channel)
{
	(void)channel;
	return 0;
}

uint64_t uicr_snr_get(void)
{
	return 0;
}
