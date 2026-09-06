/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include <cstdint>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

extern "C" {
#include "model_loader.h"
#include "uwbs_config/uwbs_config.h"
}

#include "test_uwbs_config_model_builder.h"

#include <cstring>

class UwbsModelLoad : public testing::Test {
    public:
	UwbsModelLoad()
		: model_initialized_(false)
	{
	}

    protected:
	void SetUp() override
	{
	}

	void TearDown() override
	{
		cleanup();
	}

	void cleanup()
	{
		if (!model_initialized_)
			return;

		uwbs_config_model_destroy(&model);
		model_initialized_ = false;
	}

    public:
	struct uwbs_config_model model;

    protected:
	bool model_initialized_;
};

class UwbsModelLoadFile : public UwbsModelLoad {
    protected:
	uwbs_config_status load_file(const char *path,
				     struct uwbs_config_model *model)
	{
		cleanup();
		uwbs_config_status st =
			uwbs_config_model_load_file(path, model);
		model_initialized_ = (st == UWBS_CONFIG_STATUS_OK);
		return st;
	}
};
class UwbsModelLoadStr : public UwbsModelLoad {
    protected:
	uwbs_config_status load_str(const char *str, size_t size,
				    struct uwbs_config_model *model)
	{
		cleanup();
		uwbs_config_status st =
			uwbs_config_model_load_str(str, size, model);
		model_initialized_ = (st == UWBS_CONFIG_STATUS_OK);
		return st;
	}

	template <size_t S>
	uwbs_config_status load_str(const char (&str)[S],
				    struct uwbs_config_model *model)
	{
		return load_str(str, S - 1, model);
	}

	uwbs_config_status load_str(const std::string &str,
				    struct uwbs_config_model *model)
	{
		return load_str(str.c_str(), str.size(), model);
	}

	void test_simple_number(
		const enum uwbs_config_model_underlying_type underlying_type);
	template <typename T> void test_simple_number();

	void test_simple_number(
		const enum uwbs_config_model_underlying_type underlying_type,
		const int64_t min, const int64_t max,
		const enum uwbs_config_status expected_status =
			UWBS_CONFIG_STATUS_OK);
	template <typename T>
	void test_simple_number(const int64_t min, const int64_t max,
				const enum uwbs_config_status expected_status =
					UWBS_CONFIG_STATUS_OK);
};

TEST_F(UwbsModelLoadFile, NullPath)
{
	ASSERT_EQ(load_file(NULL, &model), UWBS_CONFIG_STATUS_INVALID_ARG);
}

TEST_F(UwbsModelLoadFile, NullModel)
{
	ASSERT_EQ(load_file("/tmp/test.json", NULL),
		  UWBS_CONFIG_STATUS_INVALID_ARG);
}

TEST_F(UwbsModelLoadFile, InvalidPath)
{
	ASSERT_EQ(load_file("/tmp_foo_does_not_exists/", &model),
		  UWBS_CONFIG_STATUS_INVALID_ARG);
}

TEST_F(UwbsModelLoadStr, NullString)
{
	ASSERT_EQ(load_str(NULL, 42, &model), UWBS_CONFIG_STATUS_INVALID_ARG);
}

TEST_F(UwbsModelLoadStr, EmptyString)
{
	ASSERT_EQ(load_str("", 0, &model), UWBS_CONFIG_STATUS_INVALID_ARG);
}

TEST_F(UwbsModelLoadStr, NullModel)
{
	ASSERT_EQ(load_str("a", 1, NULL), UWBS_CONFIG_STATUS_INVALID_ARG);
}

TEST_F(UwbsModelLoadStr, EmptyModel)
{
	static const char input[] = "{}";
	ASSERT_EQ(load_str(input, &model), UWBS_CONFIG_STATUS_INVALID_FMT);
	compare(model, make_model());
}

TEST_F(UwbsModelLoadStr, EmptyRoot)
{
	static const char input[] = "{\"type\": \"object\"}";
	ASSERT_EQ(load_str(input, &model), UWBS_CONFIG_STATUS_OK);
	compare(model, make_model(make_model_def_root({}, {})));
}

TEST_F(UwbsModelLoadStr, InvalidRootField)
{
	static const char input[] = "{\n"
				    "  \"props\": {\n"
				    "  }\n"
				    "}";
	ASSERT_EQ(load_str(input, &model), UWBS_CONFIG_STATUS_INVALID_FMT);
	compare(model, make_model());
}

TEST_F(UwbsModelLoadStr, InvalidPropertyFieldName)
{
	static const char input[] = "{\n"
				    "  \"properties\": {\n"
				    "    \"time_gap\": {\n"
				    "      \"maximum_\": 150,\n"
				    "      \"minimum\": 42,\n"
				    "      \"title\": \"Time Gap\",\n"
				    "      \"type\": \"integer\",\n"
				    "      \"underlying_type\": \"uint8_t\"\n"
				    "    }\n"
				    "  }\n"
				    "}";
	ASSERT_EQ(load_str(input, &model), UWBS_CONFIG_STATUS_INVALID_FMT);
	compare(model, make_model());
}

TEST_F(UwbsModelLoadStr, DefinitionTypeNull)
{
	static const char input[] = "{\n"
				    "  \"$defs\": {\n"
				    "    \"unknown_def\": {\n"
				    "      \"type\": \"null\"\n"
				    "    },\n"
				    "  }\n"
				    "}";
	ASSERT_EQ(load_str(input, &model), UWBS_CONFIG_STATUS_INVALID_FMT);
	compare(model, make_model());
}

void UwbsModelLoadStr::test_simple_number(
	const enum uwbs_config_model_underlying_type underlying_type)
{
	std::string input =
		std::string() +
		"{\n"
		"  \"properties\": {\n"
		"    \"time_gap\": {\n"
		"      \"title\": \"Time Gap\",\n"
		"      \"type\": \"integer\",\n"
		"      \"underlying_type\": \"" +
		uwbs_config_model_underlying_type_to_str(underlying_type) +
		"\"\n"
		"    }\n"
		"  },\n"
		"  \"type\": \"object\"\n"
		"}";

	auto expected = make_model(make_model_def_root(
		{}, { make_model_number("time_gap", underlying_type) }));

	ASSERT_EQ(load_str(input, &model), UWBS_CONFIG_STATUS_OK);
	compare(model, expected);
}
template <typename T> void UwbsModelLoadStr::test_simple_number()
{
	test_simple_number(model_underlying_type<T>());
}

void UwbsModelLoadStr::test_simple_number(
	const enum uwbs_config_model_underlying_type underlying_type,
	const int64_t min, const int64_t max,
	const enum uwbs_config_status expected_status)
{
	std::string input =
		std::string() +
		"{\n"
		"  \"properties\": {\n"
		"    \"time_gap\": {\n"
		"      \"maximum\": " +
		std::to_string(max) +
		",\n"
		"      \"minimum\": " +
		std::to_string(min) +
		",\n"
		"      \"title\": \"Time Gap\",\n"
		"      \"type\": \"integer\",\n"
		"      \"underlying_type\": \"" +
		uwbs_config_model_underlying_type_to_str(underlying_type) +
		"\"\n"
		"    }\n"
		"  },\n"
		"  \"type\": \"object\"\n"
		"}";

	auto expected = make_model(make_model_def_root(
		{},
		{ make_model_number("time_gap", underlying_type, min, max) }));

	ASSERT_EQ(load_str(input, &model), expected_status);
	if (expected_status == UWBS_CONFIG_STATUS_OK)
		compare(model, expected);
	else
		compare(model, make_model());
}

template <typename T>
void UwbsModelLoadStr::test_simple_number(
	const int64_t min, const int64_t max,
	const enum uwbs_config_status expected_status)
{
	test_simple_number(model_underlying_type<T>(), min, max,
			   expected_status);
}

TEST_F(UwbsModelLoadStr, NumberModelUint8)
{
	test_simple_number<uint8_t>();
	test_simple_number<uint8_t>(12, 42);
}

TEST_F(UwbsModelLoadStr, NumberModelInt8)
{
	test_simple_number<int8_t>();
	test_simple_number<int8_t>(-42, 42);
}

TEST_F(UwbsModelLoadStr, NumberModelUint16)
{
	test_simple_number<uint16_t>();
	test_simple_number<uint16_t>(42, 15000);
}

TEST_F(UwbsModelLoadStr, NumberModelInt16)
{
	test_simple_number<int16_t>();
	test_simple_number<int16_t>(-500, -420);
}

TEST_F(UwbsModelLoadStr, NumberModelUint32)
{
	test_simple_number<uint32_t>();
	test_simple_number<uint32_t>(42, 15000);
}

TEST_F(UwbsModelLoadStr, NumberModelInt32)
{
	test_simple_number<int32_t>();
	test_simple_number<int32_t>(-5000, -4200);
}

TEST_F(UwbsModelLoadStr, NumberModelSameMinMax)
{
	test_simple_number<int32_t>(-4200, -4200);
}

TEST_F(UwbsModelLoadStr, NumberModelWrongMinMax)
{
	test_simple_number<int32_t>(0, -4200, UWBS_CONFIG_STATUS_INVALID_FMT);
}

TEST_F(UwbsModelLoadStr, BooleanModel)
{
	static const char input[] = "{\n"
				    "  \"properties\": {\n"
				    "    \"bool1\": {\n"
				    "      \"title\": \"Something\",\n"
				    "      \"type\": \"boolean\",\n"
				    "    }\n"
				    "  },\n"
				    "  \"type\": \"object\"\n"
				    "}";
	auto expected = make_model(
		make_model_def_root({}, { make_model_boolean("bool1") }));

	ASSERT_EQ(load_str(input, &model), UWBS_CONFIG_STATUS_OK);
	compare(model, expected);
}

TEST_F(UwbsModelLoadStr, ArrayModel)
{
	static const char input[] = "{\n"
				    "  \"properties\": {\n"
				    "    \"arr1\": {\n"
				    "      \"items\": {\n"
				    "        \"maximum\": 23,\n"
				    "        \"minimum\": 1,\n"
				    "        \"type\": \"integer\",\n"
				    "        \"underlying_type\": \"uint8_t\"\n"
				    "      },\n"
				    "      \"maxItems\": 4,\n"
				    "      \"title\": \"An array\",\n"
				    "      \"type\": \"array\"\n"
				    "    }\n"
				    "  },\n"
				    "  \"type\": \"object\"\n"
				    "}";
	auto expected = make_model(make_model_def_root(
		{},
		{ make_model_array(
			"arr1",
			make_model_number(
				NULL, UWBS_CONFIG_UNDERLYING_TYPE_UINT8, 1, 23),
			4) }));

	ASSERT_EQ(load_str(input, &model), UWBS_CONFIG_STATUS_OK);
	compare(model, expected);
}

TEST_F(UwbsModelLoadStr, ArrayModelInvalidItemType)
{
	static const char input[] =
		"{\n"
		"  \"$defs\": {\n"
		"    \"my_object\": {\n"
		"      \"properties\": {\n"
		"        \"val_enum\": {\n"
		"          \"$ref\": \"#/$defs/my_enum\"\n"
		"        },\n"
		"        \"time_gap\": {\n"
		"          \"maximum\": -4200,\n"
		"          \"minimum\": -5000,\n"
		"          \"title\": \"Time Gap\",\n"
		"          \"type\": \"integer\",\n"
		"          \"underlying_type\": \"int32_t\"\n"
		"        }\n"
		"      },\n"
		"      \"type\": \"object\"\n"
		"    },\n"
		"  },\n"
		"  \"properties\": {\n"
		"    \"arr1\": {\n"
		"      \"items\": {\n"
		"        \"$ref\": \"#/$defs/my_object\"\n"
		"      },\n"
		"      \"maxItems\": 4,\n"
		"      \"title\": \"An array\",\n"
		"      \"type\": \"array\"\n"
		"    }\n"
		"  },\n"
		"  \"type\": \"object\"\n"
		"}";
	auto expected = make_model();
	ASSERT_EQ(load_str(input, &model), UWBS_CONFIG_STATUS_INVALID_FMT);
	compare(model, expected);
}

TEST_F(UwbsModelLoadStr, EnumModel)
{
	static const char input[] = "{\n"
				    "  \"$defs\": {\n"
				    "    \"my_enum\": {\n"
				    "      \"enum\": [\n"
				    "        \"a\",\n"
				    "        \"b\",\n"
				    "        \"c\"\n"
				    "      ],\n"
				    "      \"type\": \"string\",\n"
				    "      \"underlying_enum\": {\n"
				    "        \"a\": 0,\n"
				    "        \"b\": 1,\n"
				    "        \"c\": 2\n"
				    "      }\n"
				    "    },\n"
				    "  },\n"
				    "  \"properties\": {\n"
				    "    \"val\": {\n"
				    "      \"$ref\": \"#/$defs/my_enum\"\n"
				    "    }\n"
				    "  },\n"
				    "  \"type\": \"object\"\n"
				    "}";
	auto expected = make_model(make_model_def_root(
		{ make_model_def_enum("my_enum",
				      { { "a", 0 }, { "b", 1 }, { "c", 2 } }) },
		{ make_model_ref("val", "#/$defs/my_enum") }));

	ASSERT_EQ(load_str(input, &model), UWBS_CONFIG_STATUS_OK);
	compare(model, expected);
}

TEST_F(UwbsModelLoadStr, ObjectModel)
{
	static const char input[] =
		"{\n"
		"  \"$defs\": {\n"
		"    \"my_enum\": {\n"
		"      \"enum\": [\n"
		"        \"a\",\n"
		"        \"b\",\n"
		"        \"c\"\n"
		"      ],\n"
		"      \"type\": \"string\",\n"
		"      \"underlying_enum\": {\n"
		"        \"a\": 0,\n"
		"        \"b\": 1,\n"
		"        \"c\": 2\n"
		"      }\n"
		"    },\n"
		"    \"my_object\": {\n"
		"      \"properties\": {\n"
		"        \"val_enum\": {\n"
		"          \"$ref\": \"#/$defs/my_enum\"\n"
		"        },\n"
		"        \"time_gap\": {\n"
		"          \"maximum\": -4200,\n"
		"          \"minimum\": -5000,\n"
		"          \"title\": \"Time Gap\",\n"
		"          \"type\": \"integer\",\n"
		"          \"underlying_type\": \"int32_t\"\n"
		"        }\n"
		"      },\n"
		"      \"type\": \"object\"\n"
		"    }\n"
		"  },\n"
		"  \"properties\": {\n"
		"    \"val\": {\n"
		"      \"$ref\": \"#/$defs/my_object\"\n"
		"    }\n"
		"  },\n"
		"  \"type\": \"object\"\n"
		"}";
	auto expected = make_model(make_model_def_root(
		{ make_model_def_enum("my_enum",
				      { { "a", 0 }, { "b", 1 }, { "c", 2 } }),
		  make_model_def_object(
			  "my_object",
			  { make_model_ref("val_enum", "#/$defs/my_enum"),
			    make_model_number("time_gap",
					      UWBS_CONFIG_UNDERLYING_TYPE_INT32,
					      -5000, -4200) }) },
		{ make_model_ref("val", "#/$defs/my_object") }));

	ASSERT_EQ(load_str(input, &model), UWBS_CONFIG_STATUS_OK);
	compare(model, expected);
}

TEST_F(UwbsModelLoadStr, Bitfield)
{
	static const char input[] = "{\n"
				    "  \"$defs\": {\n"
				    "    \"the_bf1\": {\n"
				    "      \"properties\": {\n"
				    "        \"field1\": {\n"
				    "          \"maximum\": 1,\n"
				    "          \"minimum\": 0,\n"
				    "          \"title\": \"field1\",\n"
				    "          \"type\": \"integer\"\n"
				    "        },\n"
				    "        \"field2\": {\n"
				    "          \"maximum\": 255,\n"
				    "          \"minimum\": 0,\n"
				    "          \"title\": \"field2\",\n"
				    "          \"type\": \"integer\"\n"
				    "        },\n"
				    "        \"field3\": {\n"
				    "          \"maximum\": 1023,\n"
				    "          \"minimum\": 0,\n"
				    "          \"title\": \"field3\",\n"
				    "          \"type\": \"integer\"\n"
				    "        },\n"
				    "      },\n"
				    "      \"required\": [\n"
				    "        \"field1\",\n"
				    "        \"field2\",\n"
				    "        \"field3\"\n"
				    "      ],\n"
				    "      \"type\": \"object\",\n"
				    "      \"underlying_type\": \"bitfield\"\n"
				    "    },\n"
				    "  },\n"
				    "  \"properties\": {\n"
				    "    \"bf1\": {\n"
				    "      \"$ref\": \"#/$defs/the_bf1\"\n"
				    "    }\n"
				    "  },\n"
				    "  \"type\": \"object\"\n"
				    "}\n";
	UwbsConfigModel expected = make_model(make_model_def_root(
		{ make_model_def_bitfield(
			"the_bf1",
			{
				make_model_number(
					"field1",
					UWBS_CONFIG_UNDERLYING_TYPE_NULL, 0, 1),
				make_model_number(
					"field2",
					UWBS_CONFIG_UNDERLYING_TYPE_NULL, 0,
					255),
				make_model_number(
					"field3",
					UWBS_CONFIG_UNDERLYING_TYPE_NULL, 0,
					1023),
			}) },
		{ make_model_ref("bf1", "#/$defs/the_bf1") }));
	ASSERT_EQ(load_str(input, &model), UWBS_CONFIG_STATUS_OK);
	compare(model, expected);
}

TEST_F(UwbsModelLoadStr, BitfieldInvalidFieldType)
{
	static const char input[] = "{\n"
				    "  \"$defs\": {\n"
				    "    \"the_bf1\": {\n"
				    "      \"properties\": {\n"
				    "        \"field1\": {\n"
				    "          \"title\": \"field1\",\n"
				    "          \"type\": \"string\"\n"
				    "        },\n"
				    "      },\n"
				    "      \"required\": [\n"
				    "        \"field1\",\n"
				    "      ],\n"
				    "      \"type\": \"object\",\n"
				    "      \"underlying_type\": \"bitfield\"\n"
				    "    },\n"
				    "  },\n"
				    "  \"properties\": {\n"
				    "    \"bf1\": {\n"
				    "      \"$ref\": \"#/$defs/the_bf1\"\n"
				    "    }\n"
				    "  },\n"
				    "  \"type\": \"object\"\n"
				    "}\n";
	UwbsConfigModel expected = make_model();
	ASSERT_EQ(load_str(input, &model), UWBS_CONFIG_STATUS_INVALID_FMT);
	compare(model, expected);
}

TEST_F(UwbsModelLoadStr, BitfieldInvalidFieldSize)
{
	/* This test defines a schema with bitfields size of 33 bits (1 + 16 + 16) while bitfield max
	 * size is 32 bits. */
	static const char input[] = "{\n"
				    "  \"$defs\": {\n"
				    "    \"the_bf1\": {\n"
				    "      \"properties\": {\n"
				    "        \"field1\": {\n"
				    "          \"maximum\": 1,\n"
				    "          \"minimum\": 0,\n"
				    "          \"title\": \"field1\",\n"
				    "          \"type\": \"integer\"\n"
				    "        },\n"
				    "        \"field2\": {\n"
				    "          \"maximum\": 65535,\n"
				    "          \"minimum\": 0,\n"
				    "          \"title\": \"field2\",\n"
				    "          \"type\": \"integer\"\n"
				    "        },\n"
				    "        \"field3\": {\n"
				    "          \"maximum\": 65535,\n"
				    "          \"minimum\": 0,\n"
				    "          \"title\": \"field3\",\n"
				    "          \"type\": \"integer\"\n"
				    "        },\n"
				    "      },\n"
				    "      \"required\": [\n"
				    "        \"field1\",\n"
				    "      ],\n"
				    "      \"type\": \"object\",\n"
				    "      \"underlying_type\": \"bitfield\"\n"
				    "    },\n"
				    "  },\n"
				    "  \"properties\": {\n"
				    "    \"bf1\": {\n"
				    "      \"$ref\": \"#/$defs/the_bf1\"\n"
				    "    }\n"
				    "  },\n"
				    "  \"type\": \"object\"\n"
				    "}\n";
	UwbsConfigModel expected = make_model();
	ASSERT_EQ(load_str(input, &model), UWBS_CONFIG_STATUS_INVALID_FMT);
	compare(model, expected);
}

TEST_F(UwbsModelLoadStr, BitfieldInvalidSize)
{
	static const char input[] = "{\n"
				    "  \"$defs\": {\n"
				    "    \"the_bf1\": {\n"
				    "      \"properties\": {\n"
				    "        \"field1\": {\n"
				    "          \"maximum\": 4294967296,\n"
				    "          \"minimum\": 0,\n"
				    "          \"title\": \"field1\",\n"
				    "          \"type\": \"integer\"\n"
				    "        },\n"
				    "      },\n"
				    "      \"required\": [\n"
				    "        \"field1\",\n"
				    "      ],\n"
				    "      \"type\": \"object\",\n"
				    "      \"underlying_type\": \"bitfield\"\n"
				    "    },\n"
				    "  },\n"
				    "  \"properties\": {\n"
				    "    \"bf1\": {\n"
				    "      \"$ref\": \"#/$defs/the_bf1\"\n"
				    "    }\n"
				    "  },\n"
				    "  \"type\": \"object\"\n"
				    "}\n";
	UwbsConfigModel expected = make_model();
	ASSERT_EQ(load_str(input, &model), UWBS_CONFIG_STATUS_INVALID_FMT);
	compare(model, expected);
}
