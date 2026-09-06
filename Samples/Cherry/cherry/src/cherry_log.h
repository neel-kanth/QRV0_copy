/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

extern unsigned int cherry_log_level;

#ifndef LOG_TAG
#define LOG_TAG "cherry"
#endif
#define QLOG_CURRENT_LEVEL cherry_log_level

#include <qlog.h>
