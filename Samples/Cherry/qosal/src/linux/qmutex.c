/*
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */

#include "qmutex.h"

#include "qerr.h"

#include <errno.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#define LOG_TAG "qmutex"
#include "qlog.h"

/* The pthread_mutex API don't provide a lock with timeout.
 * So need to use pthread semaphore API instead.
 *
 * - sem_init()
 * - sem_timedwait()
 * - sem_post()
 * - sem_destroy()
 */

struct qmutex {
	sem_t semaphore;
};

struct qmutex *qmutex_init(void)
{
	struct qmutex *mutex;
	int r;

	mutex = malloc(sizeof(*mutex));
	if (mutex == NULL) {
		QLOGE("No mutex available in memory slab");
		return NULL;
	}

	r = sem_init(&mutex->semaphore, 0, 1);
	if (r) {
		free(mutex);
		return NULL;
	}

	return mutex;
}

void qmutex_deinit(struct qmutex *mutex)
{
	if (!mutex)
		return;

	sem_destroy(&mutex->semaphore);
	free(mutex);
}

enum qerr qmutex_lock(struct qmutex *mutex, uint32_t timeout_ms)
{
	int r;

	if (!mutex)
		return QERR_EINVAL;

	if (timeout_ms == QOSAL_WAIT_FOREVER) {
		r = sem_wait(&mutex->semaphore);
	} else {
		struct timespec abstime;
		r = clock_gettime(CLOCK_REALTIME, &abstime);
		if (r) {
			return qerr_convert_os_to_qerr(errno);
		}
		abstime.tv_nsec += timeout_ms * 1000000;
		if (abstime.tv_nsec >= 1000000000) {
			abstime.tv_nsec -= 1000000000;
			abstime.tv_sec++;
		}
		r = sem_timedwait(&mutex->semaphore, &abstime);
	}
	return qerr_convert_os_to_qerr(r ? errno : 0);
}

enum qerr qmutex_unlock(struct qmutex *mutex)
{
	int r;

	if (!mutex)
		return QERR_EINVAL;

	r = sem_post(&mutex->semaphore);
	return qerr_convert_os_to_qerr(r ? errno : 0);
}
