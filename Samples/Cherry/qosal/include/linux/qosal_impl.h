/**
 * @file      qosal_impl.h
 *
 * @brief     Header file for qosal implementation for Linux
 *
 * @author    Qorvo Paris Applications
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */

#ifndef QOSAL_IMPL_H
#define QOSAL_IMPL_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#define QOSAL_IMPL_THREAD_MAX_NAME_LEN 0

#define QOSAL_IMPL_THREAD_STACK_DEFINE(name, stack_size) static void *name = NULL

#define QOSAL_IMPL_IRQ_CONNECT(irqn, prio, handler)

enum qorvo_log_levels {
	LOG_LEVEL_DEBUG = 3,
	LOG_LEVEL_INFORMATION = 4,
	LOG_LEVEL_WARNING = 6,
	LOG_LEVEL_ERROR = 7,
};

void qorvo_log_print(enum qorvo_log_levels level, const char *tag, const char *fmt, ...);

#define QOSAL_IMPL_LOG_ERR(...)                                         \
	do {                                                            \
		qorvo_log_print(LOG_LEVEL_ERROR, LOG_TAG, __VA_ARGS__); \
	} while (0)

#define QOSAL_IMPL_LOG_WARN(...)                                          \
	do {                                                              \
		qorvo_log_print(LOG_LEVEL_WARNING, LOG_TAG, __VA_ARGS__); \
	} while (0)

#define QOSAL_IMPL_LOG_INFO(...)                                              \
	do {                                                                  \
		qorvo_log_print(LOG_LEVEL_INFORMATION, LOG_TAG, __VA_ARGS__); \
	} while (0)

#define QOSAL_IMPL_LOG_DEBUG(...)                                       \
	do {                                                            \
		qorvo_log_print(LOG_LEVEL_DEBUG, LOG_TAG, __VA_ARGS__); \
	} while (0)

#endif /* QOSAL_IMPL_H */
