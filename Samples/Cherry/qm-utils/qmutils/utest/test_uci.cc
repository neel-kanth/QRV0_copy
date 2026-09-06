/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "test_uci.hh"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_channel_callback.hh"

#include <cstddef>
#include <cstring>

#define BUF_SIZE 4096

using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::SetErrnoAndReturn;
using ::testing::_;

ACTION_P2(SetTidAndReturn, t, r)
{
	*arg0 = t;
	return r;
}

ACTION_P2(SetRetvalAndReturn, v, r)
{
	*arg1 = v;
	return r;
}

ACTION_P2(SetIoctlArgAndReturn, a, r)
{
	*(unsigned int *)arg2 = a;
	return r;
}

extern "C" {
#include "qmutils/qmutils.h"
}

void TestUci::SetUp()
{
	calloc_buf = new char[BUF_SIZE]();
}

void TestUci::TearDown()
{
	if (!mock_alloc.pointers.empty()) {
		// MockQmalloc need all allocated pointers are freed.
		EXPECT_CALL(mock_alloc, qfree_internal(calloc_buf)).Times(1);
		qfree(Q2M(calloc_buf));
	}
	delete[] calloc_buf;
}

/**********************************************************************/

using TestUciInit = TestUci;

TEST_F(TestUciInit, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(calloc_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_uci_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);
}

TEST_F(TestUciInit, cb_error)
{
	MockChannelCallback mock_cb;

	qmhandle hnd = qmu_uci_init(NULL, &mock_cb);
	EXPECT_EQ(hnd, nullptr);
}

TEST_F(TestUciInit, calloc_error)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(ReturnNull());
	qmhandle hnd = qmu_uci_init(mock_cb.cb, &mock_cb);
	EXPECT_EQ(hnd, nullptr);
}

TEST_F(TestUciInit, qmchannel_init_error)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(calloc_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(-1));
	EXPECT_CALL(mock_alloc, qfree_internal).WillOnce(Return());
	qmhandle hnd = qmu_uci_init(mock_cb.cb, &mock_cb);
	EXPECT_EQ(hnd, nullptr);
}

/**********************************************************************/

using TestUciStart = TestUci;

TEST_F(TestUciStart, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(calloc_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_uci_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_pthread, pthread_create).WillOnce(Return(0));
	int ret = qmu_uci_start(hnd);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestUciStart, args_error)
{
	int ret = qmu_uci_start(NULL);
	EXPECT_EQ(ret, -EINVAL);
}

/**********************************************************************/

using TestUciDestroy = TestUci;

TEST_F(TestUciDestroy, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(calloc_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_uci_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_qmchannel, qmchannel_close).WillOnce(Return(0));
	EXPECT_CALL(mock_alloc, qfree_internal).WillOnce(Return());
	int ret = qmu_uci_destroy(hnd);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestUciDestroy, started_success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(calloc_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_uci_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_pthread, pthread_create)
		.WillOnce(SetTidAndReturn(42, 0));
	int ret = qmu_uci_start(hnd);
	EXPECT_EQ(ret, 0);

	EXPECT_CALL(mock_pthread, pthread_cancel).WillOnce(Return(0));
	EXPECT_CALL(mock_pthread, pthread_join).WillOnce(Return(0));
	EXPECT_CALL(mock_qmchannel, qmchannel_close).WillOnce(Return(0));
	EXPECT_CALL(mock_alloc, qfree_internal).WillOnce(Return());
	ret = qmu_uci_destroy(hnd);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestUciDestroy, cancelled_success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(calloc_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_uci_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_pthread, pthread_create)
		.WillOnce(SetTidAndReturn(42, 0));
	int ret = qmu_uci_start(hnd);
	EXPECT_EQ(ret, 0);

	EXPECT_CALL(mock_pthread, pthread_cancel).WillOnce(Return(0));
	EXPECT_CALL(mock_pthread, pthread_join)
		.WillOnce(SetRetvalAndReturn(PTHREAD_CANCELED, 0));
	EXPECT_CALL(mock_qmchannel, qmchannel_close).WillOnce(Return(0));
	EXPECT_CALL(mock_alloc, qfree_internal).WillOnce(Return());
	ret = qmu_uci_destroy(hnd);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestUciDestroy, args_error)
{
	int ret = qmu_uci_destroy(NULL);
	EXPECT_EQ(ret, -EINVAL);
}

TEST_F(TestUciDestroy, pthread_cancel_error)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(calloc_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_uci_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_pthread, pthread_create)
		.WillOnce(SetTidAndReturn(42, 0));
	int ret = qmu_uci_start(hnd);
	EXPECT_EQ(ret, 0);

	EXPECT_CALL(mock_pthread, pthread_cancel).WillOnce(Return(42));
	ret = qmu_uci_destroy(hnd);
	EXPECT_EQ(ret, -42);
}

TEST_F(TestUciDestroy, pthread_join_error)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(calloc_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_uci_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_pthread, pthread_create)
		.WillOnce(SetTidAndReturn(42, 0));
	int ret = qmu_uci_start(hnd);
	EXPECT_EQ(ret, 0);

	EXPECT_CALL(mock_pthread, pthread_cancel).WillOnce(Return(0));
	EXPECT_CALL(mock_pthread, pthread_join).WillOnce(Return(42));
	ret = qmu_uci_destroy(hnd);
	EXPECT_EQ(ret, -42);
}

TEST_F(TestUciDestroy, qmchannel_close_error)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(calloc_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_uci_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_qmchannel, qmchannel_close).WillOnce(Return(-42));
	int ret = qmu_uci_destroy(hnd);
	EXPECT_EQ(ret, -42);
}

/**********************************************************************/

using TestUciThread = TestUci;

/* This test will exit the thread routine on a poll error on the second loop. */
TEST_F(TestUciThread, pthread_success)
{
	MockChannelCallback mock_cb;
	void *(*thread_routine)(void *);
	void *thread_arg;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(calloc_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_uci_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_pthread, pthread_create)
		.WillOnce(Invoke(
			[&thread_routine, &thread_arg](
				pthread_t *thread, const pthread_attr_t *attr,
				void *(*start_routine)(void *), void *arg) {
				thread_routine = start_routine;
				thread_arg = arg;
				return 0;
			}));
	int ret = qmu_uci_start(hnd);
	EXPECT_EQ(ret, 0);

	EXPECT_CALL(mock_qmchannel, qmchannel_getpollevent)
		.WillOnce(Return(POLLIN));
	EXPECT_CALL(mock_qmchannel, qmchannel_getfd).WillOnce(Return(42));
	EXPECT_CALL(mock_pthread, pthread_setcancelstate)
		.WillRepeatedly(Return(0));
	EXPECT_CALL(mock_io, poll)
		.WillOnce(Return(0))
		.WillOnce(SetErrnoAndReturn(EFAULT, -1));
	void *retptr = thread_routine(thread_arg);
	EXPECT_EQ(retptr, (void *)EFAULT);
}

/* This test will exit the thread routine on a poll error on the second loop. */
TEST_F(TestUciThread, pthread_read_success)
{
	MockChannelCallback mock_cb;
	void *(*thread_routine)(void *);
	void *thread_arg;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(calloc_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_uci_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_pthread, pthread_create)
		.WillOnce(Invoke(
			[&thread_routine, &thread_arg](
				pthread_t *thread, const pthread_attr_t *attr,
				void *(*start_routine)(void *), void *arg) {
				thread_routine = start_routine;
				thread_arg = arg;
				return 0;
			}));
	int ret = qmu_uci_start(hnd);
	EXPECT_EQ(ret, 0);

	EXPECT_CALL(mock_qmchannel, qmchannel_getpollevent)
		.WillOnce(Return(POLLIN));
	EXPECT_CALL(mock_qmchannel, qmchannel_getfd).WillOnce(Return(42));
	EXPECT_CALL(mock_pthread, pthread_setcancelstate)
		.WillRepeatedly(Return(0));
	EXPECT_CALL(mock_io, poll)
		.WillOnce(Invoke(
			[](struct pollfd *fds, nfds_t nfds, int timeout) {
				fds[0].revents = POLLIN;
				return 1;
			}))
		.WillOnce(SetErrnoAndReturn(EFAULT, -1));
	EXPECT_CALL(mock_qmchannel, qmchannel_read).WillOnce(Return(0));
	void *retptr = thread_routine(thread_arg);
	EXPECT_EQ(retptr, (void *)EFAULT);
}

TEST_F(TestUciThread, pthread_setcancelstate_error)
{
	MockChannelCallback mock_cb;
	void *(*thread_routine)(void *);
	void *thread_arg;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(calloc_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_uci_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_pthread, pthread_create)
		.WillOnce(Invoke(
			[&thread_routine, &thread_arg](
				pthread_t *thread, const pthread_attr_t *attr,
				void *(*start_routine)(void *), void *arg) {
				thread_routine = start_routine;
				thread_arg = arg;
				return 0;
			}));
	int ret = qmu_uci_start(hnd);
	EXPECT_EQ(ret, 0);

	/* Fail on ENABLE pthread_setcancelstate(). */
	EXPECT_CALL(mock_qmchannel, qmchannel_getpollevent)
		.WillOnce(Return(POLLIN));
	EXPECT_CALL(mock_qmchannel, qmchannel_getfd).WillOnce(Return(42));
	EXPECT_CALL(mock_pthread,
		    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, _))
		.WillOnce(Return(EFAULT));
	void *retptr = thread_routine(thread_arg);
	EXPECT_EQ(retptr, (void *)EFAULT);

	/* Fail on DISABLE pthread_setcancelstate(). */
	EXPECT_CALL(mock_qmchannel, qmchannel_getpollevent)
		.WillOnce(Return(POLLIN));
	EXPECT_CALL(mock_qmchannel, qmchannel_getfd).WillOnce(Return(42));
	EXPECT_CALL(mock_pthread,
		    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, _))
		.WillOnce(Return(0));
	EXPECT_CALL(mock_io, poll).WillOnce(Return(1));
	EXPECT_CALL(mock_pthread,
		    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, _))
		.WillOnce(Return(EFAULT));
	retptr = thread_routine(thread_arg);
	EXPECT_EQ(retptr, (void *)EFAULT);
}

TEST_F(TestUciThread, pthread_read_error)
{
	MockChannelCallback mock_cb;
	void *(*thread_routine)(void *);
	void *thread_arg;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(calloc_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_uci_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_pthread, pthread_create)
		.WillOnce(Invoke(
			[&thread_routine, &thread_arg](
				pthread_t *thread, const pthread_attr_t *attr,
				void *(*start_routine)(void *), void *arg) {
				thread_routine = start_routine;
				thread_arg = arg;
				return 0;
			}));
	int ret = qmu_uci_start(hnd);
	EXPECT_EQ(ret, 0);

	EXPECT_CALL(mock_qmchannel, qmchannel_getpollevent)
		.WillOnce(Return(POLLIN));
	EXPECT_CALL(mock_qmchannel, qmchannel_getfd).WillOnce(Return(42));
	EXPECT_CALL(mock_pthread, pthread_setcancelstate)
		.WillRepeatedly(Return(0));
	EXPECT_CALL(mock_io, poll)
		.WillOnce(Invoke(
			[](struct pollfd *fds, nfds_t nfds, int timeout) {
				fds[0].revents = POLLIN;
				return 1;
			}));
	EXPECT_CALL(mock_qmchannel, qmchannel_read).WillOnce(Return(-EFAULT));
	void *retptr = thread_routine(thread_arg);
	EXPECT_EQ(retptr, (void *)EFAULT);
}

/**********************************************************************/

class TestUciHandleData : public TestUci {
    protected:
	char *malloc_buf;

	void SetUp() override
	{
		TestUci::SetUp();
		malloc_buf = new char[BUF_SIZE]();
	}

	void TearDown() override
	{
		TestUci::TearDown();
		if (!mock_alloc.pointers.empty()) {
			// MockQmalloc need all allocated pointers are freed.
			EXPECT_CALL(mock_alloc, qfree_internal(malloc_buf))
				.Times(1);
			qfree(Q2M(malloc_buf));
		}
		delete[] malloc_buf;
	}
};

TEST_F(TestUciHandleData, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(calloc_buf));
	/* Do not mock qmchannel_init(), but instead mock the system calls it
	 * contains, so the callback is registered. */
	EXPECT_CALL(mock_qmchannel, qmchannel_init)
		.WillOnce(Invoke(qmchannel_init));
	EXPECT_CALL(mock_io, open).WillOnce(Return(42));
	EXPECT_CALL(mock_io, ioctl)
		.WillOnce(SetIoctlArgAndReturn(QMCHANNEL_TYPE_UCI, 0));
	qmhandle hnd = qmu_uci_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	/* Call qmchannel_read(), so that the uci_handle_data() callback gets
	 * called. */
	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(malloc_buf));
	EXPECT_CALL(mock_io, read).WillOnce(Return(0));
	EXPECT_CALL(mock_cb, callback).WillOnce(Return(0));
	int ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, 0);
}
