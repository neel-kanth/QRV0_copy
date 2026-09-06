/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#ifndef CHERRY_THREAD_H
#define CHERRY_THREAD_H

#include <cherry/cherry.h>
#include <qerr.h>
#include <qmutex.h>
#include <qsemaphore.h>
#include <qthread.h>
#include <qtils.h>
#include <stdbool.h>

/**
 * typedef cherry_thread_task_t - Type for Cherry thread notification callback.
 * @context: Context of component (core or FiRa for example).
 * @params: Pointer to parameters if any.
 * @abort: True to indicate the task is aborted, allows freeing memory if neeeded.
 *
 * Once a task has to be processed, this callback is called. After processing
 * the task structure will be automatically freed by the Cherry thread.
 */
typedef void (*cherry_thread_task_t)(const void *context, const void *params,
				     bool abort);

/**
 * struct cherry_task - Cherry thread task structure.
 * @context: The context to give to the callback.
 * @params: Pointer to parameters, can be null according the task.
 * @params_size: size of params, if 0 and params pointer is different from NULL
 * the params pointer is valid but there is zero copy in Cherry thread.
 * @thread_task: The callback the thread will call to perform asynchronously processes.
 * @next_task: The next task.
 */
struct cherry_task {
	const void *context;
	void *params;
	size_t params_size;
	cherry_thread_task_t thread_task;
	struct cherry_task *next_task;
};

enum cherry_thread_state {
	CHERRY_THREAD_RUN,
	CHERRY_THREAD_STOP,
};

/**
 * struct cherry_thread - Internal Cherry thread structure.
 * @tasks: Pointer to cherry_task_control instance to manage a pool of tasks.
 * @cherry_thread: Pointer to the Cherry thread instance.
 * @sem_wakeup: Semaphore to wakeup the Cherry thread.
 * @stop: Indicate the thread to leave, asynchronously or synchronously.
 * @tasks: Linked list to tasks.
 * @prio_tasks: Linked list to priority tasks.
 * @task_mutex: Mutex to protect the list.
 * @under_destroy: True to indicate the thread is under destroy call.
 */
struct cherry_thread {
	struct qthread *cherry_thread;
	struct qsemaphore *sem_wakeup;
	enum cherry_thread_state state;
	struct cherry_task *tasks;
	struct cherry_task *prio_tasks;
	struct qmutex *task_mutex;
	bool under_destroy;
};

/**
 * cherry_thread_create() - Initialize Cherry thread.
 * @thread_ctx: Cherry thread context.
 *
 * Returns:
 *  - &CHERRY_ERR_NONE if succeed
 *  - &CHERRY_ERR_INTERNAL for other errors
 */
enum cherry_err cherry_thread_create(struct cherry *cherry_ctx);

/**
 * cherry_thread_send_list_task() - Send a task to the Cherry thread.
 * @thread_ctx: Cherry thread context.
 * @context: Context of component (core or FiRa for example).
 * @params: Pointer to parameters if any.
 * @params_size: Parameters size if any.
    - If sero and params not null only params pointer is copied.
    - If not zero and params not null an allocation is done to copy the params.
 * @thread_task: The callback to call to process the task.
 * @destroy: True to indicate the thread will be destroyed and no more task has
 *           to be queued after the thread_task of this call.
 *
 * Send a task to the Cherry thread: add in a cherry_task element and wakeup the thread.
 * Returns:
 *  - &CHERRY_ERR_NONE if succeed
 *  - &CHERRY_ERR_INTERNAL for other errors
 */
enum cherry_err cherry_thread_send_list_task(struct cherry_thread *thread_ctx,
					     const void *context,
					     const void *params,
					     size_t params_size,
					     cherry_thread_task_t thread_task,
					     bool destroy);

/**
 * cherry_thread_send_prio_task() - Send a prio task to the Cherry thread.
 * @thread_ctx: Cherry thread context.
 * @context: Context of component (core or FiRa for example).
 * @params: Pointer to parameters if any.
 * @params_size: Parameters size if any.
    - If sero and params not null only params pointer is copied.
    - If not zero and params not null an allocation is done to copy the params.
 * @thread_task: The callback to call to process the task.
 *
 * Send a prio task to the Cherry thread: add in a cherry_task element and wakeup the thread.
 * Returns:
 *  - &CHERRY_ERR_NONE if succeed
 *  - &CHERRY_ERR_INTERNAL for other errors
 */
enum cherry_err cherry_thread_send_prio_task(struct cherry_thread *thread_ctx,
					     const void *context,
					     const void *params,
					     size_t params_size,
					     cherry_thread_task_t thread_task);

/**
 * cherry_thread_stop() - Stop the Cherry thread.
 * @thread_ctx: Cherry thread context.
 *
 * Indicate to stop the Cherry thread, it releases any allocated memory.
 * Synchronously, cherry_thread_join has to be called.
 */
void cherry_thread_stop(struct cherry_thread *thread_ctx);

/**
 * cherry_thread_destroy - Destroy the Cherry thread.
 * @thread_ctx: Cherry thread context.
 *
 * Free all memory allocated by Cherry thread.
 */
void cherry_thread_destroy(struct cherry_thread *thread_ctx);

/**
 * cherry_thread_join - Join the Cherry thread.
 * @thread_ctx: Cherry thread context.
 *
 */
void cherry_thread_join(struct cherry_thread *thread_ctx);

/**
 * cherry_thread_remove_pending_tasks - Remove all pending tasks according to a context.
 * @thread_ctx: Cherry thread context.
 * @context: The tasks' context to free.
 *
 * Remove all pending tasks from the list and free them.
 */
void cherry_thread_remove_pending_tasks(struct cherry_thread *thread_ctx,
					const void *context);

/**
 * cherry_thread_process_prio_task - Process a prio task.
 * @thread_ctx: Cherry thread context.
 * @timeout_ms: The timeout for waiting a task in the prio taks' list.
 *
 * Process a prio task.
 */
int cherry_thread_process_prio_task(struct cherry_thread *thread_ctx,
				    uint32_t timeout_ms);

#endif /* CHERRY_THREAD_H */
