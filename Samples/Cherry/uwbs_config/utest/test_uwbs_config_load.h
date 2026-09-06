/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include <gtest/gtest.h>
#include <initializer_list>
#include <string>

extern "C" {
#include "uwbs_config/uwbs_config.h"
}

template <typename T> static enum uwbs_config_number_type key_type();

#define KEY_TYPE(Type, UTYPE)                                  \
	template <>                                            \
	inline enum uwbs_config_number_type key_type<Type>() { \
		return UWBS_CONFIG_NTYPE_##UTYPE;              \
	}

KEY_TYPE(int8_t, INT8)
KEY_TYPE(uint8_t, UINT8)
KEY_TYPE(int16_t, INT16)
KEY_TYPE(uint16_t, UINT16)
KEY_TYPE(int32_t, INT32)
KEY_TYPE(uint32_t, UINT32)

class UwbsConfigLoad : public testing::Test {
    public:
	struct KeyValue {
		KeyValue(enum uwbs_config_value_type _vtype,
			 enum uwbs_config_number_type _ntype, void *_value,
			 size_t _size)
			: vtype(_vtype)
			, ntype(_ntype)
			, value(_value)
			, size(_size)
		{
		}

		KeyValue(const KeyValue &) = delete;

		KeyValue(KeyValue &&other)
			: vtype(other.vtype)
			, ntype(other.ntype)
			, value(other.value)
			, size(other.size)
		{
			other.value = nullptr;
		}

		~KeyValue()
		{
			free(value);
		}

		enum uwbs_config_value_type vtype;
		enum uwbs_config_number_type ntype;
		void *value;
		size_t size;
	};

	typedef std::map<std::string, KeyValue> KeyMap;

    protected:
	void SetUp() override
	{
	}

	void TearDown() override
	{
	}

	static KeyMap::value_type
	make_key_value(const char *name,
		       const enum uwbs_config_value_type vtype,
		       const enum uwbs_config_number_type ntype, const void *v,
		       size_t size);

	static KeyMap::value_type
	make_key_number(const char *name,
			const enum uwbs_config_number_type ntype, const void *v,
			const size_t size)
	{
		return make_key_value(name, UWBS_CONFIG_VTYPE_NUMBER, ntype, v,
				      size);
	}

	template <typename T>
	static KeyMap::value_type make_key(const char *name, const T v)
	{
		return make_key_number(name, key_type<T>(), &v, sizeof(v));
	}

	static KeyMap::value_type make_key_int8(const char *name,
						const int8_t v)
	{
		return make_key_number(name, UWBS_CONFIG_NTYPE_INT8, &v,
				       sizeof(v));
	}

	static KeyMap::value_type make_key_uint8(const char *name,
						 const uint8_t v)
	{
		return make_key_number(name, UWBS_CONFIG_NTYPE_UINT8, &v,
				       sizeof(v));
	}

	static KeyMap::value_type make_key_int16(const char *name,
						 const int16_t v)
	{
		return make_key_number(name, UWBS_CONFIG_NTYPE_INT16, &v,
				       sizeof(v));
	}

	static KeyMap::value_type make_key_uint16(const char *name,
						  const uint16_t v)
	{
		return make_key_number(name, UWBS_CONFIG_NTYPE_UINT16, &v,
				       sizeof(v));
	}

	static KeyMap::value_type make_key_int32(const char *name,
						 const int32_t v)
	{
		return make_key_number(name, UWBS_CONFIG_NTYPE_INT32, &v,
				       sizeof(v));
	}

	static KeyMap::value_type make_key_uint32(const char *name,
						  const uint32_t v)
	{
		return make_key_number(name, UWBS_CONFIG_NTYPE_UINT32, &v,
				       sizeof(v));
	}

	static KeyMap::value_type make_key_bool(const char *name, const bool b)
	{
		return make_key_uint8(name, b ? 1 : 0);
	}

	template <typename T>
	static KeyMap::value_type
	make_key_array(const char *name, const std::initializer_list<T> &l)
	{
		std::vector<T> v;
		v.resize(l.size());
		auto iter = v.begin();
		for (const T &t : l)
			*(iter++) = t;
		return make_key_value(name, UWBS_CONFIG_VTYPE_ARRAY_NUMBER,
				      key_type<T>(), v.data(),
				      v.size() * sizeof(T));
	}

	template <typename T>
	static KeyMap::value_type make_key_array(
		const char *name,
		const std::initializer_list<std::initializer_list<T> > &l)
	{
		std::vector<T> v;
		v.resize(l.size() * l.begin()->size());
		auto iter = v.begin();
		for (const auto &l2 : l)
			for (const T &t : l2)
				*(iter++) = t;
		return make_key_value(name, UWBS_CONFIG_VTYPE_ARRAY_NUMBER,
				      key_type<T>(), v.data(),
				      v.size() * sizeof(T));
	}

	void on_new_key(const char *keyname, enum uwbs_config_value_type vtype,
			enum uwbs_config_number_type ntype, const void *value,
			size_t size);

	static void on_new_key(const char *keyname,
			       enum uwbs_config_value_type vtype,
			       enum uwbs_config_number_type ntype,
			       const void *value, size_t size, void *user_data);

    public:
	KeyMap keys;
};

inline void compare(const UwbsConfigLoad::KeyValue &v1,
		    const UwbsConfigLoad::KeyValue &v2)
{
	ASSERT_EQ(v1.vtype, v2.vtype);
	ASSERT_EQ(v1.ntype, v2.ntype);
	ASSERT_EQ(v1.size, v2.size);
	ASSERT_EQ(memcmp(v1.value, v2.value, v1.size), 0);
}

inline void UwbsConfigLoad::on_new_key(const char *keyname,
				       enum uwbs_config_value_type vtype,
				       enum uwbs_config_number_type ntype,
				       const void *value, size_t size)
{
	void *v = malloc(size);
	memcpy(v, value, size);
	keys.emplace(
		std::make_pair(keyname, KeyValue{ vtype, ntype, v, size }));
}

inline void UwbsConfigLoad::on_new_key(const char *keyname,
				       enum uwbs_config_value_type vtype,
				       enum uwbs_config_number_type ntype,
				       const void *value, size_t size,
				       void *user_data)
{
	static_cast<UwbsConfigLoad *>(user_data)->on_new_key(
		keyname, vtype, ntype, value, size);
}

inline UwbsConfigLoad::KeyMap::value_type UwbsConfigLoad::make_key_value(
	const char *name, const enum uwbs_config_value_type vtype,
	const enum uwbs_config_number_type ntype, const void *v, size_t size)
{
	void *value = malloc(size);
	assert(value);
	memcpy(value, v, size);
	return { name, UwbsConfigLoad::KeyValue(vtype, ntype, value, size) };
}
