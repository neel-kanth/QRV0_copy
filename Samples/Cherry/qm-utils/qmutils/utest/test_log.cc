/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "test_log.hh"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_channel_callback.hh"

#include <cstddef>
#include <cstring>

#define BUF_SIZE 4096

using ::testing::Invoke;
using ::testing::Return;
using ::testing::_;

ACTION_P2(CopyBufAndReturn, t, r)
{
	memcpy(arg1, t, r);
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

void TestLog::SetUp()
{
	handle_buf = new char[BUF_SIZE]();
}

void TestLog::TearDown()
{
	if (!mock_alloc.pointers.empty()) {
		// MockQmalloc need all allocated pointers are freed.
		EXPECT_CALL(mock_alloc, qfree_internal(handle_buf)).Times(1);
		qfree(Q2M(handle_buf));
	}
	delete[] handle_buf;
}

/**********************************************************************/

using TestLogInit = TestLog;

TEST_F(TestLogInit, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_log_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);
}

TEST_F(TestLogInit, read_cb_error)
{
	MockChannelCallback mock_cb;

	qmhandle hnd = qmu_log_init(NULL, &mock_cb);
	EXPECT_EQ(hnd, nullptr);
}

TEST_F(TestLogInit, qmchannel_init_error)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(-1));
	EXPECT_CALL(mock_alloc, qfree_internal(handle_buf)).WillOnce(Return());
	qmhandle hnd = qmu_log_init(mock_cb.cb, &mock_cb);
	EXPECT_EQ(hnd, nullptr);
}

/**********************************************************************/

using TestLogStart = TestLog;

TEST_F(TestLogStart, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	/* Do not mock qmchannel_init(), but instead mock the system calls it
	 * contains, so the callback is registered. */
	EXPECT_CALL(mock_qmchannel, qmchannel_init)
		.WillOnce(Invoke(qmchannel_init));
	EXPECT_CALL(mock_io, open).WillOnce(Return(0));
	EXPECT_CALL(mock_io, ioctl)
		.WillOnce(SetIoctlArgAndReturn(QMCHANNEL_TYPE_LOG, 0));
	qmhandle hnd = qmu_log_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_pthread, pthread_create).WillOnce(Return(0));
	int ret = qmu_log_start(hnd);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestLogStart, invalid_channel_type)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	/* Do not mock qmchannel_init(), but instead mock the system calls it
	 * contains, so the callback is registered. */
	EXPECT_CALL(mock_qmchannel, qmchannel_init)
		.WillOnce(Invoke(qmchannel_init));
	EXPECT_CALL(mock_io, open).WillOnce(Return(0));
	EXPECT_CALL(mock_io, ioctl)
		.WillOnce(SetIoctlArgAndReturn(QMCHANNEL_TYPE_QTRACE, 0));
	qmhandle hnd = qmu_qtrace_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	/* Expect -EINVAL error as handle type is not valid (qtrace instead of log) */
	int ret = qmu_log_start(hnd);
	EXPECT_EQ(ret, -EINVAL);
}

/**********************************************************************/

using TestLogDestroy = TestLog;

TEST_F(TestLogDestroy, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init)
		.WillOnce(Invoke(qmchannel_init));
	EXPECT_CALL(mock_io, open).WillOnce(Return(0));
	EXPECT_CALL(mock_io, ioctl)
		.WillOnce(SetIoctlArgAndReturn(QMCHANNEL_TYPE_LOG, 0));
	qmhandle hnd = qmu_log_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_qmchannel, qmchannel_close).WillOnce(Return(0));
	EXPECT_CALL(mock_alloc, qfree_internal).WillOnce(Return());
	int ret = qmu_log_destroy(hnd);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestLogDestroy, invalid_channel_type)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init)
		.WillOnce(Invoke(qmchannel_init));
	EXPECT_CALL(mock_io, open).WillOnce(Return(0));
	EXPECT_CALL(mock_io, ioctl)
		.WillOnce(SetIoctlArgAndReturn(QMCHANNEL_TYPE_QTRACE, 0));
	/* Use qmu_qtrace_init to get a wrong handle type */
	qmhandle hnd = qmu_qtrace_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	/* Call qmu_log_destroy with a wrong handle */
	int ret = qmu_log_destroy(hnd);
	/* Expect -EINVAL error as handle type is not valid */
	EXPECT_EQ(ret, -EINVAL);
}

/**********************************************************************/

using TestLogGetSources = TestLog;

TEST_F(TestLogGetSources, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init)
		.WillOnce(Invoke(qmchannel_init));
	EXPECT_CALL(mock_io, open).WillOnce(Return(0));
	EXPECT_CALL(mock_io, ioctl)
		.WillOnce(SetIoctlArgAndReturn(QMCHANNEL_TYPE_LOG, 0));
	qmhandle hnd = qmu_log_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_io, write).WillOnce(Return(2));
	int ret = qmu_log_get_sources(hnd);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestLogGetSources, invalid_channel_type)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init)
		.WillOnce(Invoke(qmchannel_init));
	EXPECT_CALL(mock_io, open).WillOnce(Return(0));
	EXPECT_CALL(mock_io, ioctl)
		.WillOnce(SetIoctlArgAndReturn(QMCHANNEL_TYPE_QTRACE, 0));
	/* Use qmu_qtrace_init to get a wrong handle type */
	qmhandle hnd = qmu_qtrace_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	/* Call qmu_log_get_sources with a wrong handle */
	int ret = qmu_log_get_sources(hnd);
	/* Expect -EINVAL error as handle type is not valid */
	EXPECT_EQ(ret, -EINVAL);
}

/**********************************************************************/

class TestLogHandleData : public TestLog {
    protected:
	char *channel_buf;

	void SetUp() override
	{
		TestLog::SetUp();
		channel_buf = new char[BUF_SIZE]();
	}

	void TearDown() override
	{
		TestLog::TearDown();
		if (!mock_alloc.pointers.empty()) {
			// MockQmalloc need all allocated pointers are freed.
			EXPECT_CALL(mock_alloc, qfree_internal(channel_buf))
				.Times(1);
			qfree(Q2M(channel_buf));
		}
		delete[] channel_buf;
	}
};

/*
 * This test uses qmu_log_init() to create a log channel, then calls
 * qmchannel_read() to simulate the reception of messages on the channel.
 */
TEST_F(TestLogHandleData, success)
{
	MockChannelCallback mock_cb;

	/* First, use qmu_log_init() to create a log channel. */
	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	/* Do not mock qmchannel_init(), but instead mock the system calls it
	 * contains, so the callback is registered. */
	EXPECT_CALL(mock_qmchannel, qmchannel_init)
		.WillOnce(Invoke(qmchannel_init));
	EXPECT_CALL(mock_io, open).WillOnce(Return(0));
	EXPECT_CALL(mock_io, ioctl)
		.WillOnce(SetIoctlArgAndReturn(QMCHANNEL_TYPE_LOG, 0));
	qmhandle hnd = qmu_log_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	/* Then, call qmchannel_read() to simulate the reception
	 *	of the get log sources response. */
	char log_sources_buf[] =
		/* command id,  length,  count,  id,  level, module name */
		{ 0x00, 0x03, 0x00, 0x08, 0x01, 0x00,
		  0x03, 'n',  'a',  'm',  'e',	'\0' };

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(log_sources_buf,
					   sizeof(log_sources_buf)));
	EXPECT_CALL(mock_alloc, qmalloc_internal(_))
		.WillOnce(Return(channel_buf));
	EXPECT_CALL(mock_cb, callback).WillOnce(Return(0));

	int ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, 0);
}

/**********************************************************************/

using TestLogGetLevel = TestLog;

TEST_F(TestLogGetLevel, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init)
		.WillOnce(Invoke(qmchannel_init));
	EXPECT_CALL(mock_io, open).WillOnce(Return(0));
	EXPECT_CALL(mock_io, ioctl)
		.WillOnce(SetIoctlArgAndReturn(QMCHANNEL_TYPE_LOG, 0));
	qmhandle hnd = qmu_log_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_io, write).WillOnce(Return(3));
	int ret = qmu_log_get_level(hnd, 0);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestLogGetLevel, invalid_channel_type)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init)
		.WillOnce(Invoke(qmchannel_init));
	EXPECT_CALL(mock_io, open).WillOnce(Return(0));
	EXPECT_CALL(mock_io, ioctl)
		.WillOnce(SetIoctlArgAndReturn(QMCHANNEL_TYPE_QTRACE, 0));
	/* Use qmu_qtrace_init to get a wrong handle type */
	qmhandle hnd = qmu_qtrace_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	/* Call qmu_log_get_level with a wrong handle */
	int ret = qmu_log_get_level(hnd, 0);
	/* Expect -EINVAL error as handle type is not valid */
	EXPECT_EQ(ret, -EINVAL);
}

/**********************************************************************/

using TestLogSetLevel = TestLog;

TEST_F(TestLogSetLevel, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init)
		.WillOnce(Invoke(qmchannel_init));
	EXPECT_CALL(mock_io, open).WillOnce(Return(0));
	EXPECT_CALL(mock_io, ioctl)
		.WillOnce(SetIoctlArgAndReturn(QMCHANNEL_TYPE_LOG, 0));
	qmhandle hnd = qmu_log_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_io, write).WillOnce(Return(3));
	int ret = qmu_log_set_level(hnd, 0, 3);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestLogSetLevel, invalid_channel_type)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init)
		.WillOnce(Invoke(qmchannel_init));
	EXPECT_CALL(mock_io, open).WillOnce(Return(0));
	EXPECT_CALL(mock_io, ioctl)
		.WillOnce(SetIoctlArgAndReturn(QMCHANNEL_TYPE_QTRACE, 0));
	/* Use qmu_qtrace_init to get a wrong handle type */
	qmhandle hnd = qmu_qtrace_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	/* Call qmu_log_set_level with a wrong handle */
	int ret = qmu_log_set_level(hnd, 0, 3);
	/* Expect -EINVAL error as handle type is not valid */
	EXPECT_EQ(ret, -EINVAL);
}
