/*
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */
#include "qthread.h"

#include "qmalloc.h"

#include <pthread.h>

#define LOG_TAG "qthread"
#include "qlog.h"

struct qthread {
	pthread_t thread_id;
	void *arg;
	qthread_func func;
};

static void *thread_cb(void *arg)
{
	struct qthread *th = (struct qthread *)arg;
	th->func(th->arg);
	return 0;
}

struct qthread *qthread_create(qthread_func thread_function, void *arg, const char *name,
			       void *stack_buffer, uint32_t stack_size, enum qthread_priority prio)
{
	struct qthread *th;
	th = (struct qthread *)qmalloc(sizeof(struct qthread));
	if (!th)
		return NULL;

	th->arg = arg;
	th->func = thread_function;
	pthread_create(&th->thread_id, NULL, thread_cb, (void *)th);

	return th;
}

enum qerr qthread_join(struct qthread *thread)
{
	int r;

	if (!thread)
		return QERR_EINVAL;

	r = pthread_join(thread->thread_id, NULL);
	if (r)
		return qerr_convert_os_to_qerr(r);

	return QERR_SUCCESS;
}

enum qerr qthread_delete(struct qthread *thread)
{
	if (!thread)
		return QERR_EINVAL;

	qfree(thread);

	return QERR_SUCCESS;
}
