/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#if defined(QMCHANNEL_USE_SYSFS)
#include "qmchannel_sysfs.h"
#elif defined(QMCHANNEL_USE_BYPASS)
#include "qmchannel_bypass.h"
#else /* Fallback to multichannel implementation. */
#include "qmchannel_multichan.h"
#include <pthread.h>
#endif

#ifndef container_of
#define container_of(ptr, type, member) \
	((type *)((char *)(ptr)-offsetof(type, member)))
#endif

/**
 * struct common_channel_data - Common channel private data.
 * @app_cb: Callback.
 * @app_cb_data: Callback data.
 * @ll: low-level qmchannel.
 * @thread_id: ID of the channel thread.
 */
struct common_channel_data {
	qmchannel_callback app_cb;
	void *app_cb_data;
	struct qmchannel ll;
#ifndef QMCHANNEL_USE_BYPASS
	pthread_t thread_id;
#endif
};

int common_handle_data(void *cb_data, void *buf, size_t len);
void *common_thread(void *arg);
qmhandle common_init(enum qmchannel_type type, size_t priv_size,
		     qmchannel_callback qmchannel_cb, qmchannel_callback app_cb,
		     void *app_cb_data);
int common_start(qmhandle hnd, void *(*thread_routine)(void *));
int common_destroy(qmhandle hnd);
