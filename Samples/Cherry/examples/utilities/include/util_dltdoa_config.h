/*
 * Public header for definition of FiRa DLTDOA session
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <cherry/cherry_fira.h>

#define MAC_ADDRESS_ANCHOR1 12
#define MAC_ADDRESS_ANCHOR2 13
#define MAC_ADDRESS_ANCHOR3 14
#define MAC_ADDRESS_ANCHOR4 15
#define MAC_ADDRESS_ANCHOR5 16
#define MAC_ADDRESS_ANCHOR6 17

extern const struct cherry_fira_anchor_round_config round_conf_7_clusters[6][7];

/* Struct with all required argument when using a Pre defined round configuration. */
struct dl_tdoa_config_light {
	uint16_t device_mac_address;
	const struct cherry_fira_anchor_round_config *round_config;
	struct cherry_fira_anchor_round_config *round_config_custom;
	int n_rounds;
};

extern const struct dl_tdoa_config_light dltdoa_mono_configs[6];

extern const struct dl_tdoa_config_light dltdoa_multi_configs[6];
