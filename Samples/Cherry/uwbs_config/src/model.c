/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "model.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define REF_SEP '/'

static void dyn_string_destructor(char **str)
{
	free(*str);
}

struct json_schema_type {
	enum uwbs_config_model_type type;
	const char *str;
};

static const struct json_schema_type json_schema_type_values[] = {
	{ UWBS_CONFIG_TYPE_NULL, "null" },
	{ UWBS_CONFIG_TYPE_STRING, "string" },
	{ UWBS_CONFIG_TYPE_INTEGER, "integer" },
	{ UWBS_CONFIG_TYPE_OBJECT, "object" },
	{ UWBS_CONFIG_TYPE_ARRAY, "array" },
	{ UWBS_CONFIG_TYPE_BOOLEAN, "boolean" },
};

bool uwbs_config_model_type_from_str(const char *str,
				     enum uwbs_config_model_type *type)
{
	const struct json_schema_type *iter = json_schema_type_values;
	const struct json_schema_type *last =
		json_schema_type_values +
		sizeof(json_schema_type_values) /
			sizeof(json_schema_type_values[0]);
	for (; iter != last; ++iter) {
		if (strcmp(str, iter->str) == 0) {
			*type = iter->type;
			return true;
		}
	}
	return false;
}

struct json_schema_underlying_type {
	enum uwbs_config_model_underlying_type utype;
	const char *str;
};

static const struct json_schema_underlying_type
	json_schema_underlying_type_values[] = {
		{ UWBS_CONFIG_UNDERLYING_TYPE_NULL, "null" },
		{ UWBS_CONFIG_UNDERLYING_TYPE_BITFIELD, "bitfield" },
		{ UWBS_CONFIG_UNDERLYING_TYPE_INT8, "int8_t" },
		{ UWBS_CONFIG_UNDERLYING_TYPE_UINT8, "uint8_t" },
		{ UWBS_CONFIG_UNDERLYING_TYPE_INT16, "int16_t" },
		{ UWBS_CONFIG_UNDERLYING_TYPE_UINT16, "uint16_t" },
		{ UWBS_CONFIG_UNDERLYING_TYPE_INT32, "int32_t" },
		{ UWBS_CONFIG_UNDERLYING_TYPE_UINT32, "uint32_t" },
	};

const char *uwbs_config_model_underlying_type_to_str(
	const enum uwbs_config_model_underlying_type utype)
{
	const struct json_schema_underlying_type *iter =
		json_schema_underlying_type_values;
	const struct json_schema_underlying_type *last =
		json_schema_underlying_type_values +
		sizeof(json_schema_underlying_type_values) /
			sizeof(json_schema_underlying_type_values[0]);
	for (; iter != last; ++iter) {
		if (utype == iter->utype)
			return iter->str;
	}
	return NULL;
}

bool uwbs_config_model_underlying_type_from_str(
	const char *str, enum uwbs_config_model_underlying_type *utype)
{
	const struct json_schema_underlying_type *iter =
		json_schema_underlying_type_values;
	const struct json_schema_underlying_type *last =
		json_schema_underlying_type_values +
		sizeof(json_schema_underlying_type_values) /
			sizeof(json_schema_underlying_type_values[0]);
	for (; iter != last; ++iter) {
		if (strcmp(str, iter->str) == 0) {
			*utype = iter->utype;
			return true;
		}
	}
	return false;
}

size_t uwbs_config_model_underlying_type_size(
	const enum uwbs_config_model_underlying_type utype)
{
	switch (utype) {
	case UWBS_CONFIG_UNDERLYING_TYPE_NULL:
		break;
	case UWBS_CONFIG_UNDERLYING_TYPE_INT8:
	case UWBS_CONFIG_UNDERLYING_TYPE_UINT8:
		return sizeof(int8_t);
	case UWBS_CONFIG_UNDERLYING_TYPE_INT16:
	case UWBS_CONFIG_UNDERLYING_TYPE_UINT16:
		return sizeof(int16_t);
	case UWBS_CONFIG_UNDERLYING_TYPE_BITFIELD:
	case UWBS_CONFIG_UNDERLYING_TYPE_INT32:
	case UWBS_CONFIG_UNDERLYING_TYPE_UINT32:
		return sizeof(int32_t);
	}
	assert(false);
	return 0;
}

void uwbs_config_model_def_underlying_enum_init(
	struct uwbs_config_model_def_underlying_enum *e)
{
	*e = (struct uwbs_config_model_def_underlying_enum){ 0 };
}

void uwbs_config_model_def_underlying_enum_destroy(
	struct uwbs_config_model_def_underlying_enum *e)
{
	free(e->name);
}

#define MODEL_NODE_STATIC_INIT() ((struct uwbs_config_model_node){ 0 })

static void uwbs_config_model_node_destroy(struct uwbs_config_model_node *node)
{
	free(node->name);
	node->name = NULL;
}

void uwbs_config_model_property_init(struct uwbs_config_model_property *prop)
{
	*prop = (struct uwbs_config_model_property){
		.node = MODEL_NODE_STATIC_INIT(),
		.minimum = INT64_MIN,
		.maximum = INT64_MAX,
	};
}

void uwbs_config_model_property_destroy(struct uwbs_config_model_property *prop)
{
	uwbs_config_model_node_destroy(&prop->node);
	free(prop->ref);
	if (prop->items) {
		uwbs_config_model_property_destroy(prop->items);
		free(prop->items);
	}
}

void uwbs_config_model_def_init(struct uwbs_config_model_def *def)
{
	*def = (struct uwbs_config_model_def){
		.node = MODEL_NODE_STATIC_INIT(),
		.defs = VECTOR_INIT_STATIC,
		.underlying_enums = VECTOR_INIT_STATIC,
		.properties = VECTOR_INIT_STATIC,
		.required = VECTOR_INIT_STATIC,
	};
}

void uwbs_config_model_def_destroy(struct uwbs_config_model_def *def)
{
	uwbs_config_model_node_destroy(&def->node);
	VECTOR_DESTROY(def->defs, uwbs_config_model_def_destroy);
	VECTOR_DESTROY(def->underlying_enums,
		       uwbs_config_model_def_underlying_enum_destroy);
	VECTOR_DESTROY(def->properties, uwbs_config_model_property_destroy);
	VECTOR_DESTROY(def->required, dyn_string_destructor);
}

struct uwbs_config_model_property *
uwbs_config_model_def_find_property(const struct uwbs_config_model_def *def,
				    const char *name)
{
	for (struct uwbs_config_model_property *
		     iter = VECTOR_BEGIN(def->properties),
		    *last = VECTOR_END(def->properties);
	     iter != last; ++iter) {
		if (strcmp(iter->node.name, name) == 0)
			return iter;
	}
	return NULL;
}

void uwbs_config_model_init(struct uwbs_config_model *model)
{
	uwbs_config_model_def_init(&model->root);
}

void uwbs_config_model_destroy(struct uwbs_config_model *model)
{
	uwbs_config_model_def_destroy(&model->root);
}

const struct uwbs_config_model_def *
uwbs_config_model_find_def(const struct uwbs_config_model *model,
			   const char *name)
{
	static const char def_root[] = "#/$defs/";
	const char *next;
	size_t len;
	const struct uwbs_config_model_def *def = &model->root;
	const struct uwbs_config_model_def *iter;
	const struct uwbs_config_model_def *last;

	if (!name)
		return NULL;

	if (strncmp(def_root, name, sizeof(def_root) - 1) != 0) {
		return NULL;
	}

	name += sizeof(def_root) - 1;

	while (name) {
		next = strchr(name, REF_SEP);
		len = next - name;
		for (iter = VECTOR_BEGIN(def->defs),
		    last = VECTOR_END(def->defs);
		     iter != last; ++iter) {
			if (strncmp(iter->node.name, name, len) == 0) {
				def = iter;
				break;
			}
		}

		if (iter == last) {
			assert(false);
			return NULL;
		}
		name = next ? next + 1 : next;
	}

	return def;
}

size_t uwbs_config_model_bitfield_get_field_bit_count(
	const struct uwbs_config_model_property *field)
{
	size_t bits = 0;
	size_t value;

	if (field->node.type != UWBS_CONFIG_TYPE_INTEGER ||
	    field->node.underlying_type != UWBS_CONFIG_UNDERLYING_TYPE_NULL)
		return 0;

	for (value = field->maximum; value; value = value >> 1)
		++bits;

	return bits;
}
