/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "mock_cherry_thread.hh"

MockCherryThread *MockCherryThread::instance_{ nullptr };

MockCherryThread::MockCherryThread()
{
	MockCherryThread::instance_ = this;
}

MockCherryThread::~MockCherryThread()
{
	MockCherryThread::instance_ = nullptr;
}

MockCherryThread *MockCherryThread::Instance()
{
	EXPECT_NE(MockCherryThread::instance_, nullptr);
	return MockCherryThread::instance_;
}

enum cherry_err cherry_thread_create(struct cherry *cherry_ctx)
{
	return MockCherryThread::Instance()->cherry_thread_create(cherry_ctx);
}

enum cherry_err cherry_thread_send_list_task(struct cherry_thread *thread_ctx,
					     const void *context,
					     const void *params,
					     size_t params_size,
					     cherry_thread_task_t thread_task,
					     bool destroy)
{
	return MockCherryThread::Instance()->cherry_thread_send_list_task(
		thread_ctx, context, params, params_size, thread_task, destroy);
}

enum cherry_err cherry_thread_send_prio_task(struct cherry_thread *thread_ctx,
					     const void *context,
					     const void *params,
					     size_t params_size,
					     cherry_thread_task_t thread_task)
{
	return MockCherryThread::Instance()->cherry_thread_send_list_task(
		thread_ctx, context, params, params_size, thread_task, false);
}

void cherry_thread_stop(struct cherry_thread *thread_ctx)
{
	return MockCherryThread::Instance()->cherry_thread_stop(thread_ctx);
}

void cherry_thread_destroy(struct cherry_thread *thread_ctx)
{
	return MockCherryThread::Instance()->cherry_thread_destroy(thread_ctx);
}

void cherry_thread_join(struct cherry_thread *thread_ctx)
{
	return MockCherryThread::Instance()->cherry_thread_join(thread_ctx);
}

void cherry_thread_remove_pending_tasks(struct cherry_thread *thread_ctx,
					const void *context)
{
	return MockCherryThread::Instance()->cherry_thread_remove_pending_tasks(
		thread_ctx, context);
}

int cherry_thread_process_prio_task(struct cherry_thread *thread_ctx,
				    uint32_t timeout_ms)
{
	return MockCherryThread::Instance()->cherry_thread_process_prio_task(
		thread_ctx, timeout_ms);
}
