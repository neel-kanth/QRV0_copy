/*
 * Header file for 360 degree AOA calibration.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <cherry/cherry.h>

/* Antenna paths and configurations */
static const uint8_t util_calib_360_aoa_ant_pair_0_ant_path[] = { 0x02, 0x01 };
static const uint8_t util_calib_360_aoa_ant_pair_1_ant_path[] = { 0x03, 0x01 };
static const uint8_t util_calib_360_aoa_ant_pair_2_ant_path[] = { 0x03, 0x02 };
static const uint8_t util_calib_360_aoa_ant_set1_tx_ant_paths[] = { 0x00,
								    0xFF };
static const uint8_t util_calib_360_aoa_ant_set1_rx_ants[] = { 0x00, 0x01,
							       0x02 };

/* PDOA LUT data from fira_360_aoa_calib.json */
static const uint16_t util_calib_360_aoa_pdoa_lut0_ch5[][2] = {
	{ 0xEC05, 0xF370 }, /* original: -2.496476, -1.570796 */
	{ 0xEBD8, 0xF446 }, /* original: -2.520544, -1.466077 */
	{ 0xEBEB, 0xF51C }, /* original: -2.500504, -1.361357 */
	{ 0xEC70, 0xF5F3 }, /* original: -2.417688, -1.256637 */
	{ 0xED4D, 0xF6C9 }, /* original: -2.289830, -1.151917 */
	{ 0xEE59, 0xF7A0 }, /* original: -2.107628, -1.047198 */
	{ 0xEF1B, 0xF876 }, /* original: -1.915842, -0.942478 */
	{ 0xF09A, 0xF94D }, /* original: -1.733579, -0.837758 */
	{ 0xF19A, 0xFA23 }, /* original: -1.561988, -0.733038 */
	{ 0xF299, 0xFAFA }, /* original: -1.386116, -0.628319 */
	{ 0xF399, 0xFBD0 }, /* original: -1.209755, -0.523599 */
	{ 0xF4B9, 0xFCA7 }, /* original: -1.016585, -0.418879 */
	{ 0xF680, 0xFD7D }, /* original: -0.806069, -0.314159 */
	{ 0xF8BC, 0xFE54 }, /* original: -0.564691, -0.209440 */
	{ 0xFB38, 0xFF2A }, /* original: -0.295829, -0.104720 */
	{ 0x0000, 0x0000 }, /* original: 0.000000, 0.000000 */
	{ 0x024D, 0x00D6 }, /* original: 0.286931, 0.104720 */
	{ 0x04C3, 0x01AC }, /* original: 0.593018, 0.209440 */
	{ 0x0731, 0x0283 }, /* original: 0.896942, 0.314159 */
	{ 0x0987, 0x0359 }, /* original: 1.185644, 0.418879 */
	{ 0x0BC3, 0x0430 }, /* original: 1.450332, 0.523599 */
	{ 0x0D37, 0x0506 }, /* original: 1.649755, 0.628319 */
	{ 0x0E50, 0x05DD }, /* original: 1.784926, 0.733038 */
	{ 0x0ED8, 0x06B3 }, /* original: 1.857456, 0.837758 */
	{ 0x0EF4, 0x078A }, /* original: 1.877550, 0.942478 */
	{ 0x0EEC, 0x0860 }, /* original: 1.872391, 1.047198 */
	{ 0x0F13, 0x0937 }, /* original: 1.902414, 1.151917 */
	{ 0x0FC0, 0x0A0D }, /* original: 1.964032, 1.256637 */
	{ 0x1082, 0x0AE4 }, /* original: 2.074064, 1.361357 */
	{ 0x1196, 0x0BBA }, /* original: 2.225441, 1.466077 */
	{ 0x12FD, 0x0C90 }, /* original: 2.393872, 1.570796 */
};

static const uint16_t util_calib_360_aoa_pdoa_lut1_ch9[][2] = {
	{ 0xE6FC, 0xF370 }, /* original: -3.135213, -1.570796 */
	{ 0xE794, 0xF446 }, /* original: -3.069086, -1.466077 */
	{ 0xE7F2, 0xF51C }, /* original: -2.964733, -1.361357 */
	{ 0xE883, 0xF5F3 }, /* original: -2.823520, -1.256637 */
	{ 0xE934, 0xF6C9 }, /* original: -2.653307, -1.151917 */
	{ 0xEA00, 0xF7A0 }, /* original: -2.468820, -1.047198 */
	{ 0xEADB, 0xF876 }, /* original: -2.278769, -0.942478 */
	{ 0xEBC5, 0xF94D }, /* original: -2.092377, -0.837758 */
	{ 0xECB8, 0xFA23 }, /* original: -1.898693, -0.733038 */
	{ 0xEDBA, 0xFAFA }, /* original: -1.700335, -0.628319 */
	{ 0xEECC, 0xFBD0 }, /* original: -1.492313, -0.523599 */
	{ 0xEF87, 0xFCA7 }, /* original: -1.255545, -0.418879 */
	{ 0xF11B, 0xFD7D }, /* original: -0.982438, -0.314159 */
	{ 0xF2F1, 0xFE54 }, /* original: -0.673336, -0.209440 */
	{ 0xF514, 0xFF2A }, /* original: -0.347133, -0.104720 */
	{ 0x0000, 0x0000 }, /* original: 0.000000, 0.000000 */
	{ 0x0296, 0x00D6 }, /* original: 0.321383, 0.104720 */
	{ 0x0507, 0x01AC }, /* original: 0.625997, 0.209440 */
	{ 0x074F, 0x0283 }, /* original: 0.910970, 0.314159 */
	{ 0x097E, 0x0359 }, /* original: 1.183374, 0.418879 */
	{ 0x0B9B, 0x0430 }, /* original: 1.441669, 0.523599 */
	{ 0x0D69, 0x0506 }, /* original: 1.674572, 0.628319 */
	{ 0x0F36, 0x05DD }, /* original: 1.897394, 0.733038 */
	{ 0x10DC, 0x06B3 }, /* original: 2.113799, 0.837758 */
	{ 0x1292, 0x078A }, /* original: 2.316167, 0.942478 */
	{ 0x1407, 0x0860 }, /* original: 2.499193, 1.047198 */
	{ 0x1556, 0x0937 }, /* original: 2.666819, 1.151917 */
	{ 0x168A, 0x0A0D }, /* original: 2.815830, 1.256637 */
	{ 0x17C0, 0x0AE4 }, /* original: 2.964775, 1.361357 */
	{ 0x186C, 0x0BBA }, /* original: 3.083037, 1.466077 */
	{ 0x19FF, 0x0C90 }, /* original: 3.168720, 1.570796 */
};

static const struct cherry_calib_key util_calib_360_aoa_keys[] = {
	/* Global Calibration. */
	CHERRY_CALIB_UINT8("xtal_trim", 0x28),
	CHERRY_CALIB_UINT8("wifi_coex_mode", 0x0),
	CHERRY_CALIB_UINT8("ch5.wifi_coex_enabled", 0x0),
	CHERRY_CALIB_UINT8("ch9.wifi_coex_enabled", 0x0),
	CHERRY_CALIB_UINT8("alternate_pulse_shape", 0x0),
	CHERRY_CALIB_UINT8("ch5.pll_locking_code", 0x1),
	CHERRY_CALIB_UINT8("ch9.pll_locking_code", 0x1),

	/* PDOA LUT Configuration. */
	CHERRY_CALIB_NUMBER_ARRAY_2D("pdoa_lut0.data",
				     util_calib_360_aoa_pdoa_lut0_ch5),
	CHERRY_CALIB_NUMBER_ARRAY_2D("pdoa_lut1.data",
				     util_calib_360_aoa_pdoa_lut1_ch9),

	/* ant0 */
	CHERRY_CALIB_UINT32("ant0.ch5.ref_frame0.tx_power_index", 0x20202020),
	CHERRY_CALIB_UINT32("ant0.ch9.ref_frame0.tx_power_index", 0x30303030),
	CHERRY_CALIB_UINT32("ant0.ch5.ant_delay", 16410),
	CHERRY_CALIB_UINT32("ant0.ch9.ant_delay", 16400),
	CHERRY_CALIB_UINT8("ant0.transceiver", 0x00),
	CHERRY_CALIB_UINT8("ant0.port", 0x02),
	CHERRY_CALIB_UINT8("ant0.ext_sw_cfg", 0x00),
	CHERRY_CALIB_UINT8("ant0.lna", 1),

	/* ant1 */
	CHERRY_CALIB_UINT8("ant1.transceiver", 0x02),
	CHERRY_CALIB_UINT8("ant1.port", 0x02),
	CHERRY_CALIB_UINT8("ant1.ext_sw_cfg", 0x00),
	CHERRY_CALIB_UINT8("ant1.lna", 1),
	CHERRY_CALIB_UINT32("ant1.ch5.ant_delay", 16410),
	CHERRY_CALIB_UINT32("ant1.ch9.ant_delay", 16400),

	/* ant2 */
	CHERRY_CALIB_UINT8("ant2.transceiver", 0x01),
	CHERRY_CALIB_UINT8("ant2.port", 0x03),
	CHERRY_CALIB_UINT8("ant2.ext_sw_cfg", 0x00),
	CHERRY_CALIB_UINT8("ant2.lna", 1),
	CHERRY_CALIB_UINT32("ant2.ch5.ant_delay", 16410),
	CHERRY_CALIB_UINT32("ant2.ch9.ant_delay", 16400),

	/* ant3 */
	CHERRY_CALIB_UINT8("ant3.transceiver", 0x01),
	CHERRY_CALIB_UINT8("ant3.port", 0x04),
	CHERRY_CALIB_UINT8("ant3.ext_sw_cfg", 0x00),
	CHERRY_CALIB_UINT8("ant3.lna", 1),
	CHERRY_CALIB_UINT32("ant3.ch5.ant_delay", 16410),
	CHERRY_CALIB_UINT32("ant3.ch9.ant_delay", 16400),

	/* Debug settings */
	CHERRY_CALIB_UINT32("debug.tx_power", 0xFEFEFEFE),
	CHERRY_CALIB_UINT8("debug.pa_enabled", 0),

	/* Ant Pair 0 - Horizontal pair (ant2-ant1) */
	CHERRY_CALIB_UINT8("ant_pair0.axis", 0x00),
	CHERRY_CALIB_NUMBER_ARRAY_1D("ant_pair0.ant_paths",
				     util_calib_360_aoa_ant_pair_0_ant_path),
	CHERRY_CALIB_INT16("ant_pair0.ch5.pdoa.offset", 0),
	CHERRY_CALIB_INT16("ant_pair0.ch9.pdoa.offset", 0),
	CHERRY_CALIB_UINT8("ant_pair0.ch5.pdoa.lut_id", 0x0),
	CHERRY_CALIB_UINT8("ant_pair0.ch9.pdoa.lut_id", 0x1),

	/* Ant Pair 1 - Vertical pair (ant3-ant1) */
	CHERRY_CALIB_UINT8("ant_pair1.axis", 0x01),
	CHERRY_CALIB_NUMBER_ARRAY_1D("ant_pair1.ant_paths",
				     util_calib_360_aoa_ant_pair_1_ant_path),
	CHERRY_CALIB_INT16("ant_pair1.ch5.pdoa.offset", 0),
	CHERRY_CALIB_INT16("ant_pair1.ch9.pdoa.offset", 0),
	CHERRY_CALIB_UINT8("ant_pair1.ch5.pdoa.lut_id", 0x0),
	CHERRY_CALIB_UINT8("ant_pair1.ch9.pdoa.lut_id", 0x1),

	/* Ant Pair 2 - Diagonal pair (ant3-ant2) */
	CHERRY_CALIB_UINT8("ant_pair2.axis", 0x02),
	CHERRY_CALIB_NUMBER_ARRAY_1D("ant_pair2.ant_paths",
				     util_calib_360_aoa_ant_pair_2_ant_path),
	CHERRY_CALIB_INT16("ant_pair2.ch5.pdoa.offset", 0),
	CHERRY_CALIB_INT16("ant_pair2.ch9.pdoa.offset", 0),
	CHERRY_CALIB_UINT8("ant_pair2.ch5.pdoa.lut_id", 0x0),
	CHERRY_CALIB_UINT8("ant_pair2.ch9.pdoa.lut_id", 0x1),

	/* Ant Set 0: Main configuration for 360 degree AoA */
	CHERRY_CALIB_NUMBER_ARRAY_1D("ant_set1.tx_ant_paths",
				     util_calib_360_aoa_ant_set1_tx_ant_paths),
	CHERRY_CALIB_UINT8("ant_set1.nb_rx_ants", 0x03),
	CHERRY_CALIB_BOOL("ant_set1.rx_ants_are_pairs", true),
	CHERRY_CALIB_NUMBER_ARRAY_1D("ant_set1.rx_ants",
				     util_calib_360_aoa_ant_set1_rx_ants),
	CHERRY_CALIB_UINT8("ant_set1.tx_power_control", 0x03),
};

static const struct cherry_calib util_calib_360_aoa = {
	.n_keys = sizeof(util_calib_360_aoa_keys) /
		  sizeof(util_calib_360_aoa_keys[0]),
	.keys = util_calib_360_aoa_keys,
};
