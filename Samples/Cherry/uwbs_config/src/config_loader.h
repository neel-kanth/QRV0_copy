/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <uwbs_config/uwbs_config.h>

struct uwbs_config_model;

enum uwbs_config_status
uwbs_config_load_str(const char *str, const size_t size,
		     const struct uwbs_config_model *model,
		     uwbs_config_on_new_key_cb user_cb, void *user_data);

enum uwbs_config_status
uwbs_config_load_file(const char *path, const struct uwbs_config_model *model,
		      uwbs_config_on_new_key_cb user_cb, void *user_data);
