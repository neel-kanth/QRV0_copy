/*
 * Header file for chip calibration.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <cherry/cherry.h>
#include <stdbool.h>

static const uint8_t util_calib_qm35822_ant_pair_0_ant_path[] = { 0x03, 0x02 };
static const uint8_t util_calib_qm35822_ant_set0_tx_ant_paths[] = { 0x01,
								    0x00 };
static const uint8_t util_calib_qm35822_ant_set0_rx_ants[] = { 0x00, 0x00,
							       0x00 };
static const uint8_t util_calib_qm35822_ant_set3_tx_ant_paths[] = { 0x00,
								    0xff };
static const uint8_t util_calib_qm35822_ant_set3_rx_ants[] = { 0x03, 0xff,
							       0xff };

/* Dual RX, PDOA, ANT1_RXB_ANT2_RXA, support for TX auto selection TX primary is ANT1, secondary is ANT2*/

static const struct cherry_calib_key util_calib_qm35822_keys[] = {
	/* ANT0 = TBD */
	CHERRY_CALIB_UINT8("ant0.transceiver", 0),
	CHERRY_CALIB_UINT8("ant0.port", 1),
	CHERRY_CALIB_UINT8("ant0.ext_sw_cfg", 0),
	CHERRY_CALIB_UINT8("ant0.lna", 0),
	/* ANT1 = TBD */
	CHERRY_CALIB_UINT8("ant1.transceiver", 0),
	CHERRY_CALIB_UINT8("ant1.port", 2),
	CHERRY_CALIB_UINT8("ant1.ext_sw_cfg", 0),
	CHERRY_CALIB_UINT8("ant1.lna", 0),
	/* ANT2 = TBD */
	CHERRY_CALIB_UINT8("ant2.transceiver", 2),
	CHERRY_CALIB_UINT8("ant2.port", 1),
	CHERRY_CALIB_UINT8("ant2.ext_sw_cfg", 0),
	CHERRY_CALIB_UINT8("ant2.lna", 1),
	/* ANT3 = TBD */
	CHERRY_CALIB_UINT8("ant3.transceiver", 1),
	CHERRY_CALIB_UINT8("ant3.port", 2),
	CHERRY_CALIB_UINT8("ant3.ext_sw_cfg", 0),
	CHERRY_CALIB_UINT8("ant3.lna", 1),
	/* Ant Pair 0 to measure Azimuth between ant 2 and ant1 */
	CHERRY_CALIB_UINT8("ant_pair0.axis", 0),
	CHERRY_CALIB_NUMBER_ARRAY_1D("ant_pair0.ant_paths",
				     util_calib_qm35822_ant_pair_0_ant_path),
	/* Ant Set0 */
	CHERRY_CALIB_UINT8("ant_set0.nb_rx_ants", 1),
	CHERRY_CALIB_BOOL("ant_set0.rx_ants_are_pairs", true),
	CHERRY_CALIB_NUMBER_ARRAY_1D("ant_set0.tx_ant_paths",
				     util_calib_qm35822_ant_set0_tx_ant_paths),
	CHERRY_CALIB_NUMBER_ARRAY_1D("ant_set0.rx_ants",
				     util_calib_qm35822_ant_set0_rx_ants),
	/* Ant Set1 */
	CHERRY_CALIB_UINT8("ant_set3.nb_rx_ants", 1),
	CHERRY_CALIB_BOOL("ant_set3.rx_ants_are_pairs", false),
	CHERRY_CALIB_NUMBER_ARRAY_1D("ant_set3.tx_ant_paths",
				     util_calib_qm35822_ant_set3_tx_ant_paths),
	CHERRY_CALIB_NUMBER_ARRAY_1D("ant_set3.rx_ants",
				     util_calib_qm35822_ant_set3_rx_ants),
	CHERRY_CALIB_UINT8("ant_set3.tx_power_control", 0x80),
};

static const struct cherry_calib util_calib_qm35822 = {
	.n_keys = sizeof(util_calib_qm35822_keys) /
		  sizeof(util_calib_qm35822_keys[0]),
	.keys = util_calib_qm35822_keys,
};
