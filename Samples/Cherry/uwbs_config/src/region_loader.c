/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "region_loader.h"

#include "loader.h"

#include <stdbool.h>
#include <stdlib.h>
#include <yaml.h>

struct uwbs_config_region_loader {
	const char *country_code;
	char *region;
	char **region_found;
	size_t sequence_level;
};

static void
uwbs_config_region_loader_init(struct uwbs_config_region_loader *loader,
			       const char *country_code, char **region)
{
	loader->country_code = country_code;
	loader->region = NULL;
	loader->region_found = region;
	loader->sequence_level = 0;

	*region = NULL;
}

static void
uwbs_config_region_loader_destroy(struct uwbs_config_region_loader *loader)
{
	free(loader->region);
}

static enum uwbs_config_status
uwbs_config_region_loader_countries_consume_event(
	struct uwbs_config_yaml_ctx *ctx, const yaml_event_t *event, void *data)
{
	enum uwbs_config_status ret = UWBS_CONFIG_STATUS_OK;
	const char *value;
	struct uwbs_config_region_loader *loader = data;

	switch (event->type) {
	case YAML_SEQUENCE_START_EVENT:
		if (++loader->sequence_level > 1)
			return UWBS_CONFIG_STATUS_INVALID_FMT;
		break;

	case YAML_SEQUENCE_END_EVENT:
		uwbs_config_yaml_leave_node(ctx);
		--loader->sequence_level;
		free(loader->region);
		loader->region = NULL;
		break;

	case YAML_SCALAR_EVENT:
		value = (const char *)event->data.scalar.value;
		if (strcmp(value, loader->country_code) == 0) {
			/* Country code is present twice. */
			if (*loader->region_found) {
				free(*loader->region_found);
				*loader->region_found = NULL;
				return UWBS_CONFIG_STATUS_INVALID_FMT;
			}

			*loader->region_found = loader->region;
			/* Avoid to free region name when leaving current yaml sequence. */
			loader->region = NULL;
		}
		break;

	default:
		/* Unexpected input, input file is not well formatted. */
		ret = UWBS_CONFIG_STATUS_INVALID_ARG;
		break;
	}
	return ret;
}

static enum uwbs_config_status uwbs_config_region_loader_regions_consume_event(
	struct uwbs_config_yaml_ctx *ctx, const yaml_event_t *event, void *data)
{
	enum uwbs_config_status ret = UWBS_CONFIG_STATUS_OK;
	const char *value;
	struct uwbs_config_region_loader *loader = data;

	switch (event->type) {
	case YAML_MAPPING_START_EVENT:
		break;

	case YAML_MAPPING_END_EVENT:
		uwbs_config_yaml_leave_node(ctx);
		break;

	case YAML_SCALAR_EVENT:
		value = (const char *)event->data.scalar.value;
		loader->region = strdup(value);
		ret = uwbs_config_yaml_enter_node(
			ctx, uwbs_config_region_loader_countries_consume_event,
			loader);
		break;

	default:
		/* Unexpected input, input file is not well formatted. */
		ret = UWBS_CONFIG_STATUS_INVALID_ARG;
		break;
	}
	return ret;
}

static enum uwbs_config_status uwbs_config_region_loader_version_consume_event(
	struct uwbs_config_yaml_ctx *ctx, const yaml_event_t *event, void *data)
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

static enum uwbs_config_status uwbs_config_region_loader_enter_version(
	struct uwbs_config_yaml_ctx *ctx,
	struct uwbs_config_region_loader *loader)
{
	return uwbs_config_yaml_enter_node(
		ctx, uwbs_config_region_loader_version_consume_event, loader);
}

static enum uwbs_config_status uwbs_config_region_loader_root_consume_event(
	struct uwbs_config_yaml_ctx *ctx, const yaml_event_t *event, void *data)
{
	enum uwbs_config_status ret = UWBS_CONFIG_STATUS_OK;
	struct uwbs_config_region_loader *loader = data;
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
			ret = uwbs_config_region_loader_enter_version(ctx,
								      loader);
		else if (strcmp(name, "regions") == 0)
			ret = uwbs_config_yaml_enter_node(
				ctx,
				uwbs_config_region_loader_regions_consume_event,
				loader);
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
uwbs_config_region_loader_find_from_str(const char *str, const size_t size,
					const char *country_code, char **region)
{
	enum uwbs_config_status ret;
	struct uwbs_config_yaml_ctx ctx;
	struct uwbs_config_region_loader loader;

	if (!str || !country_code || !region)
		return UWBS_CONFIG_STATUS_INVALID_ARG;

	uwbs_config_region_loader_init(&loader, country_code, region);
	uwbs_config_yaml_init(&ctx);
	ret = uwbs_config_yaml_load_str(
		str, size, &ctx, uwbs_config_region_loader_root_consume_event,
		&loader);
	if (ret == UWBS_CONFIG_STATUS_OK)
		ret = *loader.region_found ?
			      UWBS_CONFIG_STATUS_OK :
			      UWBS_CONFIG_STATUS_COUNTRY_CODE_NOT_FOUND;
	uwbs_config_yaml_destroy(&ctx);
	uwbs_config_region_loader_destroy(&loader);

	return ret;
}

enum uwbs_config_status uwbs_config_region_loader_find_from_file(
	const char *filename, const char *country_code, char **region)
{
	enum uwbs_config_status ret;
	struct uwbs_config_yaml_ctx ctx;
	struct uwbs_config_region_loader loader;

	if (!filename || !country_code || !region)
		return UWBS_CONFIG_STATUS_INVALID_ARG;

	uwbs_config_region_loader_init(&loader, country_code, region);
	uwbs_config_yaml_init(&ctx);
	ret = uwbs_config_yaml_load_file(
		filename, &ctx, uwbs_config_region_loader_root_consume_event,
		&loader);
	if (ret == UWBS_CONFIG_STATUS_OK)
		ret = *loader.region_found ?
			      UWBS_CONFIG_STATUS_OK :
			      UWBS_CONFIG_STATUS_COUNTRY_CODE_NOT_FOUND;
	uwbs_config_yaml_destroy(&ctx);
	uwbs_config_region_loader_destroy(&loader);

	return ret;
}
