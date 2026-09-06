/**
 * @file      qlog.c
 *
 * @brief     Implementation for Qorvo log functions
 *
 * @author    Qorvo Paris Applications
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */

#include "qlog.h"

const char *const qorvo_log_prio[] = { "UNDEF", "UNDEF", "UNDEF", "DEBUG",
				       "INFO",	"UNDEF", "WARN",  "ERROR" };

void qorvo_log_print(enum qorvo_log_levels level, const char *tag, const char *fmt, ...)
{
	va_list ap;
	FILE *output = stderr;
	fprintf(output, "%-5s %-8s: ", qorvo_log_prio[level], ((tag == NULL) ? "" : tag));
	va_start(ap, fmt);
	vfprintf(output, fmt, ap);
	fprintf(output, "\n");
	va_end(ap);
}
