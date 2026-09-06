/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <gmock/gmock.h>

extern "C" {
#include "../src/cherry_thread.h"

#include <cherry/cherry.h>
}

class MockCherryThread : public cherry_thread {
    public:
	MOCK_METHOD(enum cherry_err, cherry_thread_create,
		    (struct cherry * cherry_ctx));
	MOCK_METHOD(enum cherry_err, cherry_thread_send_list_task,
		    (struct cherry_thread * thread_ctx, const void *context,
		     const void *params, size_t params_size,
		     cherry_thread_task_t thread_task, bool destroy));
	MOCK_METHOD(enum cherry_err, cherry_thread_send_prio_task,
		    (struct cherry_thread * thread_ctx, const void *context,
		     const void *params, size_t params_size,
		     cherry_thread_task_t thread_task));
	MOCK_METHOD(void, cherry_thread_stop,
		    (struct cherry_thread * thread_ctx));
	MOCK_METHOD(void, cherry_thread_destroy,
		    (struct cherry_thread * thread_ctx));
	MOCK_METHOD(void, cherry_thread_join,
		    (struct cherry_thread * thread_ctx));
	MOCK_METHOD(void, cherry_thread_remove_pending_tasks,
		    (struct cherry_thread * thread_ctx, const void *context));
	MOCK_METHOD(int, cherry_thread_process_prio_task,
		    (struct cherry_thread * thread_ctx, uint32_t timeout_ms));

    public:
	MockCherryThread();
	virtual ~MockCherryThread();

	static MockCherryThread *Instance();

    private:
	static MockCherryThread *instance_;
};
