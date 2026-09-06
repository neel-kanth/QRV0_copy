/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <gmock/gmock.h>
#include <pthread.h>

extern "C" {
int pthread_create_mock(pthread_t *thread, const pthread_attr_t *attr,
			void *(*start_routine)(void *), void *arg);
int pthread_setcancelstate_mock(int state, int *oldstate);
int pthread_cancel_mock(pthread_t thread);
int pthread_join_mock(pthread_t thread, void **retval);
}

class MockPthread {
    public:
	/* clang-format off */
	MOCK_METHOD(int, pthread_create,
		    (pthread_t *thread, const pthread_attr_t *attr,
		     void *(*start_routine) (void *), void *arg));
	MOCK_METHOD(int, pthread_setcancelstate, (int state, int *oldstate));
	MOCK_METHOD(int, pthread_cancel, (pthread_t thread));
	MOCK_METHOD(int, pthread_join, (pthread_t thread, void **retval));
	/* clang-format on */

    public:
	MockPthread();
	virtual ~MockPthread();
	void implicit_call()
	{
		active = false;
	}
	void explicit_call()
	{
		active = true;
	}
	bool mocked()
	{
		return active;
	}

    public:
	static MockPthread *get_singleton();

    private:
	static MockPthread *instance_;
	bool active = true;
};
