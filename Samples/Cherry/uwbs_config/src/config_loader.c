/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "config_loader.h"

#include "loader.h"
#include "model.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <yaml.h>

struct uwbs_config_load_node {
	const struct uwbs_config_model_property *prop;
	const struct uwbs_config_model_def *def;
};

void uwbs_config_load_node_init(struct uwbs_config_load_node *node,
				const struct uwbs_config_model_property *prop,
				const struct uwbs_config_model_def *def)
{
	node->prop = prop;
	node->def = def;
}

void uwbs_config_load_node_destroy(struct uwbs_config_load_node *node)
{
}

static const struct uwbs_config_model_node *
uwbs_config_model_get_node(const struct uwbs_config_model_property *prop,
			   const struct uwbs_config_model_def *def)
{
	return (prop && prop->node.type != UWBS_CONFIG_TYPE_NULL ? &prop->node :
								   &def->node);
}

struct uwbs_config_load {
	const struct uwbs_config_model *model;
	uwbs_config_on_new_key_cb user_cb;
	void *user_data;

	struct uwbs_config_load_node stack[NODE_STACK_SIZE];
	size_t stack_size;

	/* Array information. */
	char *array;
	size_t array_size;
	char *array_iter;
	uint8_t array_depth;
	enum uwbs_config_number_type array_ntype;

	/* Bitfield information. */
	uint32_t bitfield_value;
	uint8_t bitfield_offset;
	uint32_t bitfield_fields_mask;

	char *keyname;
};

void uwbs_config_load_init(struct uwbs_config_load *load,
			   const struct uwbs_config_model *model,
			   const uwbs_config_on_new_key_cb user_cb,
			   void *user_data)
{
	load->model = model;
	load->user_cb = user_cb;
	load->user_data = user_data;
	load->stack_size = 0;

	load->array = NULL;

	load->keyname = NULL;
}

void uwbs_config_load_destroy(struct uwbs_config_load *load)
{
	free(load->keyname);
	free(load->array);
}

enum uwbs_config_status uwbs_config_loader_on_new_key(
	struct uwbs_config_load *load, enum uwbs_config_value_type vtype,
	enum uwbs_config_number_type ntype, const void *value, size_t size)
{
	char *keyname_iter;
	size_t len = 0;
	const struct uwbs_config_load_node *iter;
	const struct uwbs_config_load_node *last;

	/* When array mode, do not notify but duplicate value in allocated array. */
	if (load->array && value != load->array) {
		if ((size_t)(load->array_iter - load->array) + size >
		    load->array_size)
			return UWBS_CONFIG_STATUS_INVALID_FMT;
		memcpy(load->array_iter, value, size);
		load->array_iter += size;
		load->array_ntype = ntype;
		return UWBS_CONFIG_STATUS_OK;
	}

	for (iter = load->stack, last = load->stack + load->stack_size;
	     iter != last; ++iter) {
		if (!iter->prop || !iter->prop->node.name)
			continue;

		/* +1 for '.' separator or nul character. */
		len += strlen(iter->prop->node.name) + 1;
	}

	keyname_iter = realloc(load->keyname, len);
	if (!keyname_iter)
		return UWBS_CONFIG_STATUS_OOM;
	load->keyname = keyname_iter;

	for (iter = load->stack, last = load->stack + load->stack_size;
	     iter != last; ++iter) {
		if (!iter->prop || !iter->prop->node.name)
			continue;

		len = strlen(iter->prop->node.name);
		memcpy(keyname_iter, iter->prop->node.name, len);
		keyname_iter += len;
		*(keyname_iter++) = '.';
	}
	*(keyname_iter - 1) = '\0';

	load->user_cb(load->keyname, vtype, ntype, value, size,
		      load->user_data);

	return UWBS_CONFIG_STATUS_OK;
}

static enum uwbs_config_status
uwbs_config_loader_enter_node(struct uwbs_config_yaml_ctx *ctx,
			      uwbs_config_yaml_consume_event_t consume_event,
			      struct uwbs_config_load *load,
			      const struct uwbs_config_model_property *prop,
			      const struct uwbs_config_model_def *def)
{
	struct uwbs_config_load_node *child;
	if (load->stack_size == sizeof(load->stack) / sizeof(load->stack[0])) {
		assert(false);
		return UWBS_CONFIG_STATUS_INTERNAL_ERROR;
	}
	child = &load->stack[load->stack_size++];
	uwbs_config_load_node_init(child, prop, def);
	return uwbs_config_yaml_enter_node(ctx, consume_event, load);
}

static void uwbs_config_loader_leave_node(struct uwbs_config_yaml_ctx *ctx,
					  struct uwbs_config_load *load,
					  const bool force)
{
	/* When loading array, wait for YAML_SEQUENCE_END_EVENT before leaving the node. */
	if (load->array && !force)
		return;

	assert(load->stack_size > 0);
	--load->stack_size;
	uwbs_config_yaml_leave_node(ctx);
}

static struct uwbs_config_load_node *
uwbs_config_loader_get_current_node(struct uwbs_config_load *load)
{
	assert(load->stack_size > 0);
	return &load->stack[load->stack_size - 1];
}

static enum uwbs_config_status
uwbs_config_loader_node_consume_event(struct uwbs_config_yaml_ctx *ctx,
				      const yaml_event_t *event,
				      struct uwbs_config_load *load)
{
	enum uwbs_config_status ret = UWBS_CONFIG_STATUS_OK;

	switch (event->type) {
	case YAML_SEQUENCE_END_EVENT:
		if (!load->array)
			return UWBS_CONFIG_STATUS_INTERNAL_ERROR;

		if (--load->array_depth == 0) {
			if (load->array_size !=
			    (size_t)(load->array_iter - load->array)) {
				return UWBS_CONFIG_STATUS_INVALID_FMT;
			}
			ret = uwbs_config_loader_on_new_key(
				load, UWBS_CONFIG_VTYPE_ARRAY_NUMBER,
				load->array_ntype, load->array,
				load->array_size);
			free(load->array);
			load->array = NULL;
			/* Leave root array. */
			uwbs_config_loader_leave_node(ctx, load, true);
		}

		/* Leave array node. */
		uwbs_config_loader_leave_node(ctx, load, true);
		break;

	default:
		/* Unexpected input, input file is not well formatted. */
		ret = UWBS_CONFIG_STATUS_INVALID_FMT;
		break;
	}
	return ret;
}

static enum uwbs_config_status uwbs_config_loader_enter_property_node(
	struct uwbs_config_yaml_ctx *ctx, struct uwbs_config_load *load,
	const struct uwbs_config_model_property *prop,
	const struct uwbs_config_model_def *def);

static enum uwbs_config_status
uwbs_config_loader_boolean_consume_event(struct uwbs_config_yaml_ctx *ctx,
					 const yaml_event_t *event, void *data)
{
	static const char *true_values[] = { "1",    "y",   "Y",    "yes",
					     "Yes",  "YES", "true", "True",
					     "TRUE", "on",  "On",   "ON" };
	static const char *false_values[] = { "n",   "N",     "no",    "No",
					      "NO",  "false", "False", "FALSE",
					      "off", "Off",   "OFF" };

	enum uwbs_config_status ret = UWBS_CONFIG_STATUS_OK;
	struct uwbs_config_load *load = data;
	const char *value;
	struct uwbs_config_load_node *node;
	const char **iter;
	const char **last;
	uint8_t b;
	bool found;

	switch (event->type) {
	case YAML_SCALAR_EVENT:
		value = (const char *)event->data.scalar.value;
		node = &load->stack[load->stack_size - 1];
		if (node->prop->node.type != UWBS_CONFIG_TYPE_BOOLEAN &&
		    node->prop->node.underlying_type !=
			    UWBS_CONFIG_UNDERLYING_TYPE_NULL)
			return UWBS_CONFIG_STATUS_INTERNAL_ERROR;

		found = false;
		for (iter = true_values,
		    last = iter + sizeof(true_values) / sizeof(true_values[0]);
		     iter != last; ++iter) {
			if (strcmp(value, *iter) == 0) {
				found = true;
				b = true;
				break;
			}
		}

		for (iter = false_values,
		    last = iter +
			   sizeof(false_values) / sizeof(false_values[0]);
		     iter != last; ++iter) {
			if (strcmp(value, *iter) == 0) {
				found = true;
				b = false;
			}
		}

		if (!found)
			return UWBS_CONFIG_STATUS_INVALID_FMT;

		uwbs_config_loader_on_new_key(load, UWBS_CONFIG_VTYPE_NUMBER,
					      UWBS_CONFIG_NTYPE_UINT8, &b,
					      sizeof(b));
		uwbs_config_loader_leave_node(ctx, load, false);
		break;

	default:
		/* Unexpected input, forward to node. */
		ret = uwbs_config_loader_node_consume_event(ctx, event, load);
		break;
	}
	return ret;
}
static enum uwbs_config_status uwbs_config_loader_enter_boolean_node(
	struct uwbs_config_yaml_ctx *ctx, struct uwbs_config_load *load,
	const struct uwbs_config_model_property *prop,
	const struct uwbs_config_model_def *def)
{
	return uwbs_config_loader_enter_node(
		ctx, uwbs_config_loader_boolean_consume_event, load, prop, def);
}

static enum uwbs_config_status
uwbs_config_loader_integer_consume_event(struct uwbs_config_yaml_ctx *ctx,
					 const yaml_event_t *event, void *data)
{
	enum uwbs_config_status ret = UWBS_CONFIG_STATUS_OK;
	struct uwbs_config_load *load = data;
	const char *value;
	struct uwbs_config_load_node *node;
	const struct uwbs_config_model_property *prop;
	char *endptr;
	int64_t number;
	enum uwbs_config_number_type ntype = UWBS_CONFIG_NTYPE_INT8;
	int8_t int8_v;
	uint8_t uint8_v;
	int16_t int16_v;
	uint16_t uint16_v;
	int32_t int32_v;
	uint32_t uint32_v;
	const void *ntf_value = NULL;
	size_t ntf_value_size = 0;

	switch (event->type) {
	case YAML_SCALAR_EVENT:
		value = (const char *)event->data.scalar.value;
		node = &load->stack[load->stack_size - 1];
		prop = node->prop;

		if (prop->node.type != UWBS_CONFIG_TYPE_INTEGER)
			return UWBS_CONFIG_STATUS_INTERNAL_ERROR;

		number = strtoll(value, &endptr, 0);
		if (*endptr)
			return UWBS_CONFIG_STATUS_INVALID_FMT;

		if (number < prop->minimum || number > prop->maximum)
			return UWBS_CONFIG_STATUS_INVALID_FMT;

#define NUMBER(UPPER, lower)               \
	ntype = UWBS_CONFIG_NTYPE_##UPPER; \
	lower##_v = (lower##_t)number;     \
	ntf_value = &lower##_v;            \
	ntf_value_size = sizeof(lower##_v)

		switch (prop->node.underlying_type) {
		case UWBS_CONFIG_UNDERLYING_TYPE_NULL:
		case UWBS_CONFIG_UNDERLYING_TYPE_BITFIELD:
			assert(false);
			return UWBS_CONFIG_STATUS_INTERNAL_ERROR;

		case UWBS_CONFIG_UNDERLYING_TYPE_INT8:
			NUMBER(INT8, int8);
			break;

		case UWBS_CONFIG_UNDERLYING_TYPE_UINT8:
			NUMBER(UINT8, uint8);
			break;

		case UWBS_CONFIG_UNDERLYING_TYPE_INT16:
			NUMBER(INT16, int16);
			break;

		case UWBS_CONFIG_UNDERLYING_TYPE_UINT16:
			NUMBER(UINT16, uint16);
			break;

		case UWBS_CONFIG_UNDERLYING_TYPE_INT32:
			NUMBER(INT32, int32);
			break;

		case UWBS_CONFIG_UNDERLYING_TYPE_UINT32:
			NUMBER(UINT32, uint32);
			break;
		}
#undef NUMBER

		ret = uwbs_config_loader_on_new_key(load,
						    UWBS_CONFIG_VTYPE_NUMBER,
						    ntype, ntf_value,
						    ntf_value_size);

		uwbs_config_loader_leave_node(ctx, load, false);
		break;

	default:
		/* Unexpected input, forward to node. */
		ret = uwbs_config_loader_node_consume_event(ctx, event, load);
		break;
	}
	return ret;
}

static enum uwbs_config_status uwbs_config_loader_enter_integer_node(
	struct uwbs_config_yaml_ctx *ctx, struct uwbs_config_load *load,
	const struct uwbs_config_model_property *prop,
	const struct uwbs_config_model_def *def)
{
	return uwbs_config_loader_enter_node(
		ctx, uwbs_config_loader_integer_consume_event, load, prop, def);
}

static enum uwbs_config_status uwbs_config_loader_bitfield_field_consume_event(
	struct uwbs_config_yaml_ctx *ctx, const yaml_event_t *event, void *data)
{
	enum uwbs_config_status ret = UWBS_CONFIG_STATUS_OK;
	struct uwbs_config_load *load = data;
	const char *str;
	char *endptr;
	int64_t value;
	struct uwbs_config_load_node *node;

	switch (event->type) {
	case YAML_SCALAR_EVENT:
		str = (const char *)event->data.scalar.value;
		node = uwbs_config_loader_get_current_node(load);
		value = strtoll(str, &endptr, 0);
		if (*endptr)
			return UWBS_CONFIG_STATUS_INVALID_FMT;
		if (value < node->prop->minimum || value > node->prop->maximum)
			return UWBS_CONFIG_STATUS_INVALID_FMT;
		load->bitfield_value |= value << load->bitfield_offset;
		uwbs_config_loader_leave_node(ctx, load, false);
		break;

	default:
		/* Unexpected input, forward to node. */
		ret = uwbs_config_loader_node_consume_event(ctx, event, load);
		break;
	}
	return ret;
}

static enum uwbs_config_status uwbs_config_loader_enter_bitfield_field_node(
	struct uwbs_config_yaml_ctx *ctx, struct uwbs_config_load *load,
	const struct uwbs_config_model_property *prop,
	const struct uwbs_config_model_def *def)
{
	return uwbs_config_loader_enter_node(
		ctx, uwbs_config_loader_bitfield_field_consume_event, load,
		prop, def);
}

static enum uwbs_config_number_type
uwbs_config_bitfield_get_ntype(const struct uwbs_config_model_def *bf,
			       size_t *size)
{
	size_t i;
	const char *prop_name;
	const struct uwbs_config_model_property *prop;
	size_t bits = 0;

	for (i = 0; i < VECTOR_SIZE(bf->required); ++i) {
		prop_name = *VECTOR_AT(bf->required, i);
		prop = uwbs_config_model_def_find_property(bf, prop_name);
		bits += uwbs_config_model_bitfield_get_field_bit_count(prop);
	}

	if (bits <= 8) {
		*size = 1;
		return UWBS_CONFIG_NTYPE_UINT8;
	}
	if (bits <= 16) {
		*size = 2;
		return UWBS_CONFIG_NTYPE_UINT16;
	}
	*size = 4;
	return UWBS_CONFIG_NTYPE_UINT32;
}

static enum uwbs_config_status
uwbs_config_loader_bitfield_consume_event(struct uwbs_config_yaml_ctx *ctx,
					  const yaml_event_t *event, void *data)
{
	enum uwbs_config_status ret = UWBS_CONFIG_STATUS_OK;
	struct uwbs_config_load *load = data;
	const char *value;
	struct uwbs_config_load_node *node;
	const struct uwbs_config_model_property *prop;
	size_t i;
	enum uwbs_config_number_type ntype;
	size_t bf_size;
	const char *prop_name;
	uint32_t expected_mask;

	switch (event->type) {
	case YAML_MAPPING_START_EVENT:
		break;

	case YAML_MAPPING_END_EVENT:
		node = uwbs_config_loader_get_current_node(load);
		expected_mask = (1 << VECTOR_SIZE(node->def->required)) - 1;
		if (expected_mask != load->bitfield_fields_mask)
			return UWBS_CONFIG_STATUS_INVALID_FMT;
		ntype = uwbs_config_bitfield_get_ntype(node->def, &bf_size);
		ret = uwbs_config_loader_on_new_key(
			load, UWBS_CONFIG_VTYPE_NUMBER, ntype,
			&load->bitfield_value, bf_size);
		uwbs_config_loader_leave_node(ctx, load, false);
		break;

	case YAML_SCALAR_EVENT:
		value = (const char *)event->data.scalar.value;
		node = uwbs_config_loader_get_current_node(load);

		if (node->def->node.type != UWBS_CONFIG_TYPE_OBJECT ||
		    node->def->node.underlying_type !=
			    UWBS_CONFIG_UNDERLYING_TYPE_BITFIELD)
			return UWBS_CONFIG_STATUS_INTERNAL_ERROR;

		/* Find field and compute its offset in bitfield. */
		load->bitfield_offset = 0;
		for (i = 0; i < VECTOR_SIZE(node->def->required); ++i) {
			prop_name = *VECTOR_AT(node->def->required, i);
			prop = uwbs_config_model_def_find_property(node->def,
								   prop_name);
			if (!prop)
				return UWBS_CONFIG_STATUS_INTERNAL_ERROR;

			if (strcmp(prop_name, value) == 0) {
				/* Check if field has already been provided. */
				if (load->bitfield_fields_mask & (1 << i))
					return UWBS_CONFIG_STATUS_INVALID_FMT;
				/* Mark field as present. */
				load->bitfield_fields_mask |= 1 << i;
				return uwbs_config_loader_enter_bitfield_field_node(
					ctx, load, prop, NULL);
			}

			load->bitfield_offset +=
				uwbs_config_model_bitfield_get_field_bit_count(
					prop);
		}
		ret = UWBS_CONFIG_STATUS_INVALID_FMT;
		break;

	default:
		/* Unexpected input, forward to node. */
		ret = uwbs_config_loader_node_consume_event(ctx, event, load);
		break;
	}
	return ret;
}

static enum uwbs_config_status uwbs_config_loader_enter_bitfield_node(
	struct uwbs_config_yaml_ctx *ctx, struct uwbs_config_load *load,
	const struct uwbs_config_model_property *prop,
	const struct uwbs_config_model_def *def)
{
	load->bitfield_fields_mask = 0;
	load->bitfield_value = 0;
	return uwbs_config_loader_enter_node(
		ctx, uwbs_config_loader_bitfield_consume_event, load, prop,
		def);
}

static enum uwbs_config_status
uwbs_config_loader_enum_consume_event(struct uwbs_config_yaml_ctx *ctx,
				      const yaml_event_t *event, void *data)
{
	enum uwbs_config_status ret = UWBS_CONFIG_STATUS_OK;
	struct uwbs_config_load *load = data;
	const char *value;
	struct uwbs_config_load_node *node;
	const struct uwbs_config_model_def *def;
	size_t i;
	const struct uwbs_config_model_def_underlying_enum *underlying_enum;
	bool found;
	uint8_t ntf_value;

	switch (event->type) {
	case YAML_SCALAR_EVENT:
		value = (const char *)event->data.scalar.value;
		node = uwbs_config_loader_get_current_node(load);
		def = node->def;

		if (!def)
			return UWBS_CONFIG_STATUS_INTERNAL_ERROR;

		found = false;
		for (i = 0; i < VECTOR_SIZE(def->underlying_enums); ++i) {
			underlying_enum = VECTOR_AT(def->underlying_enums, i);
			if (strcmp(value, underlying_enum->name) == 0) {
				ntf_value = underlying_enum->value;
				found = true;
				break;
			}
		}

		if (!found)
			return UWBS_CONFIG_STATUS_INVALID_FMT;

		ret = uwbs_config_loader_on_new_key(
			load, UWBS_CONFIG_VTYPE_NUMBER, UWBS_CONFIG_NTYPE_UINT8,
			&ntf_value, sizeof(ntf_value));
		uwbs_config_loader_leave_node(ctx, load, false);
		break;

	default:
		/* Unexpected input, forward to node. */
		ret = uwbs_config_loader_node_consume_event(ctx, event, load);
		break;
	}
	return ret;
}

static enum uwbs_config_status uwbs_config_loader_enter_enum_node(
	struct uwbs_config_yaml_ctx *ctx, struct uwbs_config_load *load,
	const struct uwbs_config_model_property *prop,
	const struct uwbs_config_model_def *def)
{
	return uwbs_config_loader_enter_node(
		ctx, uwbs_config_loader_enum_consume_event, load, prop, def);
}

static enum uwbs_config_status
uwbs_config_loader_array_consume_event(struct uwbs_config_yaml_ctx *ctx,
				       const yaml_event_t *event, void *data)
{
	enum uwbs_config_status ret = UWBS_CONFIG_STATUS_OK;
	struct uwbs_config_load *load = data;
	struct uwbs_config_load_node *node;
	const struct uwbs_config_model_property *prop;

	switch (event->type) {
	case YAML_SEQUENCE_START_EVENT:
		node = uwbs_config_loader_get_current_node(load);

		if (node->prop->node.type != UWBS_CONFIG_TYPE_ARRAY)
			return UWBS_CONFIG_STATUS_INTERNAL_ERROR;

		ret = uwbs_config_loader_enter_property_node(
			ctx, load, node->prop->items, NULL);
		if (ret != UWBS_CONFIG_STATUS_OK)
			break;

		if (!load->array) {
			load->array_size = node->prop->max_items;
			for (prop = node->prop->items; prop;
			     prop = prop->items) {
				load->array_size *=
					(prop->node.type ==
							 UWBS_CONFIG_TYPE_ARRAY ?
						 prop->max_items :
						 uwbs_config_model_underlying_type_size(
							 prop->node
								 .underlying_type));
			}
			load->array_iter = load->array =
				malloc(load->array_size);
			if (!load->array)
				return UWBS_CONFIG_STATUS_OOM;

			load->array_depth = 1;
		} else
			++load->array_depth;
		break;

	default:
		/* Unexpected input, forward to node. */
		ret = uwbs_config_loader_node_consume_event(ctx, event, load);
		break;
	}
	return ret;
}

static enum uwbs_config_status uwbs_config_loader_enter_array_node(
	struct uwbs_config_yaml_ctx *ctx, struct uwbs_config_load *load,
	const struct uwbs_config_model_property *prop,
	const struct uwbs_config_model_def *def)
{
	return uwbs_config_loader_enter_node(
		ctx, uwbs_config_loader_array_consume_event, load, prop, def);
}

static enum uwbs_config_status
uwbs_config_loader_object_consume_event(struct uwbs_config_yaml_ctx *ctx,
					const yaml_event_t *event, void *data)
{
	enum uwbs_config_status ret = UWBS_CONFIG_STATUS_OK;
	struct uwbs_config_load *load = data;
	const char *value;
	struct uwbs_config_load_node *node;
	const struct uwbs_config_model_property *prop;
	const struct uwbs_config_model_def *def;
	const struct uwbs_config_model_node *mnode;

	switch (event->type) {
	case YAML_MAPPING_START_EVENT:
		node = uwbs_config_loader_get_current_node(load);
		mnode = uwbs_config_model_get_node(node->prop, node->def);
		if (mnode->type != UWBS_CONFIG_TYPE_OBJECT &&
		    mnode->underlying_type !=
			    UWBS_CONFIG_UNDERLYING_TYPE_BITFIELD)
			return UWBS_CONFIG_STATUS_INTERNAL_ERROR;

		break;

	case YAML_MAPPING_END_EVENT:
		uwbs_config_loader_leave_node(ctx, load, false);
		break;

	case YAML_SCALAR_EVENT:
		value = (const char *)event->data.scalar.value;
		node = uwbs_config_loader_get_current_node(load);

		prop = uwbs_config_model_def_find_property(node->def, value);
		if (!prop)
			return UWBS_CONFIG_STATUS_INVALID_KEY;

		def = uwbs_config_model_find_def(load->model, prop->ref);

		ret = uwbs_config_loader_enter_property_node(ctx, load, prop,
							     def);
		break;

	default:
		/* Unexpected input, forward to node. */
		ret = uwbs_config_loader_node_consume_event(ctx, event, load);
		break;
	}
	return ret;
}

static enum uwbs_config_status uwbs_config_loader_enter_object_node(
	struct uwbs_config_yaml_ctx *ctx, struct uwbs_config_load *load,
	const struct uwbs_config_model_property *prop,
	const struct uwbs_config_model_def *def)
{
	return uwbs_config_loader_enter_node(
		ctx, uwbs_config_loader_object_consume_event, load, prop, def);
}

static enum uwbs_config_status uwbs_config_loader_enter_property_node(
	struct uwbs_config_yaml_ctx *ctx, struct uwbs_config_load *load,
	const struct uwbs_config_model_property *prop,
	const struct uwbs_config_model_def *def)
{
	const struct uwbs_config_model_node *mnode =
		uwbs_config_model_get_node(prop, def);
	switch (mnode->type) {
	case UWBS_CONFIG_TYPE_NULL:
		return UWBS_CONFIG_STATUS_INTERNAL_ERROR;
		break;

	case UWBS_CONFIG_TYPE_STRING:
		return uwbs_config_loader_enter_enum_node(ctx, load, prop, def);

	case UWBS_CONFIG_TYPE_INTEGER:
		return uwbs_config_loader_enter_integer_node(ctx, load, prop,
							     def);

	case UWBS_CONFIG_TYPE_OBJECT:
		switch (mnode->underlying_type) {
		case UWBS_CONFIG_UNDERLYING_TYPE_NULL:
			return uwbs_config_loader_enter_object_node(ctx, load,
								    prop, def);
		case UWBS_CONFIG_UNDERLYING_TYPE_BITFIELD:
			return uwbs_config_loader_enter_bitfield_node(
				ctx, load, prop, def);

		case UWBS_CONFIG_UNDERLYING_TYPE_INT8:
		case UWBS_CONFIG_UNDERLYING_TYPE_UINT8:
		case UWBS_CONFIG_UNDERLYING_TYPE_INT16:
		case UWBS_CONFIG_UNDERLYING_TYPE_UINT16:
		case UWBS_CONFIG_UNDERLYING_TYPE_INT32:
		case UWBS_CONFIG_UNDERLYING_TYPE_UINT32:
			break;
		}
		break;

	case UWBS_CONFIG_TYPE_ARRAY:
		return uwbs_config_loader_enter_array_node(ctx, load, prop,
							   def);

	case UWBS_CONFIG_TYPE_BOOLEAN:
		return uwbs_config_loader_enter_boolean_node(ctx, load, prop,
							     def);
	}
	return UWBS_CONFIG_STATUS_INTERNAL_ERROR;
}

static enum uwbs_config_status
uwbs_config_loader_version_consume_event(struct uwbs_config_yaml_ctx *ctx,
					 const yaml_event_t *event, void *data)
{
	enum uwbs_config_status ret = UWBS_CONFIG_STATUS_OK;
	const char *str;

	switch (event->type) {
	case YAML_SCALAR_EVENT:
		str = (char *)event->data.scalar.value;
		if (str[0] != '1' || str[1] != '\0')
			ret = UWBS_CONFIG_STATUS_INVALID_FMT;
		uwbs_config_yaml_leave_node(ctx);
		break;

	default:
		/* Unexpected input. */
		ret = UWBS_CONFIG_STATUS_INVALID_FMT;
		break;
	}
	return ret;
}

static enum uwbs_config_status
uwbs_config_loader_enter_version(struct uwbs_config_yaml_ctx *ctx,
				 struct uwbs_config_load *load)
{
	return uwbs_config_yaml_enter_node(
		ctx, uwbs_config_loader_version_consume_event, load);
}

static enum uwbs_config_status
uwbs_config_loader_root_consume_event(struct uwbs_config_yaml_ctx *ctx,
				      const yaml_event_t *event, void *data)
{
	enum uwbs_config_status ret = UWBS_CONFIG_STATUS_OK;
	struct uwbs_config_load *load = data;
	const char *name;

	switch (event->type) {
	case YAML_MAPPING_START_EVENT:
		break;

	case YAML_MAPPING_END_EVENT:
		uwbs_config_yaml_leave_node(ctx);
		break;

	case YAML_SCALAR_EVENT:
		name = (const char *)event->data.scalar.value;

		if (strcmp(name, "version") == 0)
			ret = uwbs_config_loader_enter_version(ctx, load);
		else if (strcmp(name, "config") == 0)
			ret = uwbs_config_loader_enter_object_node(
				ctx, load, NULL, &load->model->root);
		else
			ret = UWBS_CONFIG_STATUS_INVALID_FMT;
		break;

	default:
		/* Unexpected input. */
		ret = UWBS_CONFIG_STATUS_INVALID_FMT;
		break;
	}
	return ret;
}

enum uwbs_config_status
uwbs_config_load_str(const char *str, const size_t size,
		     const struct uwbs_config_model *model,
		     uwbs_config_on_new_key_cb user_cb, void *user_data)
{
	enum uwbs_config_status ret;
	struct uwbs_config_yaml_ctx ctx;
	struct uwbs_config_load load;
	uwbs_config_load_init(&load, model, user_cb, user_data);

	if (!str || !model)
		return UWBS_CONFIG_STATUS_INVALID_ARG;

	uwbs_config_yaml_init(&ctx);
	ret = uwbs_config_yaml_load_str(
		str, size, &ctx, uwbs_config_loader_root_consume_event, &load);
	uwbs_config_yaml_destroy(&ctx);
	uwbs_config_load_destroy(&load);
	return ret;
}

enum uwbs_config_status
uwbs_config_load_file(const char *path, const struct uwbs_config_model *model,
		      uwbs_config_on_new_key_cb user_cb, void *user_data)
{
	enum uwbs_config_status ret;
	struct uwbs_config_yaml_ctx ctx;
	struct uwbs_config_load load;
	uwbs_config_load_init(&load, model, user_cb, user_data);

	if (!path || !model || !user_cb)
		return UWBS_CONFIG_STATUS_INVALID_ARG;

	uwbs_config_yaml_init(&ctx);
	ret = uwbs_config_yaml_load_file(
		path, &ctx, uwbs_config_loader_root_consume_event, &load);
	uwbs_config_yaml_destroy(&ctx);
	uwbs_config_load_destroy(&load);
	return ret;
}
