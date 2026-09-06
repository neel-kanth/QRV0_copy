/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include "model.h"
#include "uwbs_config/uwbs_config.h"

enum uwbs_config_status
uwbs_config_model_load_str(const char *str, const size_t size,
			   struct uwbs_config_model *model);
enum uwbs_config_status
uwbs_config_model_load_file(const char *path, struct uwbs_config_model *model);
