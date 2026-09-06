/*
 * Copyright (c) 2023 Qorvo, Inc
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

#include <cstdlib>
#include <cstring>
#include <list>

extern "C" {
#include "qerr.h"
#include "qirq.h"
#include "qmsg_queue.h"
#include "qprofiling.h"

#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
}

#define MAX_MAILBOX_COMPAT 1

struct qmsg_queue {
	std::list<void *> report_list;
	unsigned int item_type_sz;
	sem_t sem_mail;
	pthread_mutex_t mutex;
};

/* WARNING: only one mailbox can currently be created */
struct qmsg_queue *qmsg_queue_init(char *msg_queue_buffer, uint32_t item_size, uint32_t max_item)
{
	struct qmsg_queue *mlbx = new qmsg_queue;

	sem_init(&mlbx->sem_mail, 0, 0);
	pthread_mutex_init(&mlbx->mutex, NULL);
	mlbx->item_type_sz = item_size;

	return mlbx;
}

enum qerr qmsg_queue_put(struct qmsg_queue *msg_queue, const void *item)
{
	pthread_mutex_lock(&msg_queue->mutex);
	void *buffer = malloc(msg_queue->item_type_sz);
	std::memcpy(buffer, item, msg_queue->item_type_sz);
	msg_queue->report_list.push_back((void *)buffer);
	pthread_mutex_unlock(&msg_queue->mutex);
	sem_post(&msg_queue->sem_mail);

	return QERR_SUCCESS;
}

enum qerr qmsg_queue_get(struct qmsg_queue *msg_queue, void *item)
{
	int r = 0;

	r = sem_wait(&msg_queue->sem_mail);

	if (r) {
		return QERR_EAGAIN;
	} else {
		pthread_mutex_lock(&msg_queue->mutex);
		if (msg_queue->report_list.size()) {
			std::memcpy(item, msg_queue->report_list.front(), msg_queue->item_type_sz);

			free(msg_queue->report_list.front());
			msg_queue->report_list.pop_front();
		}
		pthread_mutex_unlock(&msg_queue->mutex);
	}

	return QERR_SUCCESS;
}

void qmsg_queue_deinit(struct qmsg_queue *msg_queue)
{
	sem_destroy(&msg_queue->sem_mail);
	pthread_mutex_destroy(&msg_queue->mutex);
	msg_queue->item_type_sz = 0;
	delete msg_queue;
}

unsigned int qirq_lock()
{
	return 0;
}

void qirq_unlock(unsigned int key)
{
	(void)key;
}

void qmemstat(void)
{
}

void qstackstat(void)
{
}

void qprofstat(void)
{
}
