/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include <uwbs_config/uwbs_config.h>

enum uwbs_config_status
uwbs_config_region_loader_find_from_str(const char *str, const size_t size,
					const char *country_code,
					char **region);

enum uwbs_config_status uwbs_config_region_loader_find_from_file(
	const char *filename, const char *country_code, char **region);
