/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "mock_pthread.hh"

#include <gtest/gtest.h>
#include <pthread.h>

MockPthread *MockPthread::instance_ = nullptr;

int pthread_create_mock(pthread_t *thread, const pthread_attr_t *attr,
			void *(*start_routine)(void *), void *arg)
{
	auto mock = MockPthread::get_singleton();
	if (mock && mock->mocked())
		return mock->pthread_create(thread, attr, start_routine, arg);
	/* Call real pthread_create(). */
	return pthread_create(thread, attr, start_routine, arg);
}

int pthread_setcancelstate_mock(int state, int *oldstate)
{
	auto mock = MockPthread::get_singleton();
	if (mock && mock->mocked())
		return mock->pthread_setcancelstate(state, oldstate);
	/* Call real pthread_setcancelstate(). */
	return pthread_setcancelstate(state, oldstate);
}

int pthread_cancel_mock(pthread_t thread)
{
	auto mock = MockPthread::get_singleton();
	if (mock && mock->mocked())
		return mock->pthread_cancel(thread);
	/* Call real pthread_cancel(). */
	return pthread_cancel(thread);
}

int pthread_join_mock(pthread_t thread, void **retval)
{
	auto mock = MockPthread::get_singleton();
	if (mock && mock->mocked())
		return mock->pthread_join(thread, retval);
	/* Call real pthread_join(). */
	return pthread_join(thread, retval);
}

MockPthread::MockPthread()
{
	assert(!MockPthread::instance_);
	MockPthread::instance_ = this;
}

MockPthread *MockPthread::get_singleton()
{
	return MockPthread::instance_;
}

MockPthread::~MockPthread()
{
	MockPthread::instance_ = nullptr;
}
