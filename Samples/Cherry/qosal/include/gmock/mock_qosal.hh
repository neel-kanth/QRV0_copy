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

#pragma once

#include "qerr.h"
#include "qpm.h"
#include "qthread.h"

#include <gmock/gmock.h>
#include <pthread.h>
#include <stdint.h>

struct qthread {
	pthread_t thread_id;
	qthread_func thread_func;
	void *thread_func_arg;
};

class MockQosal {
    public:
	// clang-format off
	MOCK_METHOD(enum qerr, qpm_set_low_power_mode, (bool enabled));
	MOCK_METHOD(bool, qpm_get_low_power_mode, ());
	MOCK_METHOD(enum qerr, qpm_get_min_inactivity_s4, (uint32_t *time_ms));
	MOCK_METHOD(enum qerr, qpm_set_min_inactivity_s4, (uint32_t time_ms));
	MOCK_METHOD(int64_t, qtime_get_uptime_us, ());
	MOCK_METHOD(void, qtime_msleep, (int32_t ms));
	MOCK_METHOD(void, qmemstat_get, (struct qmemstats *stats));
	MOCK_METHOD(int, qstackstat_count_get, ());
	MOCK_METHOD(int, qstackstat_get, (struct qstackstats *stats, int stack_count));
	MOCK_METHOD(void, qpm_sleep_state_lock, (enum qpm_sleep_state state, uint8_t substate_id));
	MOCK_METHOD(void, qpm_sleep_state_unlock, (enum qpm_sleep_state state, uint8_t substate_id));
	MOCK_METHOD(bool, qpm_sleep_state_is_active, (enum qpm_sleep_state state, uint8_t substate_id));
	MOCK_METHOD(const char *, qerr_to_str, (enum qerr));
	MOCK_METHOD(struct qsignal *, qsignal_init, ());
	MOCK_METHOD(void, qsignal_deinit, (struct qsignal *signal));;
	MOCK_METHOD(enum qerr, qsignal_raise, (struct qsignal *signal, int value));
	MOCK_METHOD(enum qerr, qsignal_wait, (struct qsignal *signal, int *value, uint32_t timeout_ms));
	MOCK_METHOD(enum qerr, qtracing_init, ());
	MOCK_METHOD(struct qthread*, qthread_create, (qthread_func thread, void *arg, const char *name, void *stack, uint32_t stack_size, enum qthread_priority prio));
	MOCK_METHOD(enum qerr, qthread_join, (struct qthread *thread));
	MOCK_METHOD(enum qerr, qthread_delete, (struct qthread *thread));
	// clang-format on

    public:
	static MockQosal *instance;
	bool qthread_internal_mocked = false;

	MockQosal();
	virtual ~MockQosal();

	static MockQosal *get_singleton();
};
