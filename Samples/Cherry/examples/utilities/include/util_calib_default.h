/*
 * Header file for default chip calibration.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <cherry/cherry.h>
#include <stdbool.h>

#define DEFAULT_ANTENNA_SET 0
#define RADAR_ANTENNA_SET 3

#define ANT_PATH_IDX_UNSET -1

static const uint8_t util_calib_radar_default_tx_ant_paths[] = {
	ANT_PATH_IDX_UNSET, ANT_PATH_IDX_UNSET
};

/* Default keys to check. Order is important to define other calib structure.
 * The first 8 keys are used to check if RRAM calibration is loaded.
 * The last 4 keys are used to check if more than RRAM calibration is loaded (LUT).
 */
static const struct cherry_calib_key util_calib_default_keys[] = {
	CHERRY_CALIB_UINT8("ant0.port", 0xF),
	CHERRY_CALIB_UINT8("ant1.port", 0xF),
	CHERRY_CALIB_UINT8("ant2.port", 0xF),
	CHERRY_CALIB_UINT8("ant3.port", 0xF),
	CHERRY_CALIB_UINT8("ant4.port", 0xF),
	CHERRY_CALIB_UINT8("ant5.port", 0xF),
	CHERRY_CALIB_UINT8("ant_set0.nb_rx_ants", 0),
	CHERRY_CALIB_UINT8("ant_set1.nb_rx_ants", 0),
	CHERRY_CALIB_UINT8("ant_pair0.ch5.pdoa.lut_id", -1),
	CHERRY_CALIB_UINT8("ant_pair0.ch9.pdoa.lut_id", -1),
	CHERRY_CALIB_UINT8("ant_pair1.ch5.pdoa.lut_id", -1),
	CHERRY_CALIB_UINT8("ant_pair1.ch9.pdoa.lut_id", -1),
};

/* Cablib structure with every default keys */
static const struct cherry_calib util_calib_default = {
	.n_keys = sizeof(util_calib_default_keys) /
		  sizeof(util_calib_default_keys[0]),
	.keys = util_calib_default_keys,
};

/* Calib structure with default keys to check if RRAM calibration is loaded. */
static const struct cherry_calib util_calib_default_ant = {
	.n_keys = (sizeof(util_calib_default_keys) /
		   sizeof(util_calib_default_keys[0])) -
		  4, /* Only the first 8 keys. */
	.keys = util_calib_default_keys,
};

/* Calib structure with default keys to check if more than RRAM calibration is loaded. */
static const struct cherry_calib util_calib_default_lut = {
	.n_keys = (sizeof(util_calib_default_keys) /
		   sizeof(util_calib_default_keys[0])) -
		  8, /* only the last 4 keys. */
	.keys = &util_calib_default_keys[8],
};

bool check_calib(struct cherry_calib *calib, bool *has_ant_set,
		 bool *has_lut_set);
