/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include <gtest/gtest.h>

extern "C" {
#include "cherry/cherry.h"
#include "cherry/cherry_calib_folder.h"
}

#include "mock_qmalloc.hh"

static void compare(const struct cherry_calib_key &k1,
		    const struct cherry_calib_key &k2)
{
	ASSERT_STREQ(k1.name, k2.name);
	ASSERT_EQ(k1.size, k2.size);
	ASSERT_EQ(k1.type, k2.type);
	ASSERT_EQ(k1.size, k2.size);
	if (k1.type == CHERRY_CALIB_VALUE_NUMBER_ARRAY) {
		ASSERT_EQ(k1.nb_array_items, k2.nb_array_items);
	}
	if (k1.type == CHERRY_CALIB_VALUE_NUMBER) {
		ASSERT_EQ(k1.number, k2.number);
	} else {
		for (size_t i = 0; i < k1.size; ++i)
			ASSERT_EQ(((const char *)k1.data)[i],
				  ((const char *)k2.data)[i]);
	}
}

static void compare(const struct cherry_calib_key *keys1, const size_t s1,
		    const struct cherry_calib_key *keys2, const size_t s2)
{
	ASSERT_EQ(s1, s2);
	for (size_t i = 0; i < s1; ++i)
		compare(keys1[i], keys2[i]);
}

template <size_t S>
void compare(const struct cherry_calib *calib,
	     const struct cherry_calib_key (&keys)[S])
{
	compare(calib->keys, calib->n_keys, keys, S);
}

class CherryCalib : public testing::Test {
    protected:
	void SetUp() override
	{
		mock_alloc.implicit_call();
	}

	void TearDown() override
	{
	}

	static const char *config1_path;
	static const char *config_soc_path;
	static const char *config_sip_path;
	static const char *config_radar_path;
	static const uint8_t config1_ant0_ch5_ref_frame0_tx_power_index[];
	static const int16_t config1_pdoa_lut0_data[][2];

	MockQmalloc mock_alloc;
};

const char *CherryCalib::config1_path = UTEST_RESOURCES_PATH "/config1";
const char *CherryCalib::config_soc_path = UTEST_RESOURCES_PATH "/config-soc";
const char *CherryCalib::config_sip_path = UTEST_RESOURCES_PATH "/config-sip";
const char *CherryCalib::config_radar_path = UTEST_RESOURCES_PATH
	"/config-radar";
const uint8_t CherryCalib::config1_ant0_ch5_ref_frame0_tx_power_index[] = {
	70, 71, 72, 73
};
const int16_t CherryCalib::config1_pdoa_lut0_data[][2] = {
	{ 0, 1 },   { 2, 3 },	{ 4, 5 },   { 6, 7 },	{ 8, 9 },   { 10, 11 },
	{ 12, 13 }, { 14, 15 }, { 16, 17 }, { 18, 19 }, { 20, 21 }, { 22, 23 },
	{ 24, 25 }, { 26, 27 }, { 28, 29 }, { 30, 31 }, { 32, 33 }, { 34, 35 },
	{ 36, 37 }, { 38, 39 }, { 40, 41 }, { 42, 43 }, { 44, 45 }, { 46, 47 },
	{ 48, 49 }, { 50, 51 }, { 52, 53 }, { 54, 55 }, { 56, 57 }, { 58, 59 },
	{ 60, 61 },
};

TEST_F(CherryCalib, NullPath)
{
	struct cherry_calib *calib = cherry_calib_folder_load(nullptr, nullptr);
	ASSERT_EQ(calib, nullptr);
}

TEST_F(CherryCalib, WrongPath)
{
	struct cherry_calib *calib =
		cherry_calib_folder_load("/foo/bar", nullptr);
	ASSERT_EQ(calib, nullptr);
}

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

TEST_F(CherryCalib, NoCountryCode)
{
	struct cherry_calib *calib =
		cherry_calib_folder_load(config1_path, nullptr);
	ASSERT_NE(calib, nullptr);
	static const struct cherry_calib_key expected[] = {
		CHERRY_CALIB_UINT16("restricted_channels", 12),
		CHERRY_CALIB_UINT8("wifi_coex_mode", 0),
		CHERRY_CALIB_UINT8("wifi_coex_time_gap", 0),
		CHERRY_CALIB_BOOL("ch5.wifi_coex_enabled", true),
		CHERRY_CALIB_UINT8("ch5.pll_locking_code", 255),
		CHERRY_CALIB_BOOL("ch9.wifi_coex_enabled", false),
		CHERRY_CALIB_UINT8("ch9.pll_locking_code", 1),
		CHERRY_CALIB_NUMBER_ARRAY_1D(
			"ant0.ch5.ref_frame0.tx_power_index",
			config1_ant0_ch5_ref_frame0_tx_power_index),
		CHERRY_CALIB_NUMBER_ARRAY_2D("pdoa_lut0.data",
					     config1_pdoa_lut0_data),
	};
	compare(calib, expected);
	cherry_calib_destroy(calib);
}

TEST_F(CherryCalib, UnknownCountryCode)
{
	struct cherry_calib *calib =
		cherry_calib_folder_load(config1_path, "foo");
	ASSERT_NE(calib, nullptr);
	static const struct cherry_calib_key expected[] = {
		CHERRY_CALIB_UINT16("restricted_channels", 12),
		CHERRY_CALIB_UINT8("wifi_coex_mode", 0),
		CHERRY_CALIB_UINT8("wifi_coex_time_gap", 0),
		CHERRY_CALIB_BOOL("ch5.wifi_coex_enabled", true),
		CHERRY_CALIB_UINT8("ch5.pll_locking_code", 255),
		CHERRY_CALIB_BOOL("ch9.wifi_coex_enabled", false),
		CHERRY_CALIB_UINT8("ch9.pll_locking_code", 1),
		CHERRY_CALIB_NUMBER_ARRAY_1D(
			"ant0.ch5.ref_frame0.tx_power_index",
			config1_ant0_ch5_ref_frame0_tx_power_index),
		CHERRY_CALIB_NUMBER_ARRAY_2D("pdoa_lut0.data",
					     config1_pdoa_lut0_data),
	};
	compare(calib, expected);
	cherry_calib_destroy(calib);
}

TEST_F(CherryCalib, Country1)
{
	struct cherry_calib *calib =
		cherry_calib_folder_load(config1_path, "country1");
	ASSERT_NE(calib, nullptr);
	static const struct cherry_calib_key expected[] = {
		CHERRY_CALIB_UINT16("restricted_channels", 12),
		CHERRY_CALIB_UINT8("wifi_coex_mode", 0),
		CHERRY_CALIB_UINT8("wifi_coex_time_gap", 0),
		CHERRY_CALIB_BOOL("ch5.wifi_coex_enabled", true),
		CHERRY_CALIB_UINT8("ch5.pll_locking_code", 255),
		CHERRY_CALIB_BOOL("ch9.wifi_coex_enabled", false),
		CHERRY_CALIB_UINT8("ch9.pll_locking_code", 1),
		CHERRY_CALIB_NUMBER_ARRAY_1D(
			"ant0.ch5.ref_frame0.tx_power_index",
			config1_ant0_ch5_ref_frame0_tx_power_index),
		CHERRY_CALIB_NUMBER_ARRAY_2D("pdoa_lut0.data",
					     config1_pdoa_lut0_data),
		CHERRY_CALIB_UINT8("wifi_coex_max_grant_duration", 0),
		CHERRY_CALIB_UINT8("wifi_coex_min_inactive_duration", 128),
		CHERRY_CALIB_UINT8("wifi_sw_cfg", 1 + (0 << 1) + (63 << 2)),
	};
	compare(calib, expected);
	cherry_calib_destroy(calib);
}
static const uint8_t util_calib_soc_ant_pair_0_ant_path[] = { 0x02, 0x01 };
static const uint8_t util_calib_soc_ant_set0_tx_ant_paths[] = { 0x00, 0x00 };
static const uint8_t util_calib_soc_ant_set0_rx_ants[] = { 0x01, 0x02, 0xff };
static const uint8_t util_calib_soc_ant_set1_tx_ant_paths[] = { 0x00, 0xff };
static const uint8_t util_calib_soc_ant_set1_rx_ants[] = { 0x02, 0x01, 0x00 };
TEST_F(CherryCalib, testSOCconfig)
{
	struct cherry_calib *calib =
		cherry_calib_folder_load(config_soc_path, "foo");
	ASSERT_NE(calib, nullptr);
	static const struct cherry_calib_key expected[] = {
		/* ANT0 = TX_ANT1 */
		CHERRY_CALIB_UINT8("ant0.transceiver",
				   0), //erreur  enum est en 32 bits
		CHERRY_CALIB_UINT8("ant0.port",
				   1), //erreur  enum est en 32 bits
		CHERRY_CALIB_UINT8("ant0.ext_sw_cfg", 0),
		CHERRY_CALIB_UINT8("ant0.lna", 0),
		/* ANT1 = RXB_ANT2 */
		CHERRY_CALIB_UINT8("ant1.transceiver",
				   2), //erreur  enum est en 32 bits
		CHERRY_CALIB_UINT8("ant1.port",
				   2), //erreur  enum est en 32 bits
		CHERRY_CALIB_UINT8("ant1.ext_sw_cfg", 0),
		CHERRY_CALIB_UINT8("ant1.lna", 1),
		/* ATN2 = RXA_ANT3 */
		CHERRY_CALIB_UINT8("ant2.transceiver",
				   1), //erreur  enum est en 32 bits
		CHERRY_CALIB_UINT8("ant2.port",
				   3), //erreur  enum est en 32 bits
		CHERRY_CALIB_UINT8("ant2.ext_sw_cfg", 0),
		CHERRY_CALIB_UINT8("ant2.lna", 1),
		/* Ant Pair 0 to measure Azimuth between ant 2 and ant1 */
		CHERRY_CALIB_UINT8("ant_pair0.axis",
				   0), //erreur  enum est en 32 bits
		CHERRY_CALIB_NUMBER_ARRAY_1D("ant_pair0.ant_paths",
					     util_calib_soc_ant_pair_0_ant_path),
		/* Ant Set0 */
		CHERRY_CALIB_UINT8("ant_set0.nb_rx_ants", 2),
		CHERRY_CALIB_BOOL("ant_set0.rx_ants_are_pairs", false),
		CHERRY_CALIB_NUMBER_ARRAY_1D(
			"ant_set0.tx_ant_paths",
			util_calib_soc_ant_set0_tx_ant_paths),
		CHERRY_CALIB_NUMBER_ARRAY_1D("ant_set0.rx_ants",
					     util_calib_soc_ant_set0_rx_ants),
		CHERRY_CALIB_UINT8("ant_set0.tx_power_control", 0x3),
		/* Ant Set1 */
		CHERRY_CALIB_UINT8("ant_set1.nb_rx_ants", 1),
		CHERRY_CALIB_BOOL("ant_set1.rx_ants_are_pairs", false),
		CHERRY_CALIB_NUMBER_ARRAY_1D(
			"ant_set1.tx_ant_paths",
			util_calib_soc_ant_set1_tx_ant_paths),
		CHERRY_CALIB_NUMBER_ARRAY_1D("ant_set1.rx_ants",
					     util_calib_soc_ant_set1_rx_ants),
		CHERRY_CALIB_UINT8("ant_set1.tx_power_control", 0x3),

	};
	compare(calib, expected);
	cherry_calib_destroy(calib);
}

static const uint8_t util_calib_sip_ant_pair_0_ant_path[] = { 0x02, 0x01 };
static const uint8_t util_calib_sip_ant_pair_1_ant_path[] = { 0x03, 0x01 };
static const uint8_t util_calib_sip_ant_pair_2_ant_path[] = { 0x03, 0x02 };
static const uint8_t util_calib_sip_ant_set0_tx_ant_paths[] = { 0x00, 0xff };
static const uint8_t util_calib_sip_ant_set0_rx_ants[] = { 0x01, 0x02, 0xff };
static const uint8_t util_calib_sip_ant_set1_tx_ant_paths[] = { 0x00, 0xff };
static const uint8_t util_calib_sip_ant_set1_rx_ants[] = { 0x02, 0x01, 0x00 };

TEST_F(CherryCalib, test_sip_config)
{
	struct cherry_calib *calib =
		cherry_calib_folder_load(config_sip_path, "foo");
	ASSERT_NE(calib, nullptr);
	static const struct cherry_calib_key expected[] = {
		/* ANT0 = TX_ANT2 */
		CHERRY_CALIB_UINT8("ant0.transceiver", 0),
		CHERRY_CALIB_UINT8("ant0.port", 2),
		CHERRY_CALIB_UINT8("ant0.ext_sw_cfg", 0),
		CHERRY_CALIB_UINT8("ant0.lna", 0),
		/* ANT1 = RXB_ANT2 */
		CHERRY_CALIB_UINT8("ant1.transceiver", 2),
		CHERRY_CALIB_UINT8("ant1.port", 2),
		CHERRY_CALIB_UINT8("ant1.ext_sw_cfg", 0),
		CHERRY_CALIB_UINT8("ant1.lna", 1),
		/* ATN2 = RXA_ANT3 */
		CHERRY_CALIB_UINT8("ant2.transceiver", 1),
		CHERRY_CALIB_UINT8("ant2.port", 3),
		CHERRY_CALIB_UINT8("ant2.ext_sw_cfg", 0),
		CHERRY_CALIB_UINT8("ant2.lna", 1),
		/* ATN3 = RXA_ANT4 */
		CHERRY_CALIB_UINT8("ant3.transceiver", 1),
		CHERRY_CALIB_UINT8("ant3.port", 4),
		CHERRY_CALIB_UINT8("ant3.ext_sw_cfg", 0),
		CHERRY_CALIB_UINT8("ant3.lna", 1),
		/* Ant Pair 0 to measure Azimuth between ant2 and ant1 */
		CHERRY_CALIB_UINT8("ant_pair0.axis", 0),
		CHERRY_CALIB_NUMBER_ARRAY_1D("ant_pair0.ant_paths",
					     util_calib_sip_ant_pair_0_ant_path),
		/* Ant Pair 1 to measure Azimuth between ant3 and ant1 */
		CHERRY_CALIB_UINT8("ant_pair1.axis", 1),
		CHERRY_CALIB_NUMBER_ARRAY_1D("ant_pair1.ant_paths",
					     util_calib_sip_ant_pair_1_ant_path),
		/* Ant Pair 2 to measure Azimuth between ant3 and ant2 */
		CHERRY_CALIB_UINT8("ant_pair2.axis", 2),
		CHERRY_CALIB_NUMBER_ARRAY_1D("ant_pair2.ant_paths",
					     util_calib_sip_ant_pair_2_ant_path),
		/* Ant Set0 */
		CHERRY_CALIB_UINT8("ant_set0.nb_rx_ants", 2),
		CHERRY_CALIB_BOOL("ant_set0.rx_ants_are_pairs", false),
		CHERRY_CALIB_NUMBER_ARRAY_1D(
			"ant_set0.tx_ant_paths",
			util_calib_sip_ant_set0_tx_ant_paths),
		CHERRY_CALIB_NUMBER_ARRAY_1D("ant_set0.rx_ants",
					     util_calib_sip_ant_set0_rx_ants),
		CHERRY_CALIB_UINT8("ant_set0.tx_power_control", 0x3),
		/* Ant Set1 */
		CHERRY_CALIB_BOOL("ant_set1.rx_ants_are_pairs", true),
		CHERRY_CALIB_UINT8("ant_set1.nb_rx_ants", 3),
		CHERRY_CALIB_NUMBER_ARRAY_1D(
			"ant_set1.tx_ant_paths",
			util_calib_sip_ant_set1_tx_ant_paths),
		CHERRY_CALIB_NUMBER_ARRAY_1D("ant_set1.rx_ants",
					     util_calib_sip_ant_set1_rx_ants),
		CHERRY_CALIB_UINT8("ant_set1.tx_power_control", 0x3),
	};
	compare(calib, expected);
	cherry_calib_destroy(calib);
}

static const uint8_t util_calib_radar_ant_set3_tx_ant_paths[] = { 0x00, 0xff };
static const uint8_t util_calib_radar_ant_set3_rx_ants[] = { 0x02, 0xff, 0xff };

TEST_F(CherryCalib, test_radar_config)
{
	struct cherry_calib *calib =
		cherry_calib_folder_load(config_radar_path, "foo");
	ASSERT_NE(calib, nullptr);
	static const struct cherry_calib_key expected[] = {
		/* TX */
		CHERRY_CALIB_UINT8("ant0.transceiver", 0),
		CHERRY_CALIB_UINT8("ant0.port", 1),
		CHERRY_CALIB_UINT8("ant0.ext_sw_cfg", 0),
		CHERRY_CALIB_UINT8("ant0.lna", 0),
		/* RX */
		CHERRY_CALIB_UINT8("ant2.transceiver", 1),
		CHERRY_CALIB_UINT8("ant2.port", 2),
		CHERRY_CALIB_UINT8("ant2.ext_sw_cfg", 0),
		CHERRY_CALIB_UINT8("ant2.lna", 1),
		/* Ant Set3 */
		CHERRY_CALIB_UINT8("ant_set3.nb_rx_ants", 1),
		CHERRY_CALIB_BOOL("ant_set3.rx_ants_are_pairs", false),
		CHERRY_CALIB_NUMBER_ARRAY_1D(
			"ant_set3.tx_ant_paths",
			util_calib_radar_ant_set3_tx_ant_paths),
		CHERRY_CALIB_NUMBER_ARRAY_1D("ant_set3.rx_ants",
					     util_calib_radar_ant_set3_rx_ants),
		CHERRY_CALIB_UINT8("ant_set3.tx_power_control", 0x80),
		CHERRY_CALIB_UINT8("debug.pa_enabled", 1),
		CHERRY_CALIB_UINT32("debug.tx_power", 0xfefefefe),

	};
	compare(calib, expected);
	cherry_calib_destroy(calib);
}
