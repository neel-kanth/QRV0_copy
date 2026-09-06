/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <cstdint>
#include <gtest/gtest.h>
#include <initializer_list>
#include <limits>
#include <type_traits>

extern "C" {
#include "model.h"
#include "vector.h"
}

#include "test_compare.h"

template <typename T> class VectorBuilder {
    protected:
	template <typename V>
	VectorBuilder(V &v)
		: v_((struct uwbs_config_vector &)(v))
	{
	}

	template <typename Ctr>
	void assign(const std::initializer_list<T> &l, Ctr ctr)
	{
		bool ret;
		ret = uwbs_config_vector_resize(
			&v_, sizeof(T), l.size(),
			(uwbs_config_vector_item_ctr)ctr);
		ASSERT_TRUE(ret);
		size_t i = 0;
		for (auto &e : l)
			((T *)v_.arr)[i++] = e;
	}

    private:
	struct uwbs_config_vector &v_;
};

class PropertiesBuilder
	: public VectorBuilder<struct uwbs_config_model_property> {
    public:
	template <typename V>
	PropertiesBuilder(V &v)
		: VectorBuilder<struct uwbs_config_model_property>(v)
	{
	}

	PropertiesBuilder &
	operator=(const std::initializer_list<struct uwbs_config_model_property>
			  &l)
	{
		assign(l, uwbs_config_model_property_init);
		return *this;
	}
};

class DefinitionsBuilder : public VectorBuilder<struct uwbs_config_model_def> {
    public:
	template <typename V>
	DefinitionsBuilder(V &v)
		: VectorBuilder<struct uwbs_config_model_def>(v)
	{
	}

	DefinitionsBuilder &
	operator=(const std::initializer_list<struct uwbs_config_model_def> &l)
	{
		assign(l, uwbs_config_model_def_init);
		return *this;
	}
};

class UwbsConfigModel {
    public:
	UwbsConfigModel()
	{
		uwbs_config_model_init(&model_);
	}

	UwbsConfigModel(struct uwbs_config_model &&model)
	{
		model_ = model;
	}

	~UwbsConfigModel()
	{
		uwbs_config_model_destroy(&model_);
	}

	const struct uwbs_config_model &model() const
	{
		return model_;
	}

	operator const struct uwbs_config_model &() const
	{
		return model_;
	}

	operator const struct uwbs_config_model *() const
	{
		return &model_;
	}

    private:
	struct uwbs_config_model model_;
};

inline void empty_constructor(void *p)
{
}

inline struct uwbs_config_model_def _make_model_def(
	const char *name, const enum uwbs_config_model_type type,
	std::initializer_list<struct uwbs_config_model_def> &&defs,
	std::initializer_list<struct uwbs_config_model_property> &&props)
{
	struct uwbs_config_model_def def;
	bool ret;
	uwbs_config_model_def_init(&def);
	def.node.name = name ? strdup(name) : nullptr;
	def.node.type = type;
	VECTOR_INC_SIZE(def.properties, props.size(),
			uwbs_config_model_property_init, ret);
	assert(ret);
	size_t i = 0;
	for (auto const &prop : props) {
		struct uwbs_config_model_property &p =
			*VECTOR_AT(def.properties, i);
		++i;
		p = prop;
	}

	VECTOR_INC_SIZE(def.defs, defs.size(), uwbs_config_model_def_init, ret);
	assert(ret);
	i = 0;
	for (auto const &child : defs) {
		*VECTOR_AT(def.defs, i) = child;
		++i;
	}
	return def;
}

inline struct uwbs_config_model_def make_model_def_root(
	std::initializer_list<struct uwbs_config_model_def> &&defs,
	std::initializer_list<struct uwbs_config_model_property> &&props)
{
	return _make_model_def(nullptr, UWBS_CONFIG_TYPE_OBJECT,
			       std::move(defs), std::move(props));
}

inline struct uwbs_config_model_def make_model_def_object(
	const char *name,
	std::initializer_list<struct uwbs_config_model_property> &&props)
{
	return _make_model_def(name, UWBS_CONFIG_TYPE_OBJECT, {},
			       std::move(props));
}

inline struct uwbs_config_model_def make_model_def_enum(
	const char *name,
	const std::initializer_list<std::pair<const char *, uint8_t> > &values)
{
	struct uwbs_config_model_def def;
	uwbs_config_model_def_init(&def);
	bool ret;
	def.node.name = strdup(name);
	def.node.type = UWBS_CONFIG_TYPE_STRING;
	VECTOR_INC_SIZE(def.underlying_enums, values.size(),
			uwbs_config_model_def_underlying_enum_init, ret);
	assert(ret);
	size_t i = 0;
	for (auto const &[k, v] : values) {
		struct uwbs_config_model_def_underlying_enum &e =
			*VECTOR_AT(def.underlying_enums, i);
		e.name = strdup(k);
		e.value = v;
		++i;
	}
	return def;
}

inline struct uwbs_config_model_def make_model_def_bitfield(
	const char *name,
	std::initializer_list<struct uwbs_config_model_property> &&props)
{
	bool ret;
	struct uwbs_config_model_def bf =
		make_model_def_object(name, std::move(props));

	bf.node.underlying_type = UWBS_CONFIG_UNDERLYING_TYPE_BITFIELD;
	for (size_t i = 0; i < VECTOR_SIZE(bf.properties); ++i) {
		struct uwbs_config_model_property *prop =
			VECTOR_AT(bf.properties, i);
		prop->node.underlying_type = UWBS_CONFIG_UNDERLYING_TYPE_NULL;
		VECTOR_INC_SIZE(bf.required, 1, empty_constructor, ret);
		assert(ret);
		*VECTOR_BACK(bf.required) = strdup(prop->node.name);
	}
	return bf;
}

inline struct uwbs_config_model_property
make_model_number(const char *name,
		  const enum uwbs_config_model_underlying_type underlying_type)
{
	struct uwbs_config_model_property prop;
	uwbs_config_model_property_init(&prop);
	prop.node.name = name ? strdup(name) : NULL;
	prop.node.type = UWBS_CONFIG_TYPE_INTEGER;
	prop.node.underlying_type = underlying_type;
	return prop;
}

inline struct uwbs_config_model_property
make_model_number(const char *name,
		  const enum uwbs_config_model_underlying_type underlying_type,
		  const int64_t min, const int64_t max)
{
	struct uwbs_config_model_property prop =
		make_model_number(name, underlying_type);
	prop.minimum = min;
	prop.maximum = max;
	return prop;
}

template <typename T>
enum uwbs_config_model_underlying_type model_underlying_type();

#define MODEL_UNDERLYING_TYPE(Type, UTYPE)                  \
	template <>                                         \
	inline enum uwbs_config_model_underlying_type       \
	model_underlying_type<Type>() {                     \
		return UWBS_CONFIG_UNDERLYING_TYPE_##UTYPE; \
	}

MODEL_UNDERLYING_TYPE(int8_t, INT8)
MODEL_UNDERLYING_TYPE(uint8_t, UINT8)
MODEL_UNDERLYING_TYPE(int16_t, INT16)
MODEL_UNDERLYING_TYPE(uint16_t, UINT16)
MODEL_UNDERLYING_TYPE(int32_t, INT32)
MODEL_UNDERLYING_TYPE(uint32_t, UINT32)

template <typename T>
inline struct uwbs_config_model_property make_model_number(const char *name)
{
	return make_model_number(name, model_underlying_type<T>());
}

template <typename T>
inline struct uwbs_config_model_property
make_model_number(const char *name, const int64_t min, const int64_t max)
{
	return make_model_number(name, model_underlying_type<T>(), min, max);
}

inline struct uwbs_config_model_property make_model_boolean(const char *name)
{
	struct uwbs_config_model_property prop;
	uwbs_config_model_property_init(&prop);
	prop.node.name = name ? strdup(name) : NULL;
	prop.node.type = UWBS_CONFIG_TYPE_BOOLEAN;
	return prop;
}

inline struct uwbs_config_model_property
make_model_array(const char *name, struct uwbs_config_model_property items,
		 uint32_t max_items)
{
	struct uwbs_config_model_property prop;
	uwbs_config_model_property_init(&prop);
	prop.node.name = name ? strdup(name) : nullptr;
	prop.node.type = UWBS_CONFIG_TYPE_ARRAY;
	prop.max_items = max_items;
	prop.items = (struct uwbs_config_model_property *)malloc(sizeof(items));
	(*prop.items) = items;
	return prop;
}

inline struct uwbs_config_model_property
make_model_array(struct uwbs_config_model_property items, uint32_t max_items)
{
	return make_model_array(nullptr, items, max_items);
}

inline struct uwbs_config_model_property make_model_ref(const char *name,
							const char *refname)
{
	struct uwbs_config_model_property prop;
	uwbs_config_model_property_init(&prop);
	prop.node.name = strdup(name);
	prop.ref = strdup(refname);
	return prop;
}

inline UwbsConfigModel make_model(struct uwbs_config_model_def &&root)
{
	return (struct uwbs_config_model){ root };
}

inline UwbsConfigModel make_model()
{
	struct uwbs_config_model model;
	uwbs_config_model_init(&model);
	return model;
}

template <typename T> void compare_vector(const T &t1, const T &t2);

template <typename T> void compare(const T *p1, const T *p2)
{
	ASSERT_EQ(p1 != NULL, p2 != NULL);
	if (p1)
		compare(*p1, *p2);
}

inline void compare(const char *s1, const char *s2)
{
	ASSERT_STREQ(s1, s2);
}

inline void compare(const struct uwbs_config_model_def_underlying_enum &d1,
		    const struct uwbs_config_model_def_underlying_enum &d2)
{
	ASSERT_STREQ(d1.name, d2.name);
	ASSERT_EQ(d1.value, d2.value);
}

inline void compare(const struct uwbs_config_model_node &d1,
		    const struct uwbs_config_model_node &d2)
{
	ASSERT_STREQ(d1.name, d2.name);
	ASSERT_EQ(d1.type, d2.type);
	ASSERT_EQ(d1.underlying_type, d2.underlying_type);
}

inline void compare(const struct uwbs_config_model_property &d1,
		    const struct uwbs_config_model_property &d2)
{
	ASSERT_STREQ(d1.ref, d2.ref);
	ASSERT_EQ(d1.minimum, d2.minimum);
	ASSERT_EQ(d1.maximum, d2.maximum);
	compare(d1.items, d2.items);
	ASSERT_EQ(d1.max_items, d2.max_items);
	ASSERT_EQ(d1.bit_size, d2.bit_size);
}

inline void compare(const struct uwbs_config_model_def &d1,
		    const struct uwbs_config_model_def &d2)
{
	compare_vector(d1.defs, d2.defs);
	compare_vector(d1.underlying_enums, d2.underlying_enums);
	compare_vector(d1.properties, d2.properties);
	compare_vector(d1.required, d2.required);
}

inline void compare(const struct uwbs_config_model &d1,
		    const struct uwbs_config_model &d2)
{
	compare(d1.root, d2.root);
}

template <typename T> void compare_vector(const T &t1, const T &t2)
{
	ASSERT_EQ(VECTOR_SIZE(t1), VECTOR_SIZE(t2));
	for (size_t i = 0; i < VECTOR_SIZE(t1); ++i) {
		assert(VECTOR_AT(t1, i));
		assert(VECTOR_AT(t2, i));
		compare(*VECTOR_AT(t1, i), *VECTOR_AT(t2, i));
	}
}
