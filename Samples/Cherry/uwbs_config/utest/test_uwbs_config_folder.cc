/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "test_compare.h"
#include "test_uwbs_config_load.h"

extern "C" {
#include <uwbs_config/uwbs_config.h>
}

class UwbsConfigLoadFolder : public UwbsConfigLoad {
    protected:
	enum uwbs_config_status load(const char *folder,
				     const char *country_code)
	{
		keys.clear();
		return uwbs_config_load(folder, country_code,
					&UwbsConfigLoad::on_new_key,
					static_cast<UwbsConfigLoad *>(this));
	}
	const char *config1_path = UTEST_RESOURCES_PATH "/config1";
};

TEST_F(UwbsConfigLoadFolder, InvalidPath)
{
	ASSERT_EQ(load(nullptr, nullptr), UWBS_CONFIG_STATUS_INVALID_ARG);
	compare(keys, {});
}

TEST_F(UwbsConfigLoadFolder, NoCountryCode)
{
	ASSERT_EQ(load(config1_path, nullptr), UWBS_CONFIG_STATUS_OK);
	KeyMap expected;
	expected.emplace(make_key_uint16("restricted_channels", 12));
	expected.emplace(make_key_uint8("wifi_coex_mode", 0));
	expected.emplace(make_key_uint8("wifi_coex_time_gap", 0));
	expected.emplace(make_key_bool("ch5.wifi_coex_enabled", true));
	expected.emplace(make_key_uint8("ch5.pll_locking_code", 255));
	expected.emplace(make_key_bool("ch9.wifi_coex_enabled", false));
	expected.emplace(make_key_uint8("ch9.pll_locking_code", 1));
	expected.emplace(make_key_array<uint8_t>(
		"ant0.ch5.ref_frame0.tx_power_index", { 70, 71, 72, 73 }));
	expected.emplace(make_key_array<int16_t>(
		"pdoa_lut0.data",
		{
			{ 0, 1 },   { 2, 3 },	{ 4, 5 },   { 6, 7 },
			{ 8, 9 },   { 10, 11 }, { 12, 13 }, { 14, 15 },
			{ 16, 17 }, { 18, 19 }, { 20, 21 }, { 22, 23 },
			{ 24, 25 }, { 26, 27 }, { 28, 29 }, { 30, 31 },
			{ 32, 33 }, { 34, 35 }, { 36, 37 }, { 38, 39 },
			{ 40, 41 }, { 42, 43 }, { 44, 45 }, { 46, 47 },
			{ 48, 49 }, { 50, 51 }, { 52, 53 }, { 54, 55 },
			{ 56, 57 }, { 58, 59 }, { 60, 61 },
		}));
	compare(keys, expected);
}

TEST_F(UwbsConfigLoadFolder, UnknowCountryCode)
{
	ASSERT_EQ(load(config1_path, "foo"),
		  UWBS_CONFIG_STATUS_COUNTRY_CODE_NOT_FOUND);
	KeyMap expected;
	expected.emplace(make_key_uint16("restricted_channels", 12));
	expected.emplace(make_key_uint8("wifi_coex_mode", 0));
	expected.emplace(make_key_uint8("wifi_coex_time_gap", 0));
	expected.emplace(make_key_bool("ch5.wifi_coex_enabled", true));
	expected.emplace(make_key_uint8("ch5.pll_locking_code", 255));
	expected.emplace(make_key_bool("ch9.wifi_coex_enabled", false));
	expected.emplace(make_key_uint8("ch9.pll_locking_code", 1));
	expected.emplace(make_key_array<uint8_t>(
		"ant0.ch5.ref_frame0.tx_power_index", { 70, 71, 72, 73 }));
	expected.emplace(make_key_array<int16_t>(
		"pdoa_lut0.data",
		{
			{ 0, 1 },   { 2, 3 },	{ 4, 5 },   { 6, 7 },
			{ 8, 9 },   { 10, 11 }, { 12, 13 }, { 14, 15 },
			{ 16, 17 }, { 18, 19 }, { 20, 21 }, { 22, 23 },
			{ 24, 25 }, { 26, 27 }, { 28, 29 }, { 30, 31 },
			{ 32, 33 }, { 34, 35 }, { 36, 37 }, { 38, 39 },
			{ 40, 41 }, { 42, 43 }, { 44, 45 }, { 46, 47 },
			{ 48, 49 }, { 50, 51 }, { 52, 53 }, { 54, 55 },
			{ 56, 57 }, { 58, 59 }, { 60, 61 },
		}));
	compare(keys, expected);
}

TEST_F(UwbsConfigLoadFolder, Country1)
{
	ASSERT_EQ(load(config1_path, "country1"), UWBS_CONFIG_STATUS_OK);
	KeyMap expected;
	expected.emplace(make_key_uint16("restricted_channels", 12));
	expected.emplace(make_key_uint8("wifi_coex_mode", 0));
	expected.emplace(make_key_uint8("wifi_coex_time_gap", 0));
	expected.emplace(make_key_bool("ch5.wifi_coex_enabled", true));
	expected.emplace(make_key_uint8("ch5.pll_locking_code", 255));
	expected.emplace(make_key_bool("ch9.wifi_coex_enabled", false));
	expected.emplace(make_key_uint8("ch9.pll_locking_code", 1));
	expected.emplace(make_key_array<uint8_t>(
		"ant0.ch5.ref_frame0.tx_power_index", { 70, 71, 72, 73 }));
	expected.emplace(make_key_array<int16_t>(
		"pdoa_lut0.data",
		{
			{ 0, 1 },   { 2, 3 },	{ 4, 5 },   { 6, 7 },
			{ 8, 9 },   { 10, 11 }, { 12, 13 }, { 14, 15 },
			{ 16, 17 }, { 18, 19 }, { 20, 21 }, { 22, 23 },
			{ 24, 25 }, { 26, 27 }, { 28, 29 }, { 30, 31 },
			{ 32, 33 }, { 34, 35 }, { 36, 37 }, { 38, 39 },
			{ 40, 41 }, { 42, 43 }, { 44, 45 }, { 46, 47 },
			{ 48, 49 }, { 50, 51 }, { 52, 53 }, { 54, 55 },
			{ 56, 57 }, { 58, 59 }, { 60, 61 },
		}));
	expected.emplace(make_key_uint8("wifi_coex_max_grant_duration", 0));
	expected.emplace(
		make_key_uint8("wifi_coex_min_inactive_duration", 128));
	expected.emplace(
		make_key_uint8("wifi_sw_cfg", 1 + (0 << 1) + (63 << 2)));
	compare(keys, expected);
}
