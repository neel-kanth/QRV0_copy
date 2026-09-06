/*
 * Utilities for common log management in examples.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "util_log.h"

unsigned int util_log_level = QLOG_LEVEL_DEBUG;

void util_log_set_level(unsigned int level)
{
	util_log_level = level;
}
