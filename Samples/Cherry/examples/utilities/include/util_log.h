/*
 * Public header for utilities for common log management.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

extern unsigned int util_log_level;

#define QLOG_CURRENT_LEVEL util_log_level
#include "qlog.h"

/**
 * util_log_set_level() - Change log level used by examples.
 * @level: New log level.
 */
void util_log_set_level(unsigned int level);
