/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include "vector.h"

struct uwbs_config_model_def_underlying_enum {
	char *name;
	uint8_t value;
};

void uwbs_config_model_def_underlying_enum_init(
	struct uwbs_config_model_def_underlying_enum *e);
void uwbs_config_model_def_underlying_enum_destroy(
	struct uwbs_config_model_def_underlying_enum *e);

/** JSON schema types. */
enum uwbs_config_model_type {
	/** Type is undefined. */
	UWBS_CONFIG_TYPE_NULL,
	/** String type is used to represent enums. */
	UWBS_CONFIG_TYPE_STRING,
	/** Integer type, see underlying type for complete C definition. */
	UWBS_CONFIG_TYPE_INTEGER,
	/** Depends on underlying type:
	 *   - Object when underlying type is NULL
	 *   - Bitfield when underlying type is BITFIELD
	 */
	UWBS_CONFIG_TYPE_OBJECT,
	/** Array */
	UWBS_CONFIG_TYPE_ARRAY,
	/** Boolean */
	UWBS_CONFIG_TYPE_BOOLEAN,
};

bool uwbs_config_model_type_from_str(const char *str,
				     enum uwbs_config_model_type *type);

/** Underlying types added by Qorvo to JSON schema. */
enum uwbs_config_model_underlying_type {
	/** Undefined. */
	UWBS_CONFIG_UNDERLYING_TYPE_NULL,
	/** Bitfield. */
	UWBS_CONFIG_UNDERLYING_TYPE_BITFIELD,
	/** C int8_t */
	UWBS_CONFIG_UNDERLYING_TYPE_INT8,
	/** C uint8_t */
	UWBS_CONFIG_UNDERLYING_TYPE_UINT8,
	/** C int16_t */
	UWBS_CONFIG_UNDERLYING_TYPE_INT16,
	/** C uint16_t */
	UWBS_CONFIG_UNDERLYING_TYPE_UINT16,
	/** C int32_t */
	UWBS_CONFIG_UNDERLYING_TYPE_INT32,
	/** C uint32_t */
	UWBS_CONFIG_UNDERLYING_TYPE_UINT32,
};

const char *uwbs_config_model_underlying_type_to_str(
	const enum uwbs_config_model_underlying_type utype);
bool uwbs_config_model_underlying_type_from_str(
	const char *str, enum uwbs_config_model_underlying_type *utype);

size_t uwbs_config_model_underlying_type_size(
	const enum uwbs_config_model_underlying_type utype);

struct uwbs_config_model_node {
	/**
	 * @name: definition name. Mandatory and checked when leaving definition node.
	 */
	char *name;
	/**
	 * @type: definition type. Must be != UWBS_CONFIG_TYPE_NULL and checked when
	 *        leaving definition node.
	 */
	enum uwbs_config_model_type type;
	/**
	 * @underlying_type: object and number description.
	 */
	enum uwbs_config_model_underlying_type underlying_type;
};

struct uwbs_config_model_property {
	struct uwbs_config_model_node node;
	char *ref;
	int64_t minimum;
	int64_t maximum;
	/* Array child items */
	struct uwbs_config_model_property *items;
	uint32_t max_items;
	uint32_t bit_size;
};

void uwbs_config_model_property_init(struct uwbs_config_model_property *prop);
void uwbs_config_model_property_destroy(struct uwbs_config_model_property *prop);

struct uwbs_config_model_def {
	/**
	 * @node: node information.
	 */
	struct uwbs_config_model_node node;
	/**
	 * @defs: sub definitions.
	 */
	VECTOR(struct uwbs_config_model_def, defs);
	/**
	 * @underlying_enums: C/C++ enum definitions with string and numeric value.
	 */
	VECTOR(struct uwbs_config_model_def_underlying_enum, underlying_enums);
	/**
	 * @properties: object children list.
	 */
	VECTOR(struct uwbs_config_model_property, properties);
	/**
	 * @required: required object children list.
	 */
	VECTOR(char *, required);
};

void uwbs_config_model_def_init(struct uwbs_config_model_def *def);
void uwbs_config_model_def_destroy(struct uwbs_config_model_def *def);

struct uwbs_config_model_property *
uwbs_config_model_def_find_property(const struct uwbs_config_model_def *def,
				    const char *name);

struct uwbs_config_model {
	struct uwbs_config_model_def root;
};

void uwbs_config_model_init(struct uwbs_config_model *uwbs_config_model);
void uwbs_config_model_destroy(struct uwbs_config_model *uwbs_config_model);

const struct uwbs_config_model_def *
uwbs_config_model_find_def(const struct uwbs_config_model *uwbs_config_model,
			   const char *name);

size_t uwbs_config_model_bitfield_get_field_bit_count(
	const struct uwbs_config_model_property *field);
