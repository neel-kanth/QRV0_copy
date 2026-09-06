/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "test_uwbs_config_load.h"
#include "test_uwbs_config_model_builder.h"

#include <gtest/gtest.h>
#include <initializer_list>
#include <limits>
#include <list>
#include <string>

extern "C" {
#include "config_loader.h"
}

class UwbsConfigLoadFile : public UwbsConfigLoad {
    protected:
	enum uwbs_config_status load_file(const char *path,
					  const struct uwbs_config_model *model)
	{
		return uwbs_config_load_file(
			path, model, &UwbsConfigLoad::on_new_key,
			static_cast<UwbsConfigLoad *>(this));
	}
};

class UwbsConfigLoadStr : public UwbsConfigLoad {
    protected:
	enum uwbs_config_status load_str(const char *str, const size_t size,
					 const struct uwbs_config_model *model)
	{
		keys.clear();
		printf("Yaml:\n%s", str);
		return uwbs_config_load_str(
			str, size, model, &UwbsConfigLoad::on_new_key,
			static_cast<UwbsConfigLoad *>(this));
	}

	template <size_t S>
	enum uwbs_config_status load_str(const char (&str)[S],
					 const struct uwbs_config_model *model)
	{
		return load_str(str, S - 1, model);
	}

	enum uwbs_config_status load_str(const std::string &str,
					 const struct uwbs_config_model *model)
	{
		return load_str(str.c_str(), str.size(), model);
	}

	void test_simple_number(const UwbsConfigModel &model,
				const char *keyname, const int64_t ok,
				const KeyMap &expected_ok, const int64_t min,
				const int64_t max);

	template <typename T>
	void
	test_simple_number(const char *keyname,
			   const int64_t ok = std::numeric_limits<T>::max() / 2,
			   const int64_t min = std::numeric_limits<T>::min(),
			   const int64_t max = std::numeric_limits<T>::max())
	{
		UwbsConfigModel model = make_model(make_model_def_root(
			{}, { make_model_number<T>(keyname, min, max) }));

		KeyMap expected_ok;
		expected_ok.emplace(make_key<T>(keyname, ok));

		test_simple_number(model, keyname, ok, expected_ok, min, max);
	}
};

TEST_F(UwbsConfigLoadFile, NullPath)
{
	UwbsConfigModel model = make_model(make_model_def_root({}, {}));
	ASSERT_EQ(load_file(NULL, model), UWBS_CONFIG_STATUS_INVALID_ARG);
}

TEST_F(UwbsConfigLoadFile, NullModel)
{
	ASSERT_EQ(load_file("/tmp/test.json", NULL),
		  UWBS_CONFIG_STATUS_INVALID_ARG);
}

TEST_F(UwbsConfigLoadFile, InvalidPath)
{
	UwbsConfigModel model = make_model(make_model_def_root({}, {}));
	ASSERT_EQ(load_file("/tmp_foo_does_not_exists/", model),
		  UWBS_CONFIG_STATUS_INVALID_ARG);
}

TEST_F(UwbsConfigLoadStr, NullString)
{
	UwbsConfigModel model = make_model(make_model_def_root({}, {}));
	ASSERT_EQ(load_str(NULL, 42, model), UWBS_CONFIG_STATUS_INVALID_ARG);
}

TEST_F(UwbsConfigLoadStr, NullModel)
{
	ASSERT_EQ(load_str("a", 1, NULL), UWBS_CONFIG_STATUS_INVALID_ARG);
}

TEST_F(UwbsConfigLoadStr, EmptyYaml)
{
	static const char input[] = "";
	UwbsConfigModel model = make_model(make_model_def_root({}, {}));
	ASSERT_EQ(load_str(input, model), UWBS_CONFIG_STATUS_OK);
	compare(keys, KeyMap{});
}

class YamlBuilderNode;
typedef std::list<YamlBuilderNode> YamlBuilderNodeList;

class YamlBuilderNode {
    public:
	YamlBuilderNode(std::string &&_keyname, std::string &&_value)
		: keyname(std::move(_keyname))
		, value(std::move(_value))
	{
	}

	YamlBuilderNode(std::string &&_keyname, YamlBuilderNodeList &&_children)
		: keyname(std::move(_keyname))
		, children(std::move(_children))
	{
	}

	void to_string(std::ostream &os, const std::string &indent) const
	{
		os << indent << keyname << ':';
		if (!value.empty())
			os << ' ' << value << '\n';
		if (!children.empty()) {
			os << '\n';
			for (const YamlBuilderNode &child : children)
				child.to_string(os, indent + "  ");
		}
	}

    private:
	std::string keyname;
	std::string value;
	YamlBuilderNodeList children;
};

class YamlBuilder {
    public:
	YamlBuilder()
	{
	}

	YamlBuilder(YamlBuilderNodeList &&_children)
		: children(std::move(_children))
	{
	}

	std::string to_string() const
	{
		std::ostringstream oss;
		for (auto &child : children)
			child.to_string(oss, "");
		return oss.str();
	}

	operator std::string() const
	{
		return to_string();
	}

    private:
	YamlBuilderNodeList children;
};

YamlBuilderNode make_yaml_key(const char *keyname,
			      YamlBuilderNodeList &&children)
{
	return YamlBuilderNode(keyname, std::move(children));
}

YamlBuilderNode make_yaml_key(const char *keyname,
			      std::initializer_list<YamlBuilderNode> &&children)
{
	return YamlBuilderNode(keyname,
			       YamlBuilderNodeList(std::move(children)));
}

YamlBuilderNode make_yaml_key(const char *keyname, std::string &&value)
{
	return YamlBuilderNode(keyname, std::move(value));
}

YamlBuilderNode make_yaml_key(const char *keyname, const char *value)
{
	return make_yaml_key(keyname, std::string(value));
}

template <typename T>
YamlBuilderNode make_yaml_key(const char *keyname, const T &t)
{
	return make_yaml_key(keyname, std::to_string(t));
}

template <typename T> std::string make_yaml_value_array(const T &t)
{
	return std::to_string(t);
}

template <typename T>
std::string make_yaml_value_array(const std::initializer_list<T> &l)
{
	std::string value = "[";
	for (auto &i : l)
		value += make_yaml_value_array(i) + ",";
	value.back() = ']';
	return value;
}

template <typename T>
YamlBuilderNode
make_yaml_key(const char *keyname,
	      const std::initializer_list<std::initializer_list<T> > &l)
{
	return make_yaml_key(keyname, make_yaml_value_array(l));
}

template <typename T>
YamlBuilderNode make_yaml_key(const char *keyname,
			      const std::initializer_list<T> &l)
{
	return make_yaml_key(keyname, make_yaml_value_array(l));
}

std::string make_yaml_root(YamlBuilderNodeList &&children)
{
	return YamlBuilder({ make_yaml_key("version", "1"),
			     make_yaml_key("config", std::move(children)) });
}

template <typename T>
std::string make_simple_yaml(const char *keyname, const T &t)
{
	return make_yaml_root({ make_yaml_key(keyname, t) });
}

template <typename T>
std::string
make_simple_yaml(const char *keyname,
		 const std::initializer_list<std::initializer_list<T> > &l)
{
	return make_yaml_root({ make_yaml_key(keyname, l) });
}

template <typename T>
std::string make_simple_yaml(const char *keyname,
			     const std::initializer_list<T> &l)
{
	return make_yaml_root({ make_yaml_key(keyname, l) });
}

void UwbsConfigLoadStr::test_simple_number(const UwbsConfigModel &model,
					   const char *keyname,
					   const int64_t ok,
					   const KeyMap &expected_ok,
					   const int64_t min, const int64_t max)
{
	const std::string config_ok = make_simple_yaml(keyname, ok);
	const std::string config_lower = make_simple_yaml(keyname, min - 1);
	const std::string config_higher = make_simple_yaml(keyname, max + 1);

	// Test in range value
	ASSERT_EQ(load_str(config_ok, model), UWBS_CONFIG_STATUS_OK);
	compare(keys, expected_ok);

	// Test lower value
	ASSERT_EQ(load_str(config_lower, model),
		  UWBS_CONFIG_STATUS_INVALID_FMT);
	compare(keys, {});

	// Test higher value
	ASSERT_EQ(load_str(config_higher, model),
		  UWBS_CONFIG_STATUS_INVALID_FMT);
	compare(keys, {});
}

TEST_F(UwbsConfigLoadStr, Int8)
{
	test_simple_number<int8_t>("int8");
}

TEST_F(UwbsConfigLoadStr, Uint8)
{
	test_simple_number<uint8_t>("uint8");
}

TEST_F(UwbsConfigLoadStr, Int16)
{
	test_simple_number<int16_t>("int16");
}

TEST_F(UwbsConfigLoadStr, Uint16)
{
	test_simple_number<uint16_t>("uint16");
}

TEST_F(UwbsConfigLoadStr, Int32)
{
	test_simple_number<int32_t>("int32");
}

TEST_F(UwbsConfigLoadStr, Uint32)
{
	test_simple_number<uint32_t>("uint32");
}

TEST_F(UwbsConfigLoadStr, Bool)
{
	static const char *true_values[] = { "1",    "y",   "Y",    "yes",
					     "Yes",  "YES", "true", "True",
					     "TRUE", "on",  "On",   "ON" };
	static const char *false_values[] = { "n",   "N",     "no",    "No",
					      "NO",  "false", "False", "FALSE",
					      "off", "Off",   "OFF" };
	static const char *wrong_values[] = { "foo", "bar",    "09",	"12",
					      "42",  "true42", "false!" };

	static const UwbsConfigModel model = make_model(
		make_model_def_root({}, { make_model_boolean("my_bool") }));

	KeyMap expected;

	expected.emplace(make_key_bool("my_bool", true));
	for (auto true_value : true_values) {
		const std::string yaml =
			make_simple_yaml("my_bool", true_value);
		ASSERT_EQ(load_str(yaml, model), UWBS_CONFIG_STATUS_OK);
		compare(keys, expected);
	}

	expected.clear();
	expected.emplace(make_key_bool("my_bool", false));
	for (auto false_value : false_values) {
		const std::string yaml =
			make_simple_yaml("my_bool", false_value);
		ASSERT_EQ(load_str(yaml, model), UWBS_CONFIG_STATUS_OK);
		compare(keys, expected);
	}

	expected.clear();
	for (auto wrong_value : wrong_values) {
		const std::string yaml =
			make_simple_yaml("my_bool", wrong_value);
		ASSERT_EQ(load_str(yaml, model),
			  UWBS_CONFIG_STATUS_INVALID_FMT);
		compare(keys, {});
	}
}

TEST_F(UwbsConfigLoadStr, Enum)
{
	static const char *keyname = "val";
	static const UwbsConfigModel model = make_model(make_model_def_root(
		{ make_model_def_enum("my_enum", { { "ENUM_1", 1 },
						   { "ENUM_42", 42 },
						   { "ENUM_100", 100 } }) },
		{ make_model_ref(keyname, "#/$defs/my_enum") }));

	struct GoodValue {
		const char *str;
		uint8_t v;
	};

	static const GoodValue good_values[] = { { "ENUM_1", 1 },
						 { "ENUM_42", 42 },
						 { "ENUM_100", 100 } };
	static const char *wrong_values[] = { "foo", "bar", "ENUM_15" };
	KeyMap expected;

	for (const GoodValue &good_value : good_values) {
		expected.clear();
		expected.emplace(make_key_uint8(keyname, good_value.v));

		std::string yaml = make_simple_yaml(keyname, good_value.str);
		ASSERT_EQ(load_str(yaml, model), UWBS_CONFIG_STATUS_OK);
		compare(keys, expected);
	}

	for (const auto wrong_value : wrong_values) {
		std::string yaml = make_simple_yaml(keyname, wrong_value);
		ASSERT_EQ(load_str(yaml, model),
			  UWBS_CONFIG_STATUS_INVALID_FMT);
		compare(keys, {});
	}
}

TEST_F(UwbsConfigLoadStr, OneDimensionArray)
{
	static const char *keyname = "arr1";
	const UwbsConfigModel model = make_model(make_model_def_root(
		{},
		{ make_model_array(
			keyname,
			make_model_number(
				NULL, UWBS_CONFIG_UNDERLYING_TYPE_UINT8, 1, 23),
			4) }));

	std::string yaml = make_simple_yaml(keyname, { 1, 2, 3, 4 });
	KeyMap expected;
	expected.emplace(make_key_array<uint8_t>(keyname, { 1, 2, 3, 4 }));
	ASSERT_EQ(load_str(yaml, model), UWBS_CONFIG_STATUS_OK);
	compare(keys, expected);
}

TEST_F(UwbsConfigLoadStr, TwoDimensionArray)
{
	static const char *keyname = "arr1";
	const UwbsConfigModel model = make_model(make_model_def_root(
		{},
		{ make_model_array(
			keyname,
			make_model_array(
				make_model_number(
					NULL, UWBS_CONFIG_UNDERLYING_TYPE_UINT8,
					1, 23),
				4),
			2) }));

	std::string yaml =
		make_simple_yaml(keyname, { { 1, 2, 3, 4 }, { 5, 6, 7, 8 } });
	KeyMap expected;
	expected.emplace(make_key_array<uint8_t>(keyname, { { 1, 2, 3, 4 },
							    { 5, 6, 7, 8 } }));
	ASSERT_EQ(load_str(yaml, model), UWBS_CONFIG_STATUS_OK);
	compare(keys, expected);
}

TEST_F(UwbsConfigLoadStr, OneDimensionArrayTooSmall)
{
	static const char *keyname = "arr1";
	const UwbsConfigModel model = make_model(make_model_def_root(
		{},
		{ make_model_array(
			keyname,
			make_model_number(
				NULL, UWBS_CONFIG_UNDERLYING_TYPE_UINT8, 1, 23),
			4) }));

	std::string yaml = make_simple_yaml(keyname, { 1, 2, 3 });
	KeyMap expected;
	ASSERT_EQ(load_str(yaml, model), UWBS_CONFIG_STATUS_INVALID_FMT);
	compare(keys, expected);
}

TEST_F(UwbsConfigLoadStr, OneDimensionArrayTooLarge)
{
	static const char *keyname = "arr1";
	const UwbsConfigModel model = make_model(make_model_def_root(
		{},
		{ make_model_array(
			keyname,
			make_model_number(
				NULL, UWBS_CONFIG_UNDERLYING_TYPE_UINT8, 1, 23),
			4) }));

	std::string yaml = make_simple_yaml(keyname, { 1, 2, 3, 4, 5 });
	KeyMap expected;
	ASSERT_EQ(load_str(yaml, model), UWBS_CONFIG_STATUS_INVALID_FMT);
	compare(keys, expected);
}

TEST_F(UwbsConfigLoadStr, Bitfield8)
{
	const UwbsConfigModel model = make_model(make_model_def_root(
		{ make_model_def_bitfield(
			"the_bf1",
			{
				make_model_number(
					"field1",
					UWBS_CONFIG_UNDERLYING_TYPE_NULL, 0, 1),
				make_model_number(
					"field2",
					UWBS_CONFIG_UNDERLYING_TYPE_NULL, 0, 1),
				make_model_number(
					"field3",
					UWBS_CONFIG_UNDERLYING_TYPE_NULL, 0, 31),
				make_model_number(
					"field4",
					UWBS_CONFIG_UNDERLYING_TYPE_NULL, 0, 1),
			}) },
		{ make_model_ref("bf1", "#/$defs/the_bf1") }));
	ASSERT_EQ(load_str(make_yaml_root({ make_yaml_key(
				   "bf1", { make_yaml_key("field1", 1),
					    make_yaml_key("field2", 0),
					    make_yaml_key("field3", 30),
					    make_yaml_key("field4", 1) }) }),
			   model),
		  UWBS_CONFIG_STATUS_OK);
	KeyMap expected;
	expected.emplace(
		make_key_uint8("bf1", 1 + (0 << 1) + (30 << 2) + (1 << 7)));
	compare(keys, expected);
}

TEST_F(UwbsConfigLoadStr, Bitfield16)
{
	const UwbsConfigModel model = make_model(make_model_def_root(
		{ make_model_def_bitfield(
			"the_bf1",
			{
				make_model_number(
					"field1",
					UWBS_CONFIG_UNDERLYING_TYPE_NULL, 0, 7),
				make_model_number(
					"field2",
					UWBS_CONFIG_UNDERLYING_TYPE_NULL, 0, 7),
				make_model_number(
					"field3",
					UWBS_CONFIG_UNDERLYING_TYPE_NULL, 0, 15),
			}) },
		{ make_model_ref("bf1", "#/$defs/the_bf1") }));
	ASSERT_EQ(load_str(make_yaml_root({ make_yaml_key(
				   "bf1", { make_yaml_key("field1", 3),
					    make_yaml_key("field2", 7),
					    make_yaml_key("field3", 14) }) }),
			   model),
		  UWBS_CONFIG_STATUS_OK);
	KeyMap expected;
	expected.emplace(make_key_uint16("bf1", 3 + (7 << 3) + (14 << 6)));
	compare(keys, expected);
}

TEST_F(UwbsConfigLoadStr, Bitfield32)
{
	const UwbsConfigModel model = make_model(make_model_def_root(
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
	ASSERT_EQ(load_str(make_yaml_root({ make_yaml_key(
				   "bf1", { make_yaml_key("field1", 1),
					    make_yaml_key("field2", 255),
					    make_yaml_key("field3", 1023) }) }),
			   model),
		  UWBS_CONFIG_STATUS_OK);
	KeyMap expected;
	expected.emplace(make_key_uint32("bf1", 1 + (255 << 1) + (1023 << 9)));
	compare(keys, expected);
}

TEST_F(UwbsConfigLoadStr, BitfieldInObject)
{
	/* Test bitfield loader release parser correctly. */
	const UwbsConfigModel model = make_model(make_model_def_root(
		{ make_model_def_bitfield(
			  "the_bf1",
			  {
				  make_model_number(
					  "field1",
					  UWBS_CONFIG_UNDERLYING_TYPE_NULL, 0,
					  1),
				  make_model_number(
					  "field2",
					  UWBS_CONFIG_UNDERLYING_TYPE_NULL, 0,
					  255),
				  make_model_number(
					  "field3",
					  UWBS_CONFIG_UNDERLYING_TYPE_NULL, 0,
					  1023),
			  }),
		  make_model_def_object(
			  "my_object",
			  { make_model_number<int8_t>("field4"),
			    make_model_ref("bf1", "#/$defs/the_bf1"),
			    make_model_number<int16_t>("field5") }) },
		{ make_model_number<int32_t>("field0"),
		  make_model_ref("obj1", "#/$defs/my_object"),
		  make_model_number<uint32_t>("field6") }));
	/*const char yaml[] = "field0: 42\n"*/
	/*		    "obj1:\n"*/
	/*		    "  field4: 40\n"*/
	/*		    "  bf1:\n"*/
	/*		    "    field1: 1\n"*/
	/*		    "    field2: 255\n"*/
	/*		    "    field3: 1023\n"*/
	/*		    "  field5: 500\n"*/
	/*		    "field6: 600\n";*/
	ASSERT_EQ(load_str(make_yaml_root(
				   { make_yaml_key("field0", 42),
				     make_yaml_key(
					     "obj1",
					     { make_yaml_key("field4", 40),
					       make_yaml_key(
						       "bf1",
						       { make_yaml_key("field1",
								       1),
							 make_yaml_key("field2",
								       255),
							 make_yaml_key("field3",
								       1023) }),
					       make_yaml_key("field5", 500) }),
				     make_yaml_key("field6", 600) }),
			   model),
		  UWBS_CONFIG_STATUS_OK);
	KeyMap expected;
	expected.emplace(make_key_int32("field0", 42));
	expected.emplace(make_key_int8("obj1.field4", 40));
	expected.emplace(
		make_key_uint32("obj1.bf1", 1 + (255 << 1) + (1023 << 9)));
	expected.emplace(make_key_int16("obj1.field5", 500));
	expected.emplace(make_key_uint32("field6", 600));
	compare(keys, expected);
}

TEST_F(UwbsConfigLoadStr, BitfieldMissingField)
{
	const UwbsConfigModel model = make_model(make_model_def_root(
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
	/*const char yaml[] = "bf1:\n"*/
	/*		    "  field1: 1\n"*/
	/*		    "  field3: 1023\n";*/
	ASSERT_EQ(load_str(make_yaml_root({ make_yaml_key(
				   "bf1", { make_yaml_key("field1", 1),
					    make_yaml_key("field3", 1023) }) }),
			   model),
		  UWBS_CONFIG_STATUS_INVALID_FMT);
	KeyMap expected;
	compare(keys, expected);
}
