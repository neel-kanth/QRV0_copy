/*
 * Copyright (c) 2022 Qorvo, Inc
 *
 * All rights reserved.
 *
 * NOTICE: All information contained herein is, and remains the property
 * of Qorvo, Inc. and its suppliers, if any. The intellectual and technical
 * concepts herein are proprietary to Qorvo, Inc. and its suppliers, and
 * may be covered by patents, patent applications, and are protected by
 * trade secret and/or copyright law. Dissemination of this information
 * or reproduction of this material is strictly forbidden unless prior written
 * permission is obtained from Qorvo, Inc.
 *
 */

#include "qmalloc.h"
#include "qmsg_queue.h"

#include <stdint.h>

#define QSIGNAL_QUEUE_DEPTH 5 /* Depth of the message queue used for the qsignal. */

struct qsignal {
	struct qmsg_queue *msg_queue; /* Message queue used to transmit signal.
				       */
};

struct qsignal *qsignal_init()
{
	struct qsignal *signal = qmalloc(sizeof(struct qsignal));
	if (!signal)
		return NULL;

	signal->msg_queue = qmsg_queue_init(NULL, sizeof(int), QSIGNAL_QUEUE_DEPTH);
	if (!signal->msg_queue) {
		qfree(signal);
		return NULL;
	}

	return signal;
}

void qsignal_deinit(struct qsignal *signal)
{
	if (!signal)
		return;

	qmsg_queue_deinit(signal->msg_queue);

	qfree(signal);
}

enum qerr qsignal_raise(struct qsignal *signal, int value)
{
	return qmsg_queue_put(signal->msg_queue, &value);
}

enum qerr qsignal_wait(struct qsignal *signal, int *value, uint32_t timeout_ms)
{
	return qmsg_queue_get(signal->msg_queue, value, timeout_ms);
}
