/*
 * SPDX-FileCopyrightText: Copyright (c) 2022 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include <zephyr/logging/log.h>

/* Register the most permissive log level, to allow
 * compile-time filtering based on QLOG_CURRENT_LEVEL locally defined. */
LOG_MODULE_REGISTER(qlog, LOG_LEVEL_DBG);
