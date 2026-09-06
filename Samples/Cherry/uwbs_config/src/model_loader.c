/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "model_loader.h"

#include "loader.h"

#include <stdlib.h>
#include <yaml.h>

static enum uwbs_config_status
uwbs_config_model_loader_check_node(const struct uwbs_config_model_node *node,
				    const char *ref)
{
	/* Type is mandatory. */
	if (node->type == UWBS_CONFIG_TYPE_NULL && (!ref || !ref[0]))
		return UWBS_CONFIG_STATUS_INVALID_FMT;

	/* If type is object, underlying type should be null or bitfield. */
	if (node->type == UWBS_CONFIG_TYPE_OBJECT &&
	    node->underlying_type != UWBS_CONFIG_UNDERLYING_TYPE_NULL &&
	    node->underlying_type != UWBS_CONFIG_UNDERLYING_TYPE_BITFIELD)
		return UWBS_CONFIG_STATUS_INVALID_FMT;

	/* If type is integer, underlying type should a number or null type when part of bitfield. */
	if (node->type == UWBS_CONFIG_TYPE_INTEGER) {
		switch (node->underlying_type) {
		case UWBS_CONFIG_UNDERLYING_TYPE_BITFIELD:
			return UWBS_CONFIG_STATUS_INVALID_FMT;

		case UWBS_CONFIG_UNDERLYING_TYPE_NULL:
		case UWBS_CONFIG_UNDERLYING_TYPE_INT8:
		case UWBS_CONFIG_UNDERLYING_TYPE_UINT8:
		case UWBS_CONFIG_UNDERLYING_TYPE_INT16:
		case UWBS_CONFIG_UNDERLYING_TYPE_UINT16:
		case UWBS_CONFIG_UNDERLYING_TYPE_INT32:
		case UWBS_CONFIG_UNDERLYING_TYPE_UINT32:
			break;
		}
	}

	return UWBS_CONFIG_STATUS_OK;
}

static enum uwbs_config_status
uwbs_config_model_loader_check_def_bitfield_field(
	const struct uwbs_config_model_def *bitfield,
	const struct uwbs_config_model_property *field)
{
	bool found;
	size_t j;

	/* Check bitfield property name is present in required fields. */
	found = false;
	for (j = 0; j < VECTOR_SIZE(bitfield->required); ++j) {
		if (strcmp(*VECTOR_AT(bitfield->required, j),
			   field->node.name)) {
			found = true;
			break;
		}
	}
	if (!found)
		return UWBS_CONFIG_STATUS_INVALID_FMT;

	/* Check field type. */
	if (field->node.type != UWBS_CONFIG_TYPE_INTEGER ||
	    field->node.underlying_type != UWBS_CONFIG_UNDERLYING_TYPE_NULL)
		return UWBS_CONFIG_STATUS_INVALID_FMT;

	/* Bit field value expect only positive value. */
	if (field->minimum < 0)
		return UWBS_CONFIG_STATUS_INVALID_FMT;

	return UWBS_CONFIG_STATUS_OK;
}

static enum uwbs_config_status uwbs_config_model_loader_check_def_bitfield(
	const struct uwbs_config_model_def *bitfield)
{
	size_t i;
	const struct uwbs_config_model_property *field;
	enum uwbs_config_status st;
	size_t total_size = 0;

	/* Bitfield has no definitions. */
	if (VECTOR_SIZE(bitfield->defs) != 0)
		return UWBS_CONFIG_STATUS_INVALID_FMT;

	/* Required field must contains all bitfields children. */
	if (VECTOR_SIZE(bitfield->properties) !=
	    VECTOR_SIZE(bitfield->required))
		return UWBS_CONFIG_STATUS_INVALID_FMT;

	for (i = 0; i < VECTOR_SIZE(bitfield->properties); ++i) {
		field = VECTOR_AT(bitfield->properties, i);
		st = uwbs_config_model_loader_check_def_bitfield_field(bitfield,
								       field);
		if (st != UWBS_CONFIG_STATUS_OK)
			return st;

		total_size +=
			uwbs_config_model_bitfield_get_field_bit_count(field);
	}

	/* Check bitfield size is less or equal than 32 bits. */
	if (total_size == 0 || total_size > 32)
		return UWBS_CONFIG_STATUS_INVALID_FMT;

	return UWBS_CONFIG_STATUS_OK;
}

static enum uwbs_config_status
uwbs_config_model_loader_check_def(const struct uwbs_config_model_def *def)
{
	size_t i;
	enum uwbs_config_status st;

	st = uwbs_config_model_loader_check_node(&def->node, NULL);
	if (st != UWBS_CONFIG_STATUS_OK)
		return st;

	for (i = 0; i < VECTOR_SIZE(def->required); ++i) {
		const char *req = *VECTOR_AT(def->required, i);
		if (uwbs_config_model_def_find_property(def, req) == NULL)
			return UWBS_CONFIG_STATUS_INVALID_FMT;
	}

	if (def->node.type == UWBS_CONFIG_TYPE_OBJECT &&
	    def->node.underlying_type == UWBS_CONFIG_UNDERLYING_TYPE_BITFIELD) {
		st = uwbs_config_model_loader_check_def_bitfield(def);
		if (st != UWBS_CONFIG_STATUS_OK)
			return st;
	}

	return UWBS_CONFIG_STATUS_OK;
}

static enum uwbs_config_status uwbs_config_model_loader_check_property(
	const struct uwbs_config_model_property *prop)
{
	enum uwbs_config_status st;

	st = uwbs_config_model_loader_check_node(&prop->node, prop->ref);
	if (st != UWBS_CONFIG_STATUS_OK)
		return st;

	/* Object type property must reference a definition. */
	if (prop->node.type == UWBS_CONFIG_TYPE_OBJECT && !prop->ref)
		return UWBS_CONFIG_STATUS_INVALID_FMT;

	/* Array type property must reference items property and items property must be an array or
	 * an integer. */
	if (prop->node.type == UWBS_CONFIG_TYPE_ARRAY &&
	    (!prop->items ||
	     (prop->items->node.type != UWBS_CONFIG_TYPE_ARRAY &&
	      prop->items->node.type != UWBS_CONFIG_TYPE_INTEGER)))
		return UWBS_CONFIG_STATUS_INVALID_FMT;

	if (prop->minimum > prop->maximum)
		return UWBS_CONFIG_STATUS_INVALID_FMT;

	return UWBS_CONFIG_STATUS_OK;
}

static void empty_constructor(void *p)
{
}

static enum uwbs_config_status uwbs_config_model_loader_type_consume_event(
	struct uwbs_config_yaml_ctx *ctx, const yaml_event_t *event, void *data)
{
	enum uwbs_config_status ret = UWBS_CONFIG_STATUS_OK;
	enum uwbs_config_model_type *type = data;
	const char *str;

	switch (event->type) {
	case YAML_SCALAR_EVENT:
		str = (const char *)event->data.scalar.value;
		if (!uwbs_config_model_type_from_str(str, type))
			ret = UWBS_CONFIG_STATUS_INVALID_FMT;
		else
			uwbs_config_yaml_leave_node(ctx);
		break;
	default:
		/* Unexpected input, input file is not well formatted. */
		ret = UWBS_CONFIG_STATUS_INVALID_FMT;
		break;
	}
	return ret;
}

static enum uwbs_config_status
uwbs_config_yaml_enter_type_node(struct uwbs_config_yaml_ctx *ctx,
				 enum uwbs_config_model_type *type)
{
	if (!type) {
		/* Do not allow to use a pointer pointing a string. */
		return UWBS_CONFIG_STATUS_INTERNAL_ERROR;
	}
	return uwbs_config_yaml_enter_node(
		ctx, uwbs_config_model_loader_type_consume_event, type);
}

static enum uwbs_config_status
uwbs_config_model_loader_underlying_type_consume_event(
	struct uwbs_config_yaml_ctx *ctx, const yaml_event_t *event, void *data)
{
	enum uwbs_config_status ret = UWBS_CONFIG_STATUS_OK;
	enum uwbs_config_model_underlying_type *utype = data;
	const char *str;

	switch (event->type) {
	case YAML_SCALAR_EVENT:
		str = (const char *)event->data.scalar.value;
		if (!uwbs_config_model_underlying_type_from_str(str, utype))
			ret = UWBS_CONFIG_STATUS_INVALID_FMT;
		else
			uwbs_config_yaml_leave_node(ctx);
		break;
	default:
		/* Unexpected input, input file is not well formatted. */
		ret = UWBS_CONFIG_STATUS_INVALID_FMT;
		break;
	}
	return ret;
}

static enum uwbs_config_status uwbs_config_yaml_enter_underlying_type_node(
	struct uwbs_config_yaml_ctx *ctx,
	enum uwbs_config_model_underlying_type *utype)
{
	if (!utype) {
		/* Do not allow to use a pointer pointing a string. */
		return UWBS_CONFIG_STATUS_INTERNAL_ERROR;
	}
	return uwbs_config_yaml_enter_node(
		ctx, uwbs_config_model_loader_underlying_type_consume_event,
		utype);
}

static enum uwbs_config_status
uwbs_config_model_loader_enter_underlying_enum_node(
	struct uwbs_config_yaml_ctx *ctx, struct uwbs_config_model_def *def,
	const char *name)
{
	bool ret;
	struct uwbs_config_model_def_underlying_enum *e;

	VECTOR_INC_SIZE(def->underlying_enums, 1,
			uwbs_config_model_def_underlying_enum_init, ret);
	if (!ret)
		return UWBS_CONFIG_STATUS_OOM;

	e = VECTOR_BACK(def->underlying_enums);
	e->name = strdup(name);
	if (!e->name)
		return UWBS_CONFIG_STATUS_INTERNAL_ERROR;
	return uwbs_config_yaml_enter_uint8_node(ctx, &e->value);
}

static enum uwbs_config_status
uwbs_config_model_loader_def_underlying_enum_consume_event(
	struct uwbs_config_yaml_ctx *ctx, const yaml_event_t *event, void *data)
{
	enum uwbs_config_status ret = UWBS_CONFIG_STATUS_OK;
	const char *name;
	struct uwbs_config_model_def *def = data;

	switch (event->type) {
	case YAML_MAPPING_END_EVENT:
		uwbs_config_yaml_leave_node(ctx);
		break;
	case YAML_MAPPING_START_EVENT:
		break;
	case YAML_SCALAR_EVENT:
		name = (const char *)event->data.scalar.value;
		ret = uwbs_config_model_loader_enter_underlying_enum_node(
			ctx, def, name);
		break;
	default:
		ret = UWBS_CONFIG_STATUS_INVALID_FMT;
		break;
	}
	return ret;
}

static enum uwbs_config_status
uwbs_config_model_loader_node_consume_event(struct uwbs_config_yaml_ctx *ctx,
					    const yaml_event_t *event,
					    struct uwbs_config_model_node *node)
{
	enum uwbs_config_status ret = UWBS_CONFIG_STATUS_OK;
	const char *name;

	switch (event->type) {
	case YAML_SCALAR_EVENT:
		name = (const char *)event->data.scalar.value;
		if (strcmp(name, "underlying_type") == 0)
			return uwbs_config_yaml_enter_underlying_type_node(
				ctx, &node->underlying_type);
		else if (strcmp(name, "type") == 0)
			return uwbs_config_yaml_enter_type_node(ctx,
								&node->type);
		else
			printf("Unknown \"%s\"\n", name);
	/* fallthrough */
	default:
		/* Unexpected input, input file is not well formatted. */
		ret = UWBS_CONFIG_STATUS_INVALID_FMT;
		break;
	}
	return ret;
}

static enum uwbs_config_status uwbs_config_model_loader_property_consume_event(
	struct uwbs_config_yaml_ctx *ctx, const yaml_event_t *event, void *data)
{
	enum uwbs_config_status ret = UWBS_CONFIG_STATUS_OK;
	const char *name;
	struct uwbs_config_model_property *prop = data;

	switch (event->type) {
	case YAML_MAPPING_END_EVENT:
		uwbs_config_yaml_leave_node(ctx);
		ret = uwbs_config_model_loader_check_property(prop);
		break;
	case YAML_MAPPING_START_EVENT:
		break;
	case YAML_SCALAR_EVENT:
		name = (const char *)event->data.scalar.value;
		if (strcmp(name, "$ref") == 0)
			return uwbs_config_yaml_enter_string_node(ctx,
								  &prop->ref);
		else if (strcmp(name, "minimum") == 0)
			return uwbs_config_yaml_enter_int64_node(
				ctx, &prop->minimum);
		else if (strcmp(name, "maximum") == 0)
			return uwbs_config_yaml_enter_int64_node(
				ctx, &prop->maximum);
		else if (strcmp(name, "items") == 0) {
			if (prop->items) {
				/* Multiple "items" found, the input file is not well formatted. */
				return UWBS_CONFIG_STATUS_INVALID_FMT;
			}
			prop->items = malloc(sizeof(*prop->items));
			uwbs_config_model_property_init(prop->items);
			return uwbs_config_yaml_enter_node(
				ctx,
				uwbs_config_model_loader_property_consume_event,
				prop->items);
		} else if (strcmp(name, "title") == 0)
			return uwbs_config_yaml_skip_scalar(ctx);
		else if (strcmp(name, "maxItems") == 0)
			return uwbs_config_yaml_enter_uint32_node(
				ctx, &prop->max_items);
		else if (strcmp(name, "bit_size") == 0)
			return uwbs_config_yaml_enter_uint32_node(
				ctx, &prop->bit_size);
	/* fallthrough */
	default:
		/* Unexpected input, input file is not well formatted. */
		ret = uwbs_config_model_loader_node_consume_event(ctx, event,
								  &prop->node);
		break;
	}
	return ret;
}

static enum uwbs_config_status
uwbs_config_model_loader_enter_property_node(struct uwbs_config_yaml_ctx *ctx,
					     struct uwbs_config_model_def *def,
					     const char *name)
{
	bool ret;
	struct uwbs_config_model_property *prop;

	VECTOR_INC_SIZE(def->properties, 1, uwbs_config_model_property_init,
			ret);
	if (!ret)
		return UWBS_CONFIG_STATUS_OOM;

	prop = VECTOR_BACK(def->properties);
	prop->node.name = strdup(name);
	if (!prop->node.name)
		return UWBS_CONFIG_STATUS_INTERNAL_ERROR;

	return uwbs_config_yaml_enter_node(
		ctx, uwbs_config_model_loader_property_consume_event, prop);
}

static enum uwbs_config_status
uwbs_config_model_loader_properties_consume_event(
	struct uwbs_config_yaml_ctx *ctx, const yaml_event_t *event, void *data)
{
	enum uwbs_config_status ret = UWBS_CONFIG_STATUS_OK;
	const char *name;
	struct uwbs_config_model_def *def = data;

	switch (event->type) {
	case YAML_MAPPING_END_EVENT:
		uwbs_config_yaml_leave_node(ctx);
		break;
	case YAML_MAPPING_START_EVENT:
		break;
	case YAML_SCALAR_EVENT:
		name = (const char *)event->data.scalar.value;
		ret = uwbs_config_model_loader_enter_property_node(ctx, def,
								   name);
		break;
	default:
		/* Unexpected input, input file is not well formatted. */
		ret = UWBS_CONFIG_STATUS_INVALID_FMT;
		break;
	}
	return ret;
}

static enum uwbs_config_status
uwbs_config_model_loader_required_list_consume_event(
	struct uwbs_config_yaml_ctx *ctx, const yaml_event_t *event, void *data)
{
	bool check;
	enum uwbs_config_status ret = UWBS_CONFIG_STATUS_OK;
	const char *name;
	struct uwbs_config_model_def *def = data;
	char **required;

	switch (event->type) {
	case YAML_SEQUENCE_END_EVENT:
		uwbs_config_yaml_leave_node(ctx);
		break;
	case YAML_SEQUENCE_START_EVENT:
		break;
	case YAML_SCALAR_EVENT:
		name = (const char *)event->data.scalar.value;
		VECTOR_INC_SIZE(def->required, 1, empty_constructor, check);
		if (!check)
			return UWBS_CONFIG_STATUS_OOM;
		required = VECTOR_BACK(def->required);
		*required = strdup(name);
		break;
	default:
		/* Unexpected input, input file is not well formatted. */
		ret = UWBS_CONFIG_STATUS_INVALID_FMT;
		break;
	}
	return ret;
}

static enum uwbs_config_status
uwbs_config_model_loader_enter_def_node(struct uwbs_config_yaml_ctx *ctx,
					struct uwbs_config_model_def *def,
					const char *name);

static enum uwbs_config_status uwbs_config_model_loader_defs_consume_event(
	struct uwbs_config_yaml_ctx *ctx, const yaml_event_t *event, void *data)
{
	enum uwbs_config_status ret = UWBS_CONFIG_STATUS_OK;
	const char *name;
	struct uwbs_config_model_def *def = data;

	switch (event->type) {
	case YAML_MAPPING_END_EVENT:
		uwbs_config_yaml_leave_node(ctx);
		break;
	case YAML_MAPPING_START_EVENT:
		break;
	case YAML_SCALAR_EVENT:
		name = (const char *)event->data.scalar.value;
		ret = uwbs_config_model_loader_enter_def_node(ctx, def, name);
		break;
	default:
		/* Unexpected input, input file is not well formatted. */
		ret = UWBS_CONFIG_STATUS_INVALID_FMT;
		break;
	}
	return ret;
}

static enum uwbs_config_status uwbs_config_model_loader_def_consume_event(
	struct uwbs_config_yaml_ctx *ctx, const yaml_event_t *event, void *data)
{
	enum uwbs_config_status ret = UWBS_CONFIG_STATUS_OK;
	const char *name;
	struct uwbs_config_model_def *def = data;

	switch (event->type) {
	case YAML_MAPPING_END_EVENT:
		ret = uwbs_config_model_loader_check_def(def);
		uwbs_config_yaml_leave_node(ctx);
		break;
	case YAML_MAPPING_START_EVENT:
		break;
	case YAML_SCALAR_EVENT:
		name = (const char *)event->data.scalar.value;
		if (strcmp(name, "enum") == 0) {
			return uwbs_config_yaml_skip_node(ctx);
		} else if (strcmp(name, "underlying_enum") == 0) {
			return uwbs_config_yaml_enter_node(
				ctx,
				uwbs_config_model_loader_def_underlying_enum_consume_event,
				def);
		} else if (strcmp(name, "properties") == 0) {
			return uwbs_config_yaml_enter_node(
				ctx,
				uwbs_config_model_loader_properties_consume_event,
				def);
		} else if (strcmp(name, "required") == 0) {
			return uwbs_config_yaml_enter_node(
				ctx,
				uwbs_config_model_loader_required_list_consume_event,
				def);
		} else if (strcmp(name, "title") == 0) {
			return uwbs_config_yaml_skip_scalar(ctx);
		} else if (strcmp(name, "$defs") == 0) {
			return uwbs_config_yaml_enter_node(
				ctx,
				uwbs_config_model_loader_defs_consume_event,
				def);
		}
		/* fallthrough */
	default:
		/* Unexpected input, input file is not well formatted. */
		ret = uwbs_config_model_loader_node_consume_event(ctx, event,
								  &def->node);
		break;
	}
	return ret;
}

static enum uwbs_config_status
uwbs_config_model_loader_enter_def_node(struct uwbs_config_yaml_ctx *ctx,
					struct uwbs_config_model_def *def,
					const char *name)
{
	bool ret;
	struct uwbs_config_model_def *child_def;
	VECTOR_INC_SIZE(def->defs, 1, uwbs_config_model_def_init, ret);
	if (!ret)
		return UWBS_CONFIG_STATUS_OOM;
	child_def = VECTOR_BACK(def->defs);
	child_def->node.name = strdup(name);
	return uwbs_config_yaml_enter_node(
		ctx, uwbs_config_model_loader_def_consume_event, child_def);
}

static enum uwbs_config_status
uwbs_config_model_check_prop(const struct uwbs_config_model *model,
			     const struct uwbs_config_model_property *prop)
{
	return UWBS_CONFIG_STATUS_OK;
}

static enum uwbs_config_status
uwbs_config_model_check_def_bitfield(const struct uwbs_config_model_def *def)
{
	size_t i;
	size_t j;

	/* Bitfields does not have internal definitions. */
	if (!VECTOR_EMPTY(def->defs))
		return UWBS_CONFIG_STATUS_INVALID_FMT;

	for (i = 0; i < VECTOR_SIZE(def->properties); ++i) {
		bool found = false;
		const struct uwbs_config_model_property *prop =
			VECTOR_AT(def->properties, i);

		/* Check all bitfield properties are required. */
		for (j = 0; j < VECTOR_SIZE(def->required); ++j) {
			const char *name = *VECTOR_AT(def->required, j);
			if (strcmp(name, prop->node.name)) {
				found = true;
				break;
			}
		}
		if (!found)
			return UWBS_CONFIG_STATUS_INVALID_FMT;

		/* Check type is number and underlying_type is null. */
		if (prop->node.type != UWBS_CONFIG_TYPE_INTEGER ||
		    prop->node.underlying_type !=
			    UWBS_CONFIG_UNDERLYING_TYPE_NULL)
			return UWBS_CONFIG_STATUS_INVALID_FMT;

		/* Check type has no sub items. */
		if (prop->items)
			return UWBS_CONFIG_STATUS_INVALID_FMT;
	}

	return UWBS_CONFIG_STATUS_OK;
}

static enum uwbs_config_status
uwbs_config_model_check_def(const struct uwbs_config_model *model,
			    const struct uwbs_config_model_def *def)
{
	size_t i;
	enum uwbs_config_status ret;

	for (i = 0; i != VECTOR_SIZE(def->properties); ++i) {
		ret = uwbs_config_model_check_prop(
			model, VECTOR_AT(def->properties, i));
		if (ret != UWBS_CONFIG_STATUS_OK)
			return ret;
	}

	for (i = 0; i != VECTOR_SIZE(def->defs); ++i) {
		ret = uwbs_config_model_check_def(model,
						  VECTOR_AT(def->defs, i));
		if (ret != UWBS_CONFIG_STATUS_OK)
			return ret;
	}

	if (def->node.type == UWBS_CONFIG_TYPE_OBJECT &&
	    def->node.underlying_type == UWBS_CONFIG_UNDERLYING_TYPE_BITFIELD) {
		ret = uwbs_config_model_check_def_bitfield(def);
		if (ret != UWBS_CONFIG_STATUS_OK)
			return ret;
	}

	return UWBS_CONFIG_STATUS_OK;
}

static enum uwbs_config_status
uwbs_config_model_check(const struct uwbs_config_model *model)
{
	return uwbs_config_model_check_def(model, &model->root);
}

enum uwbs_config_status
uwbs_config_model_load_str(const char *str, const size_t size,
			   struct uwbs_config_model *model)
{
	enum uwbs_config_status ret;
	struct uwbs_config_yaml_ctx ctx;

	if (!str || size == 0 || !model)
		return UWBS_CONFIG_STATUS_INVALID_ARG;

	uwbs_config_model_init(model);
	uwbs_config_yaml_init(&ctx);
	ret = uwbs_config_yaml_load_str(
		str, size, &ctx, uwbs_config_model_loader_def_consume_event,
		&model->root);
	uwbs_config_yaml_destroy(&ctx);

	if (ret == UWBS_CONFIG_STATUS_OK)
		ret = uwbs_config_model_check(model);

	if (ret != UWBS_CONFIG_STATUS_OK)
		uwbs_config_model_destroy(model);
	return ret;
}

enum uwbs_config_status
uwbs_config_model_load_file(const char *path, struct uwbs_config_model *model)
{
	enum uwbs_config_status ret;
	struct uwbs_config_yaml_ctx ctx;

	if (!path || !model)
		return UWBS_CONFIG_STATUS_INVALID_ARG;

	uwbs_config_model_init(model);
	uwbs_config_yaml_init(&ctx);
	ret = uwbs_config_yaml_load_file(
		path, &ctx, uwbs_config_model_loader_def_consume_event,
		&model->root);
	uwbs_config_yaml_destroy(&ctx);

	if (ret == UWBS_CONFIG_STATUS_OK)
		ret = uwbs_config_model_check(model);

	if (ret != UWBS_CONFIG_STATUS_OK)
		uwbs_config_model_destroy(model);
	return ret;
}
