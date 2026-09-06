/*
 * Copyright (c) 2023-2024 Qorvo, Inc
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

#include "qprofiling.h"

#include "FreeRTOS.h"

#ifdef CONFIG_QOSAL_PROFILING_STATS
#include "qmalloc.h"
#include "task.h"

void qmemstat(void)
{
}

int qstackstat_count_get(void)
{
	return (int)uxTaskGetNumberOfTasks();
}

int qstackstat_get(struct qstackstats *stats, int stack_count)
{
	TaskStatus_t *task_status_array;
	uint32_t nb_tasks;
	uint32_t i = 0;
	uint32_t total_runtime;

	/* Take a snapshot of the number of tasks in case it changes while this
	function is executing. */
	nb_tasks = uxTaskGetNumberOfTasks();

	/* Allocate a TaskStatus_t structure for each task. An array could be
	allocated statically at compile time. */
	task_status_array = qmalloc(nb_tasks * sizeof(TaskStatus_t));
	if (task_status_array != NULL) {
		/* Generate raw status information about each task. */
		nb_tasks = uxTaskGetSystemState(task_status_array, nb_tasks,
						(uint32_t *const)&total_runtime);

		uint32_t loop_count = (stack_count < nb_tasks) ? stack_count : nb_tasks;

		for (i = 0; i < loop_count; i++) {
			uint32_t *p = task_status_array[i].pxStackBase;
			uint32_t total = (p[-1] & 0xFFFFUL) - 8;

#if QOSAL_THREAD_MAX_NAME_LEN > 0
			sprintf(&stats[i].thread_name[0], "%s", task_status_array[i].pcTaskName);
#endif

			stats[i].stack_used = total - task_status_array[i].usStackHighWaterMark * 4;
			stats[i].stack_peak = 0;
			stats[i].stack_size = total;
		}

		/* The array is no longer needed, free the memory it consumes.
		 */
		qfree(task_status_array);
	}

	return i;
}

void qstackstat(void)
{
}

#else
void qmemstat(void)
{
}

int qstackstat_count_get()
{
	return 0;
}

int qstackstat_get(struct qstackstats *stats, int stack_count)
{
	return 0;
}

void qstackstat(void)
{
}
#endif

void qmemstat_get(struct qmemstats *stats)
{
	stats->static_size = 0;
	stats->heap_used = configTOTAL_HEAP_SIZE - xPortGetFreeHeapSize();
	stats->heap_peak = configTOTAL_HEAP_SIZE - xPortGetMinimumEverFreeHeapSize();
	stats->heap_size = configTOTAL_HEAP_SIZE;
}

void qprofstat(void)
{
	qmemstat();
	qstackstat();
}
