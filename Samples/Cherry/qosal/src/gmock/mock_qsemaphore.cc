/**
 * @file      mock_qsemaphore.cc
 *
 * @brief     Implementations for mock qsemaphore
 *
 * @author    Qorvo Paris Applications
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */
#include "mock_qsemaphore.hh"

#include <assert.h>
#include <cstdlib>
#include <gtest/gtest.h>

extern "C" {
#include <errno.h>
#include <limits.h>
#include <semaphore.h>
#include <stdint.h>
#include <time.h>
}

struct qsemaphore {
	sem_t sem;
	uint32_t max_count;
};

/* Implementation copied from src/linux/qsemaphore.h but with mock included. */

struct qsemaphore *qsemaphore_init(uint32_t init_count, uint32_t max_count)
{
	EXPECT_LE(init_count, max_count);

	auto mock = MockQsemaphore::get_singleton();
	if (mock && mock->qsemaphore_init_mocked)
		return mock->qsemaphore_init(init_count, max_count);

	struct qsemaphore *sem = (struct qsemaphore *)malloc(sizeof(*sem));
	EXPECT_NE(sem, nullptr);

	int r = sem_init(&sem->sem, 0, init_count);
	if (r) {
		free(sem);
		return NULL;
	}

	if (mock)
		mock->pointers.insert(sem);
	sem->max_count = max_count;
	return sem;
}

void qsemaphore_deinit(struct qsemaphore *sem)
{
	ASSERT_NE(sem, nullptr);

	auto mock = MockQsemaphore::get_singleton();
	if (mock && mock->qsemaphore_deinit_mocked)
		return mock->qsemaphore_deinit(sem);

	sem_destroy(&sem->sem);
	if (mock) {
		auto it = mock->pointers.find(sem);
		EXPECT_NE(it, mock->pointers.end());
		mock->pointers.erase(it);
	}
	free(sem);
}

enum qerr qsemaphore_take(struct qsemaphore *sem, uint32_t timeout_ms)
{
	EXPECT_NE(sem, nullptr);

	auto mock = MockQsemaphore::get_singleton();
	if (mock && mock->qsemaphore_take_mocked)
		return mock->qsemaphore_take(sem, timeout_ms);

	int r;
	struct timespec ts;
	if (timeout_ms == QOSAL_WAIT_FOREVER)
		r = sem_wait(&sem->sem);
	else if ((r = clock_gettime(CLOCK_REALTIME, &ts)) == 0) {
		ts.tv_sec += timeout_ms / 1000;
		ts.tv_nsec += (timeout_ms % 1000) * 1000000;
		if (ts.tv_nsec >= 1000000000L) {
			ts.tv_nsec -= 1000000000L;
			++ts.tv_sec;
		}
		r = sem_timedwait(&sem->sem, &ts);
	}
	if (r == -1)
		r = -errno;
	return qerr_convert_os_to_qerr(r);
}

enum qerr qsemaphore_give(struct qsemaphore *sem)
{
	EXPECT_NE(sem, nullptr);

	auto mock = MockQsemaphore::get_singleton();
	if (mock && mock->qsemaphore_give_mocked)
		return mock->qsemaphore_give(sem);

	int r, val;
	r = sem_getvalue(&sem->sem, &val);
	if (r || val >= (int)sem->max_count)
		return QERR_EINVAL;
	r = sem_post(&sem->sem);
	return qerr_convert_os_to_qerr(r);
}

MockQsemaphore *MockQsemaphore::instance_ = nullptr;

MockQsemaphore::MockQsemaphore()
{
	/* Check singleton instance. */
	assert(!MockQsemaphore::instance_);
	MockQsemaphore::instance_ = this;
}

MockQsemaphore::~MockQsemaphore()
{
	/* Check that all pointers are free. */
	assert(pointers.empty());
	/* Clear singleton pointer.*/
	MockQsemaphore::instance_ = nullptr;
}

MockQsemaphore *MockQsemaphore::get_singleton()
{
	/* Return the singleton object if exists. */
	return MockQsemaphore::instance_;
}

void MockQsemaphore::explicit_call()
{
	auto mock = MockQsemaphore::get_singleton();

	mock->qsemaphore_init_mocked = true;
	mock->qsemaphore_deinit_mocked = true;
	mock->qsemaphore_give_mocked = true;
	mock->qsemaphore_take_mocked = true;
}

void MockQsemaphore::implicit_call()
{
	auto mock = MockQsemaphore::get_singleton();

	mock->qsemaphore_init_mocked = false;
	mock->qsemaphore_deinit_mocked = false;
	mock->qsemaphore_give_mocked = false;
	mock->qsemaphore_take_mocked = false;
}
