/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include <errno.h>
#include <qmalloc.h>
#include <stdlib.h>

#ifndef QMCHANNEL_USE_BYPASS
#include <poll.h>
#include <pthread.h>
#include <unistd.h>
#endif

#include "qmchannel_priv.h"
#include "qmutils/qmutils.h"

#if defined(__ANDROID__)
#include "qmutils_pthread_cancel.h"
#endif

#ifdef USE_MOCK
#ifndef QMCHANNEL_USE_BYPASS
int poll_mock(struct pollfd *fds, nfds_t nfds, int timeout);
#define poll(f, n, t) poll_mock(f, n, t)
int pthread_create_mock(pthread_t *thread, const pthread_attr_t *attr,
			void *(*start_routine)(void *), void *arg);
#define pthread_create(t, a, r, g) pthread_create_mock(t, a, r, g)
int pthread_setcancelstate_mock(int state, int *oldstate);
#define pthread_setcancelstate(s, o) pthread_setcancelstate_mock(s, o)
int pthread_cancel_mock(pthread_t thread);
#define pthread_cancel(t) pthread_cancel_mock(t)
int pthread_join_mock(pthread_t thread, void **retval);
#define pthread_join(t, r) pthread_join_mock(t, r)
#endif /* QMCHANNEL_USE_BYPASS */

int qmchannel_init_mock(qmhandle hnd, enum qmchannel_type type,
			qmchannel_callback cb, void *cb_data);
#define qmchannel_init(h, t, c, d) qmchannel_init_mock(h, t, c, d)
int qmchannel_close_mock(qmhandle hnd);
#define qmchannel_close(h) qmchannel_close_mock(h)
int qmchannel_getfd_mock(qmhandle hnd);
#define qmchannel_getfd(h) qmchannel_getfd_mock(h)
int qmchannel_getpollevent_mock(qmhandle hnd);
#define qmchannel_getpollevent(h) qmchannel_getpollevent_mock(h)
int qmchannel_read_mock(qmhandle hnd);
#define qmchannel_read(h) qmchannel_read_mock(h)
#endif

/**
 * common_handle_data() - Callback for the qmchannel_read() function.
 * @cb_data: Callback data.
 * @buf: Buffer to use.
 * @len: Length of the buffer.
 *
 * Return: 0 on success, else a negative error code.
 */
int common_handle_data(void *cb_data, void *buf, size_t len)
{
	struct common_channel_data *ch = cb_data;

	return ch->app_cb(ch->app_cb_data, buf, len);
}

/**
 * common_thread() - Thread to read from the qmchannel.
 * @arg: Last argument passed to pthread_create().
 *
 * Note: if the thread gets cancelled, retval will be replaced by
 * PTHREAD_CANCELED (-1) in pthread_join(). So return other error codes as
 * **positive** values.
 *
 * Return: Positive error code.
 */
void *common_thread(void *arg)
{
#ifndef QMCHANNEL_USE_BYPASS
	struct common_channel_data *ch = arg;
	struct pollfd pfd;
	int channel_event = qmchannel_getpollevent(&ch->ll);
	long ret;

	pfd.fd = qmchannel_getfd(&ch->ll);
	pfd.events = channel_event;

	while (true) {
		/* Only allow cancellation while waiting for events. */
		ret = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		if (ret)
			break;
		ret = poll(&pfd, 1, -1);
		if (ret < 0) {
			ret = errno;
			break;
		}
		ret = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		if (ret)
			break;

		if (pfd.revents & channel_event) {
			/* Reverse sign of returned value. */
			ret = -qmchannel_read(&ch->ll);
			if (ret)
				break;
		}
	}
	return (void *)ret;
#else
	return NULL;
#endif
}

/**
 * common_init() - Initialize and return a new channel.
 * @type: Channel type.
 * @priv_size: size of the private data struct.
 * @qmchannel_cb: qmchannel-level callback.
 * @app_cb: Application-level callback.
 * @app_cb_data: Application-level callback data.
 *
 * Return: channel qmhandle on success, else NULL.
 */
qmhandle common_init(enum qmchannel_type type, size_t priv_size,
		     qmchannel_callback qmchannel_cb, qmchannel_callback app_cb,
		     void *app_cb_data)
{
	struct common_channel_data *ch;
	int ret;

	if (!qmchannel_cb || !app_cb)
		return NULL;

	ch = qcalloc(1, priv_size);
	if (!ch)
		return NULL;
	ch->app_cb = app_cb;
	ch->app_cb_data = app_cb_data;

	ret = qmchannel_init(&ch->ll, type, qmchannel_cb, ch);
	if (ret) {
		qfree(ch);
		return NULL;
	}
	return &ch->ll;
}

/**
 * common_start() - Start the channel read thread.
 * @hnd: qmchannel handle.
 * @thread_routine: Thread routine.
 *
 * Return: 0 on success, else a negative error code.
 */
int common_start(qmhandle hnd, void *(*thread_routine)(void *))
{
#ifndef QMCHANNEL_USE_BYPASS
	struct common_channel_data *ch;
	int ret;

	if (!hnd)
		return -EINVAL;
	ch = container_of(hnd, struct common_channel_data, ll);

	ret = pthread_create(&ch->thread_id, NULL, thread_routine, ch);

	return -ret;
#else
	/* The qmutils_bypass version don't require a thread since the
	 * qmchannel_read() function is automatically called by the bypass
	 * API through the provided callback.
	 */
	(void)(hnd);
	(void)(thread_routine);
	return -ENOTSUP;
#endif
}

/**
 * common_destroy() - Stop the channel read thread and destroy the instance.
 * @hnd: qmchannel handle.
 *
 * Return: 0 on success, else a negative error code.
 */
int common_destroy(qmhandle hnd)
{
	struct common_channel_data *ch;
	long retval = 0;
	int ret;

	if (!hnd)
		return -EINVAL;
	ch = container_of(hnd, struct common_channel_data, ll);

#ifndef QMCHANNEL_USE_BYPASS
	if (ch->thread_id) {
		ret = pthread_cancel(ch->thread_id);
		if (ret)
			return -ret;

		ret = pthread_join(ch->thread_id, (void **)&retval);
		if (ret)
			return -ret;
		if (retval == (long)PTHREAD_CANCELED)
			retval = 0;
	}
#endif

	ret = qmchannel_close(&ch->ll);
	if (ret)
		return ret;

	qfree(ch);

	return retval;
}
