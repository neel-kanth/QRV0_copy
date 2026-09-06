/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "uwbs_config/uwbs_config.h"

#include <gtest/gtest.h>
#include <string>

extern "C" {
#include "region_loader.h"
}

class UwbsRegionLoad : public testing::Test {
    protected:
	void SetUp() override
	{
	}

	void TearDown() override
	{
	}
};

class UwbsRegionLoadFile : public UwbsRegionLoad {
    protected:
	enum uwbs_config_status find_from_file(const char *path,
					       const char *country_code,
					       char **region)
	{
		return uwbs_config_region_loader_find_from_file(
			path, country_code, region);
	}
};

class UwbsRegionLoadStr : public UwbsRegionLoad {
    protected:
	enum uwbs_config_status find_from_str(const char *str,
					      const size_t size,
					      const char *country_code,
					      char **region)
	{
		return uwbs_config_region_loader_find_from_str(
			str, size, country_code, region);
	}

	template <size_t S>
	enum uwbs_config_status find_from_str(const char (&str)[S],
					      const char *country_code,
					      char **region)
	{
		return find_from_str(str, S - 1, country_code, region);
	}
};

TEST_F(UwbsRegionLoadFile, NullPath)
{
	char *region = nullptr;
	ASSERT_EQ(find_from_file(nullptr, "foo", &region),
		  UWBS_CONFIG_STATUS_INVALID_ARG);
	ASSERT_EQ(region, nullptr);
}

TEST_F(UwbsRegionLoadFile, NullCountryCode)
{
	char *region = nullptr;
	ASSERT_EQ(find_from_file("/foo", nullptr, &region),
		  UWBS_CONFIG_STATUS_INVALID_ARG);
	ASSERT_EQ(region, nullptr);
}

TEST_F(UwbsRegionLoadFile, NullRegion)
{
	ASSERT_EQ(find_from_file("/foo", "foo", nullptr),
		  UWBS_CONFIG_STATUS_INVALID_ARG);
}

TEST_F(UwbsRegionLoadStr, NullString)
{
	char *region = nullptr;
	ASSERT_EQ(find_from_str(nullptr, 42, "foo", &region),
		  UWBS_CONFIG_STATUS_INVALID_ARG);
	ASSERT_EQ(region, nullptr);
}

TEST_F(UwbsRegionLoadStr, NullCountryCode)
{
	char *region = nullptr;
	ASSERT_EQ(find_from_str("", 1, nullptr, &region),
		  UWBS_CONFIG_STATUS_INVALID_ARG);
	ASSERT_EQ(region, nullptr);
}

TEST_F(UwbsRegionLoadStr, NullCountryRegion)
{
	ASSERT_EQ(find_from_str("", 1, "foo", nullptr),
		  UWBS_CONFIG_STATUS_INVALID_ARG);
}

TEST_F(UwbsRegionLoadStr, EmptyYaml)
{
	static const char input[] = "";
	char *region = nullptr;
	ASSERT_EQ(find_from_str(input, "foo", &region),
		  UWBS_CONFIG_STATUS_COUNTRY_CODE_NOT_FOUND);
	ASSERT_EQ(region, nullptr);
}

TEST_F(UwbsRegionLoadStr, ValidRegions)
{
	static const char input[] = "version: 1\n"
				    "regions:\n"
				    "  region1:\n"
				    "   - country1\n"
				    "   - country2\n"
				    "  region2:\n"
				    "   - country3\n"
				    "   - country4\n";
	char *region = nullptr;
	ASSERT_EQ(find_from_str(input, "country2", &region),
		  UWBS_CONFIG_STATUS_OK);
	ASSERT_STREQ(region, "region1");
	free(region);
	ASSERT_EQ(find_from_str(input, "country3", &region),
		  UWBS_CONFIG_STATUS_OK);
	ASSERT_STREQ(region, "region2");
	free(region);
	ASSERT_EQ(find_from_str(input, "country42", &region),
		  UWBS_CONFIG_STATUS_COUNTRY_CODE_NOT_FOUND);
	ASSERT_EQ(region, nullptr);
}

TEST_F(UwbsRegionLoadStr, DuplicatedCountry)
{
	static const char input[] = "version: 1\n"
				    "regions:\n"
				    "  region1:\n"
				    "   - country1\n"
				    "   - country2\n"
				    "  region2:\n"
				    "   - country2\n"
				    "   - country3\n";
	char *region = nullptr;
	ASSERT_EQ(find_from_str(input, "country2", &region),
		  UWBS_CONFIG_STATUS_INVALID_FMT);
	ASSERT_EQ(region, nullptr);
	free(region);
	ASSERT_EQ(find_from_str(input, "country3", &region),
		  UWBS_CONFIG_STATUS_OK);
	ASSERT_STREQ(region, "region2");
	free(region);
}
