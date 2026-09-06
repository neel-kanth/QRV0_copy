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

#include "mock_qosal.hh"

#include <gtest/gtest.h>

extern "C" {
#include "qprofiling.h"
#include "qsignal.h"
#include "qthread.h"
#include "qtime.h"
#include "qtracing.h"

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
}

#define MAX_THREAD_COMPAT 1
#define MAX_MAILBOX_COMPAT 1

static struct qthread threads[MAX_THREAD_COMPAT];
static unsigned int num_threads = 0;

MockQosal *MockQosal::instance{ nullptr };

MockQosal::MockQosal()
{
	instance = this;
}

MockQosal::~MockQosal()
{
	instance = nullptr;
}

MockQosal *MockQosal::get_singleton()
{
	/* Check singleton instance. */
	EXPECT_NE(MockQosal::instance, nullptr);
	/* Return the singleton object. */
	return MockQosal::instance;
}

void *os_thread_handler(void *arg)
{
	struct qthread *th = (struct qthread *)arg;

	th->thread_func(th->thread_func_arg);

	return NULL;
}

enum qerr qpm_set_low_power_mode(bool enabled)
{
	EXPECT_NE(MockQosal::instance, nullptr);
	return MockQosal::instance->qpm_set_low_power_mode(enabled);
}

bool qpm_get_low_power_mode(void)
{
	EXPECT_NE(MockQosal::instance, nullptr);
	return MockQosal::instance->qpm_get_low_power_mode();
}

enum qerr qpm_set_min_inactivity_s4(uint32_t time_ms)
{
	EXPECT_NE(MockQosal::instance, nullptr);
	return MockQosal::instance->qpm_set_min_inactivity_s4(time_ms);
}

enum qerr qpm_get_min_inactivity_s4(uint32_t *time_ms)
{
	EXPECT_NE(MockQosal::instance, nullptr);
	return MockQosal::instance->qpm_get_min_inactivity_s4(time_ms);
}

int64_t qtime_get_uptime_us(void)
{
	EXPECT_NE(MockQosal::instance, nullptr);
	return MockQosal::instance->qtime_get_uptime_us();
}

void qtime_msleep(int32_t ms)
{
	EXPECT_NE(MockQosal::instance, nullptr);
	MockQosal::instance->qtime_msleep(ms);
}

void qmemstat_get(struct qmemstats *stats)
{
	EXPECT_NE(MockQosal::instance, nullptr);
	MockQosal::instance->qmemstat_get(stats);
}

int qstackstat_count_get()
{
	EXPECT_NE(MockQosal::instance, nullptr);
	return MockQosal::instance->qstackstat_count_get();
}

int qstackstat_get(struct qstackstats *stats, int stack_count)
{
	EXPECT_NE(MockQosal::instance, nullptr);
	return MockQosal::instance->qstackstat_get(stats, stack_count);
}

void qpm_sleep_state_lock(enum qpm_sleep_state state, uint8_t substate_id)
{
	EXPECT_NE(MockQosal::instance, nullptr);
	MockQosal::instance->qpm_sleep_state_lock(state, substate_id);
}

void qpm_sleep_state_unlock(enum qpm_sleep_state state, uint8_t substate_id)
{
	EXPECT_NE(MockQosal::instance, nullptr);
	MockQosal::instance->qpm_sleep_state_unlock(state, substate_id);
}

bool qpm_sleep_state_is_active(enum qpm_sleep_state state, uint8_t substate_id)
{
	EXPECT_NE(MockQosal::instance, nullptr);
	return MockQosal::instance->qpm_sleep_state_is_active(state, substate_id);
}

const char *qerr_to_str(enum qerr error)
{
	EXPECT_NE(MockQosal::instance, nullptr);
	return MockQosal::instance->qerr_to_str(error);
}

struct qsignal *qsignal_init(void)
{
	EXPECT_NE(MockQosal::instance, nullptr);
	return MockQosal::instance->qsignal_init();
}

void qsignal_deinit(struct qsignal *signal)
{
	EXPECT_NE(MockQosal::instance, nullptr);
	return MockQosal::instance->qsignal_deinit(signal);
}

enum qerr qsignal_raise(struct qsignal *signal, int value)
{
	EXPECT_NE(MockQosal::instance, nullptr);
	return MockQosal::instance->qsignal_raise(signal, value);
}

enum qerr qsignal_wait(struct qsignal *signal, int *value, uint32_t timeout_ms)
{
	EXPECT_NE(MockQosal::instance, nullptr);
	return MockQosal::instance->qsignal_wait(signal, value, timeout_ms);
}

enum qerr qtracing_init(void)
{
	EXPECT_NE(MockQosal::instance, nullptr);
	return MockQosal::instance->qtracing_init();
}

struct qthread *qthread_create(qthread_func thread, void *arg, const char *name, void *stack,
			       uint32_t stack_size, enum qthread_priority prio)
{
	auto mock = MockQosal::get_singleton();

	if (mock->qthread_internal_mocked) {
		EXPECT_NE(MockQosal::instance, nullptr);
		return mock->qthread_create(thread, arg, name, stack, stack_size, prio);
	}

	if (num_threads >= MAX_THREAD_COMPAT)
		return NULL;

	struct qthread *th = &threads[num_threads++];

	th->thread_func = thread;
	th->thread_func_arg = arg;
	pthread_create(&th->thread_id, NULL, os_thread_handler, th);

	return th;
}

enum qerr qthread_join(struct qthread *thread)
{
	auto mock = MockQosal::get_singleton();

	if (mock->qthread_internal_mocked) {
		EXPECT_NE(MockQosal::instance, nullptr);
		return mock->qthread_join(thread);
	}

	/* TODO: Implement using POSIX. */
	return QERR_ENOTSUP;
}

enum qerr qthread_delete(struct qthread *thread)
{
	auto mock = MockQosal::get_singleton();

	if (mock->qthread_internal_mocked) {
		EXPECT_NE(MockQosal::instance, nullptr);
		return mock->qthread_delete(thread);
	}

	pthread_join(thread->thread_id, NULL);

	/* WARNING: threads must be terminated in reverse order of their
	 * creation */
	num_threads--;

	return QERR_SUCCESS;
}
