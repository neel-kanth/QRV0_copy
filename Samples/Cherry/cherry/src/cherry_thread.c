/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "cherry_thread.h"

#include "cherry_log.h"
#include "cherry_priv.h"

#include <cherry/cherry.h>
#include <qmalloc.h>
#include <string.h>

#ifndef CONFIG_CHERRY_THREAD_STACK_SIZE
#define CONFIG_CHERRY_THREAD_STACK_SIZE 4096
#endif

QTHREAD_STACK_DEFINE(cherry_thread_stack, CONFIG_CHERRY_THREAD_STACK_SIZE);

static void cherry_thread_hdl(void *arg);
static void cherry_thread_wakeup(struct cherry_thread *thread_ctx);
static struct cherry_task *
cherry_thread_alloc_task(const void *context, const void *params,
			 size_t params_size, cherry_thread_task_t thread_task);
static void cherry_thread_free_task(struct cherry_task *task);
static bool cherry_thread_task_enqueue(struct cherry_thread *thread_ctx,
				       struct cherry_task *task,
				       struct cherry_task **head, bool destroy,
				       bool force_task);
static struct cherry_task *
cherry_thread_dequeue_first_task(struct cherry_thread *thread_ctx,
				 struct cherry_task **head);
static void cherry_thread_remove_all_tasks(struct cherry_thread *thread_ctx,
					   struct cherry_task *head);

enum cherry_err cherry_thread_create(struct cherry *cherry_ctx)
{
	struct cherry_thread *thread_ctx = &cherry_ctx->thread_ctx;

	thread_ctx->tasks = NULL;
	thread_ctx->state = CHERRY_THREAD_RUN;
	thread_ctx->prio_tasks = NULL;
	thread_ctx->under_destroy = false;

	if ((thread_ctx->sem_wakeup = qsemaphore_init(0, 0xffff)) == NULL) {
		QLOGE("%s: qsemaphore_init failed.", __func__);
		return CHERRY_ERR_INTERNAL;
	}

	if ((thread_ctx->task_mutex = qmutex_init()) == NULL) {
		QLOGE("%s: qmutex_init failed.", __func__);
		goto sem_wakeup_destroy;
	}

	if ((thread_ctx->cherry_thread = qthread_create(
		     cherry_thread_hdl, cherry_ctx, "Cherry thread",
		     cherry_thread_stack, CONFIG_CHERRY_THREAD_STACK_SIZE,
		     QTHREAD_PRIORITY_NORMAL)) == NULL) {
		QLOGE("%s: qthread_create failed.", __func__);
		goto mutex_destroy;
	}

	return CHERRY_ERR_NONE;

mutex_destroy:
	qmutex_deinit(thread_ctx->task_mutex);
sem_wakeup_destroy:
	qsemaphore_deinit(thread_ctx->sem_wakeup);

	return CHERRY_ERR_INTERNAL;
}

enum cherry_err cherry_thread_task_list_send(
	struct cherry_thread *thread_ctx, struct cherry_task **tasks,
	const void *context, const void *params, size_t params_size,
	cherry_thread_task_t thread_task, bool destroy, bool force_task)
{
	struct cherry_task *task;

	if (!context || !thread_task)
		return CHERRY_ERR_INTERNAL;

	if (thread_ctx->state != CHERRY_THREAD_RUN)
		return CHERRY_ERR_INTERNAL;

	task = cherry_thread_alloc_task(context, params, params_size,
					thread_task);
	if (!task) {
		QLOGE("%s: Unable to allocate task.", __func__);
		return CHERRY_ERR_INTERNAL;
	}
	if (cherry_thread_task_enqueue(thread_ctx, task, tasks, destroy,
				       force_task))
		cherry_thread_wakeup(thread_ctx);
	else
		return CHERRY_ERR_INTERNAL;

	return CHERRY_ERR_NONE;
}

enum cherry_err cherry_thread_send_list_task(struct cherry_thread *thread_ctx,
					     const void *context,
					     const void *params,
					     size_t params_size,
					     cherry_thread_task_t thread_task,
					     bool destroy)
{
	return cherry_thread_task_list_send(thread_ctx, &thread_ctx->tasks,
					    context, params, params_size,
					    thread_task, destroy, false);
}

enum cherry_err cherry_thread_send_prio_task(struct cherry_thread *thread_ctx,
					     const void *context,
					     const void *params,
					     size_t params_size,
					     cherry_thread_task_t thread_task)
{
	/*
	 * A prio task never pass thread under destroy,
	 * but always has to pe processed.
	 */
	return cherry_thread_task_list_send(thread_ctx, &thread_ctx->prio_tasks,
					    context, params, params_size,
					    thread_task, false, true);
}

void cherry_thread_stop(struct cherry_thread *thread_ctx)
{
	/* Free all unprocessed tasks. */
	cherry_thread_remove_all_tasks(thread_ctx, thread_ctx->tasks);
	cherry_thread_remove_all_tasks(thread_ctx, thread_ctx->prio_tasks);
	thread_ctx->state = CHERRY_THREAD_STOP;
	cherry_thread_wakeup(thread_ctx);
	return;
}

void cherry_thread_destroy(struct cherry_thread *thread_ctx)
{
	/* Free Cherry thread memory. */
	qmutex_deinit(thread_ctx->task_mutex);
	qsemaphore_deinit(thread_ctx->sem_wakeup);
	qthread_delete(thread_ctx->cherry_thread);
}

void cherry_thread_join(struct cherry_thread *thread_ctx)
{
	qthread_join(thread_ctx->cherry_thread);
}

static void cherry_thread_remove_pending_tasks_from_list(
	struct cherry_thread *thread_ctx, const void *context,
	struct cherry_task **head, bool abort)
{
	struct cherry_task *task;
	struct cherry_task *task_next;
	struct cherry_task *task_out = NULL;

	if (!context) {
		QLOGE("%s: Context is invalid.", __func__);
		return;
	}

	qmutex_lock(thread_ctx->task_mutex, QOSAL_WAIT_FOREVER);

	task = *head;
	*head = NULL;

	for (task_next = (task ? task->next_task : NULL); task;
	     task = task_next,
	    task_next = task_next ? task_next->next_task : NULL) {
		if (task->context == context) {
			cherry_thread_free_task(task);
			continue;
		}

		if (task_out)
			task_out->next_task = task;
		else
			*head = task;
		task_out = task;
	}

	if (task_out)
		task_out->next_task = NULL;

	qmutex_unlock(thread_ctx->task_mutex);

	return;
}

void cherry_thread_remove_pending_tasks(struct cherry_thread *thread_ctx,
					const void *context)
{
	/*
	 * Free all unprocessed tasks according to context, check in both
	 * tasks' lists.
	 */
	cherry_thread_remove_pending_tasks_from_list(thread_ctx, context,
						     &thread_ctx->tasks, false);
	/*
	 * Only prio tasks list can have data to free, so we need to call the
	 * tasks with abort flag.
	 */
	cherry_thread_remove_pending_tasks_from_list(
		thread_ctx, context, &thread_ctx->prio_tasks, true);
}

static void cherry_thread_wakeup(struct cherry_thread *thread_ctx)
{
	qsemaphore_give(thread_ctx->sem_wakeup);
}

static struct cherry_task *
cherry_thread_alloc_task(const void *context, const void *params,
			 size_t params_size, cherry_thread_task_t thread_task)
{
	struct cherry_task *task;

	task = (struct cherry_task *)qmalloc(sizeof(struct cherry_task));
	if (!task) {
		QLOGE("%s: Unable to allocate task.", __func__);
		return NULL;
	}

	task->context = context;
	task->thread_task = thread_task;
	task->params = (void *)params;
	task->params_size = params_size;
	if (params && params_size) {
		task->params = qmalloc(params_size);
		if (!task->params) {
			QLOGE("%s: Unable to allocate params.", __func__);
			qfree(task);
			return NULL;
		}
		memcpy(task->params, params, params_size);
	}

	return task;
}

static void cherry_thread_free_task(struct cherry_task *task)
{
	if (task->params && task->params_size)
		qfree(task->params);
	qfree(task);
}

static bool cherry_thread_task_enqueue(struct cherry_thread *thread_ctx,
				       struct cherry_task *task,
				       struct cherry_task **head, bool destroy,
				       bool force_task)
{
	bool is_queue = true;

	/* Add task in tail. */
	task->next_task = NULL;

	qmutex_lock(thread_ctx->task_mutex, QOSAL_WAIT_FOREVER);

	if (thread_ctx->under_destroy && !force_task) {
		is_queue = false;
		goto unlock;
	}

	if (*head == NULL)
		*head = task;
	else {
		struct cherry_task *tmp_task = *head;

		while (tmp_task->next_task != NULL)
			tmp_task = tmp_task->next_task;
		tmp_task->next_task = task;
	}

	if (destroy)
		thread_ctx->under_destroy = true;

unlock:
	qmutex_unlock(thread_ctx->task_mutex);

	return is_queue;
}

static struct cherry_task *
cherry_thread_dequeue_first_task(struct cherry_thread *thread_ctx,
				 struct cherry_task **head)
{
	struct cherry_task *task = NULL;

	qmutex_lock(thread_ctx->task_mutex, QOSAL_WAIT_FOREVER);
	if (*head != NULL) {
		task = *head;
		*head = task->next_task;
	}
	qmutex_unlock(thread_ctx->task_mutex);
	return task;
}

static void cherry_thread_remove_all_tasks(struct cherry_thread *thread_ctx,
					   struct cherry_task *head)
{
	struct cherry_task *task = head;
	struct cherry_task *tmp_task;

	qmutex_lock(thread_ctx->task_mutex, QOSAL_WAIT_FOREVER);

	while (task != NULL) {
		tmp_task = task->next_task;
		/* Abort the tasks if needed. */
		task->thread_task(task->context, task->params, true);
		cherry_thread_free_task(task);
		task = tmp_task;
	}
	head = NULL;

	qmutex_unlock(thread_ctx->task_mutex);
}

int cherry_thread_process_prio_task(struct cherry_thread *thread_ctx,
				    uint32_t timeout_ms)
{
	struct cherry_task *task;

	qsemaphore_take(thread_ctx->sem_wakeup, timeout_ms);
	/* Get and remove first task from the priority list, it will be processed. */
	task = cherry_thread_dequeue_first_task(thread_ctx,
						&thread_ctx->prio_tasks);
	if (!task)
		return 0;

	/* Callback does the processing. */
	task->thread_task(task->context, task->params, false);

	/* Free the allocated task once it is processed. */
	cherry_thread_free_task(task);

	return 1;
}

int cherry_thread_run_one(struct cherry *cherry_ctx)
{
	struct cherry_thread *thread_ctx = &cherry_ctx->thread_ctx;
	struct cherry_task *task;

	qsemaphore_take(thread_ctx->sem_wakeup, QOSAL_WAIT_FOREVER);

	while (thread_ctx->state == CHERRY_THREAD_RUN) {
		/* Get and remove first task from the priority list, it will be processed. */
		task = cherry_thread_dequeue_first_task(
			thread_ctx, &thread_ctx->prio_tasks);
		if (!task) {
			/* Get and remove first task from the second list, it will be processed. */
			task = cherry_thread_dequeue_first_task(
				thread_ctx, &thread_ctx->tasks);
		}

		if (!task)
			return 0;

		/* Callback does the processing. */
		task->thread_task(task->context, task->params, false);

		/* Free the allocated task once it is processed. */
		cherry_thread_free_task(task);
	}

	return 1;
}

static void cherry_thread_hdl(void *arg)
{
	struct cherry *cherry_ctx = (struct cherry *)arg;
	struct cherry_thread *thread_ctx = &cherry_ctx->thread_ctx;

	while (thread_ctx->state == CHERRY_THREAD_RUN) {
		if (cherry_thread_run_one(cherry_ctx) == -1)
			break;
	}
}
