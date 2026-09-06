/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#if defined(__ANDROID__)

/* Workaround for `pthread_cancel()` in Android, using `pthread_kill()` instead,
 * as Android NDK does not support `pthread_cancel()`.
 */

#include "qmutils_pthread_cancel.h"
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <string.h>
#include <threads.h>

#define SIGCANCEL (SIGRTMIN + 7)

static thread_local atomic_bool cancellable = ATOMIC_VAR_INIT(true);
static thread_local atomic_bool cancelled = ATOMIC_VAR_INIT(false);

void qmutils_thread_exit_handler(int sig);

int pthread_setcanceltype(int type, int *oldtype)
{
	return 0;
}

int pthread_setcancelstate(int state, int *oldstate)
{
	atomic_store(&cancellable, state == PTHREAD_CANCEL_ENABLE);

	if (atomic_load(&cancellable) && atomic_load(&cancelled))
		pthread_exit(PTHREAD_CANCELED);

	return 0;
}

int pthread_cancel(pthread_t thread_id)
{
	static bool initialized = false;

	if (!initialized) {
		struct sigaction actions;

		memset(&actions, 0, sizeof(actions));
		sigemptyset(&actions.sa_mask);
		actions.sa_flags = 0;
		actions.sa_handler = qmutils_thread_exit_handler;

		sigaction(SIGCANCEL, &actions, NULL);
		initialized = true;
	}

	return pthread_kill(thread_id, SIGCANCEL);
}

void qmutils_thread_exit_handler(int sig)
{
	atomic_store(&cancelled, true);
	if (atomic_load(&cancellable))
		pthread_exit(PTHREAD_CANCELED);
}

#endif // defined(HAVE_PTHREAD) && defined(__ANDROID__)
