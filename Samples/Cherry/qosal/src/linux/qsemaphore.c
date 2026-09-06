/**
 * @file      qsemaphore.c
 *
 * @brief     Implementation for Qorvo semaphore implementation
 *
 * @author    Qorvo Paris Applications
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */

#include "qsemaphore.h"

#include "qerr.h"
#include "qmalloc.h"

#include <errno.h>
#include <limits.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#define QLOG_CURRENT_LEVEL QLOG_LEVEL_ERR
#define LOG_TAG "qsemaphore"
#include "qlog.h"

struct qsemaphore {
	sem_t sem;
	uint32_t max_count;
};

struct qsemaphore *qsemaphore_init(uint32_t init_count, uint32_t max_count)
{
	int r;
	struct qsemaphore *semaphore;

	if (init_count > max_count || max_count > SEM_VALUE_MAX)
		return NULL;

	semaphore = qmalloc(sizeof(struct qsemaphore));
	if (!semaphore)
		return NULL;

	r = sem_init(&semaphore->sem, 0, init_count);

	if (r) {
		qfree(semaphore);
		return NULL;
	}

	semaphore->max_count = max_count;

	return semaphore;
}

void qsemaphore_deinit(struct qsemaphore *sem)
{
	if (!sem)
		return;

	sem_destroy(&sem->sem);
	qfree(sem);
}

enum qerr qsemaphore_take(struct qsemaphore *sem, uint32_t timeout_ms)
{
	int r;
	struct timespec ts;

	if (!sem)
		return QERR_EINVAL;

	if (timeout_ms == QOSAL_WAIT_FOREVER)
		r = sem_wait(&sem->sem);
	else {
		if ((r = clock_gettime(CLOCK_REALTIME, &ts)) == 0) {
			/* The ts points to a structure that specifies an absolute timeout
			 * in seconds and nanoseconds since the Epoch. */
			ts.tv_sec += timeout_ms / 1000;
			ts.tv_nsec += (timeout_ms % 1000) * 1000000;
			if (ts.tv_nsec >= 1000000000L) {
				ts.tv_nsec -= 1000000000L;
				++ts.tv_sec;
			}
			while ((r = sem_timedwait(&sem->sem, &ts)) == -1 && errno == EINTR) {
				perror("qsemaphore_take");
				continue; /* Restart if interrupted by handler */
			}
		}
		if (r == -1)
			r = -errno;
	}

	return qerr_convert_os_to_qerr(r);
}

enum qerr qsemaphore_give(struct qsemaphore *sem)
{
	int r, val;

	if (!sem)
		return QERR_EINVAL;

	r = sem_getvalue(&sem->sem, &val);
	if (r)
		return QERR_EINVAL;

	if (val < (int)sem->max_count) {
		r = sem_post(&sem->sem);
		return qerr_convert_os_to_qerr(r);
	} else
		return QERR_EINVAL;
}
