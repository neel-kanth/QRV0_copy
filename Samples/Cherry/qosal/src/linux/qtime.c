/**
 * @file      qtime.c
 *
 * @brief     Implementation for Qorvo timing functions
 *
 * @author    Qorvo Paris Applications
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */

#include "qtime.h"

#include <errno.h>
#include <time.h>

int64_t qtime_get_uptime_us(void)
{
	struct timespec ts;
	int ret = clock_gettime(CLOCK_BOOTTIME, &ts);
	if (ret)
		return -1;
	return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

void qtime_msleep(int ms)
{
	struct timespec ts;
	int res;

	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000;

	do {
		res = nanosleep(&ts, NULL);
	} while (res && errno == EINTR);
}

void qtime_usleep(int us)
{
	struct timespec ts;
	int res;

	ts.tv_sec = us / 1000000;
	ts.tv_nsec = (us % 1000000) * 1000;

	do {
		res = nanosleep(&ts, NULL);
	} while (res && errno == EINTR);
}

uint32_t qtime_get_sys_freq_hz(void)
{
	/* TODO parse /proc/cpuinfo. */
	return 0;
}
