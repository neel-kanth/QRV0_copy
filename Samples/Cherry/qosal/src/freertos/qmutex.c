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

#include "qmutex.h"

#include "FreeRTOS.h"
#include "qirq.h"
#include "qmalloc.h"
#include "qprivate.h"
#include "semphr.h"

struct qmutex {
	SemaphoreHandle_t mutex;
};

struct qmutex *qmutex_init()
{
	struct qmutex *mutex = (struct qmutex *)qmalloc(sizeof(struct qmutex));
	if (mutex == NULL)
		return NULL;

	mutex->mutex = xSemaphoreCreateMutex();
	if (mutex->mutex == NULL) {
		qfree(mutex);
		return NULL;
	}

	return mutex;
}

void qmutex_deinit(struct qmutex *mutex)
{
	if (!mutex)
		return;

	vSemaphoreDelete(mutex->mutex);

	qfree(mutex);
}

enum qerr qmutex_lock(struct qmutex *mutex, uint32_t timeout_ms)
{
	BaseType_t ret_val = pdFALSE;

	if (!mutex)
		return QERR_EINVAL;

	if (qprivate_is_in_isr()) {
		portBASE_TYPE task_woken = pdFALSE;

		ret_val = xSemaphoreTakeFromISR(mutex->mutex, &task_woken);
	} else {
		TickType_t ticks = 0;

		if (timeout_ms == QOSAL_WAIT_FOREVER) {
			ticks = portMAX_DELAY;
		} else if (timeout_ms != 0) {
			ticks = pdMS_TO_TICKS(timeout_ms);
		}

		ret_val = xSemaphoreTake(mutex->mutex, ticks);
	}

	if (!ret_val)
		return QERR_EIO;

	return QERR_SUCCESS;
}

enum qerr qmutex_unlock(struct qmutex *mutex)
{
	BaseType_t ret_val = pdFALSE;

	if (!mutex)
		return QERR_EINVAL;

	if (qprivate_is_in_isr()) {
		portBASE_TYPE task_woken = pdFALSE;

		ret_val = xSemaphoreGiveFromISR(mutex->mutex, &task_woken);
	} else {
		ret_val = xSemaphoreGive(mutex->mutex);
	}

	if (!ret_val)
		return QERR_EIO;

	return QERR_SUCCESS;
}
