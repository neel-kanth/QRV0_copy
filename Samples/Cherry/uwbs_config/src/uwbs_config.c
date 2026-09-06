/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "uwbs_config/uwbs_config.h"

#include "config_loader.h"
#include "model_loader.h"
#include "region_loader.h"

#include <stdlib.h>
#include <string.h>

static enum uwbs_config_status
uwbs_config_model_append_path(char **path, size_t *path_size,
			      const size_t prefix_len, const char *suffix)
{
	const size_t suffix_len = strlen(suffix);
	char *str;
	const size_t expected_size = prefix_len + suffix_len + 1;

	if (*path_size < expected_size) {
		*path_size = expected_size;
		str = realloc(*path, *path_size);
		if (!str)
			return UWBS_CONFIG_STATUS_OOM;
		*path = str;
	}

	strncpy(*path + prefix_len, suffix, *path_size - prefix_len);
	return UWBS_CONFIG_STATUS_OK;
}

enum uwbs_config_status
uwbs_config_load(const char *path, const char *country_code,
		 const uwbs_config_on_new_key_cb user_cb, void *user_data)
{
	/* When updating those files please update the file_path allocation size! */
	static const char *schema_file = "schema.json";
	static const char *region_file = "regions.yml";
	static const char *global_files[] = { "definition.yml", "settings.yml",
					      "calibration.yml" };
	static const char *regional_files[] = { "settings.yml" };

	struct uwbs_config_model model;
	size_t i;
	enum uwbs_config_status st;
	size_t path_len;
	char *file_path;
	size_t file_path_size;
	char *region_name;

	if (!path)
		return UWBS_CONFIG_STATUS_INVALID_ARG;

	/* Prepare file_path. */
	path_len = strlen(path);
	/* +11 for largest string in schema/files variable */
	/* +10 for region name estimation */
	/* +1 for trailing nul char. */
	file_path_size = path_len + 11 + 10 + 1;
	file_path = malloc(file_path_size);
	if (!file_path) {
		st = UWBS_CONFIG_STATUS_OOM;
		goto out0;
	}
	memcpy(file_path, path, path_len);
	file_path[path_len++] = '/';

	/* Load schema file. */
	st = uwbs_config_model_append_path(&file_path, &file_path_size,
					   path_len, schema_file);
	if (st != UWBS_CONFIG_STATUS_OK)
		goto out1;
	st = uwbs_config_model_load_file(file_path, &model);
	if (st != UWBS_CONFIG_STATUS_OK)
		goto out1;

	/* Load global files. */
	for (i = 0; i < sizeof(global_files) / sizeof(global_files)[0]; ++i) {
		st = uwbs_config_model_append_path(&file_path, &file_path_size,
						   path_len, global_files[i]);
		if (st != UWBS_CONFIG_STATUS_OK)
			goto out2;

		st = uwbs_config_load_file(file_path, &model, user_cb,
					   user_data);

		/* Skip unknown file. */
		if (st == UWBS_CONFIG_STATUS_INVALID_ARG)
			st = UWBS_CONFIG_STATUS_OK;

		if (st != UWBS_CONFIG_STATUS_OK)
			goto out2;
	}

	if (!country_code)
		goto out2;

	/* Open region.yaml, find country code associated region name. */
	st = uwbs_config_model_append_path(&file_path, &file_path_size,
					   path_len, region_file);
	if (st != UWBS_CONFIG_STATUS_OK)
		goto out2;
	st = uwbs_config_region_loader_find_from_file(file_path, country_code,
						      &region_name);
	if (st != UWBS_CONFIG_STATUS_OK)
		goto out2;

	st = uwbs_config_model_append_path(&file_path, &file_path_size,
					   path_len, region_name);
	if (st != UWBS_CONFIG_STATUS_OK)
		goto out2;
	path_len += strlen(region_name);
	file_path[path_len++] = '/';

	/* Load regional files. */
	for (i = 0; i < sizeof(regional_files) / sizeof(regional_files)[0];
	     ++i) {
		st = uwbs_config_model_append_path(&file_path, &file_path_size,
						   path_len, regional_files[i]);
		if (st != UWBS_CONFIG_STATUS_OK)
			break;
		st = uwbs_config_load_file(file_path, &model, user_cb,
					   user_data);
		/* Skip unknown file. */
		if (st == UWBS_CONFIG_STATUS_INVALID_ARG)
			st = UWBS_CONFIG_STATUS_OK;

		if (st != UWBS_CONFIG_STATUS_OK)
			break;
	}

	free(region_name);

out2:
	uwbs_config_model_destroy(&model);
out1:
	free(file_path);
out0:
	return st;
}
