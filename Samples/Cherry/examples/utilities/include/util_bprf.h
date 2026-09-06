/*
 * Public header for definition of HPRF settings.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include <cherry/cherry_fira.h>

#pragma once

static const struct cherry_fira_phy_params bprf_settings[] = {
	CHERRY_FIRA_PHY_PARAMS(SP0, BPRF_62_4M, 0, 64, 0, NA, 6_81M),
	CHERRY_FIRA_PHY_PARAMS(SP0, BPRF_62_4M, 2, 64, 0, NA, 6_81M),
	CHERRY_FIRA_PHY_PARAMS(SP1, BPRF_62_4M, 2, 64, 1, 64, 6_81M),
	CHERRY_FIRA_PHY_PARAMS(SP3, BPRF_62_4M, 2, 64, 1, 64, NA),
	CHERRY_FIRA_PHY_PARAMS(SP1, BPRF_62_4M, 0, 64, 1, 64, 6_81M),
	CHERRY_FIRA_PHY_PARAMS(SP3, BPRF_62_4M, 0, 64, 1, 64, NA),
};

#define MAX_BPRF_SET_ID sizeof(bprf_settings) / sizeof(bprf_settings[0])
