/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "test_qtrace.hh"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_channel_callback.hh"

#include <cstddef>
#include <cstdint>
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

void TestQtrace::SetUp()
{
	handle_buf = new char[BUF_SIZE]();
}

void TestQtrace::TearDown()
{
	if (!mock_alloc.pointers.empty()) {
		// MockQmalloc need all allocated pointers are freed.
		EXPECT_CALL(mock_alloc, qfree_internal(handle_buf)).Times(1);
		qfree(Q2M(handle_buf));
	}
	delete[] handle_buf;
}

/**********************************************************************/

using TestQtraceInit = TestQtrace;

TEST_F(TestQtraceInit, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_qtrace_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);
}

TEST_F(TestQtraceInit, read_cb_error)
{
	MockChannelCallback mock_cb;

	qmhandle hnd = qmu_qtrace_init(NULL, &mock_cb);
	EXPECT_EQ(hnd, nullptr);
}

TEST_F(TestQtraceInit, qmchannel_init_error)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(-1));
	EXPECT_CALL(mock_alloc, qfree_internal).WillOnce(Return());
	qmhandle hnd = qmu_qtrace_init(mock_cb.cb, &mock_cb);
	EXPECT_EQ(hnd, nullptr);
}

/**********************************************************************/

using TestQtraceStart = TestQtrace;

TEST_F(TestQtraceStart, success)
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

	EXPECT_CALL(mock_pthread, pthread_create).WillOnce(Return(0));
	int ret = qmu_qtrace_start(hnd);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestQtraceStart, invalid_channel_type)
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

	/* Expect -EINVAL error as handle type is not valid (log instead of qtrace) */
	int ret = qmu_qtrace_start(hnd);
	EXPECT_EQ(ret, -EINVAL);
}

/**********************************************************************/

using TestQtraceDestroy = TestQtrace;

TEST_F(TestQtraceDestroy, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init)
		.WillOnce(Invoke(qmchannel_init));
	EXPECT_CALL(mock_io, open).WillOnce(Return(0));
	EXPECT_CALL(mock_io, ioctl)
		.WillOnce(SetIoctlArgAndReturn(QMCHANNEL_TYPE_QTRACE, 0));
	qmhandle hnd = qmu_qtrace_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_qmchannel, qmchannel_close).WillOnce(Return(0));
	EXPECT_CALL(mock_alloc, qfree_internal).WillOnce(Return());
	int ret = qmu_qtrace_destroy(hnd);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestQtraceDestroy, invalid_channel_type)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init)
		.WillOnce(Invoke(qmchannel_init));
	EXPECT_CALL(mock_io, open).WillOnce(Return(0));
	EXPECT_CALL(mock_io, ioctl)
		.WillOnce(SetIoctlArgAndReturn(QMCHANNEL_TYPE_LOG, 0));
	/* Use log_init to get a wrong handle type */
	qmhandle hnd = qmu_log_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	/* Call qmu_qtrace_start with a wrong handle */
	int ret = qmu_qtrace_destroy(hnd);
	/* Expect -EINVAL error as handle type is not valid */
	EXPECT_EQ(ret, -EINVAL);
}

/**********************************************************************/

class TestQtraceHandleData : public TestQtrace {
    protected:
	char *channel_buf;

	void SetUp() override
	{
		TestQtrace::SetUp();
		channel_buf = new char[BUF_SIZE]();
	}

	void TearDown() override
	{
		TestQtrace::TearDown();
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
 * This test uses qmu_qtrace_init() to create a qtrace channel, then calls
 * qmchannel_read() to simulate the reception of messages on the channel.
 */
TEST_F(TestQtraceHandleData, success)
{
	MockChannelCallback mock_cb;

	/* First, use qmu_qtrace_init() to create a qtrace channel. */
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

	/* Then, call qmchannel_read() to simulate the reception
	 * of a qtrace buffer */
	uint8_t qtrace_val_buf[] = {
		0x5A, 0x00, 0x00, 0xFF, 0x00, 0x51, 0x54, 0x52, 0x41, 0x43,
		0x45, 0x2D, 0x30, 0x2E, 0x31, 0x2D, 0x4C, 0x45, 0x2D, 0x2D,
		0x2D, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58,
		0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x00, 0xFE, 0x00,
		0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00, 0x10, 0x03,
		0x75, 0x63, 0x69, 0x00, 0x01, 0x03, 0x6D, 0x61, 0x63, 0x00,
		0x02, 0x03, 0x6D, 0x63, 0x70, 0x73, 0x38, 0x30, 0x32, 0x31,
		0x35, 0x34, 0x00, 0x20, 0x04, 0x6C, 0x6C, 0x68, 0x77, 0x00,
		0x00, 0x05, 0x64, 0x65, 0x66, 0x61, 0x75, 0x6C, 0x74, 0x00
	};

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(qtrace_val_buf,
					   sizeof(qtrace_val_buf)));
	EXPECT_CALL(mock_alloc, qmalloc_internal(_))
		.WillOnce(Return(channel_buf));
	EXPECT_CALL(mock_cb, callback).WillOnce(Return(0));

	int ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, 0);
}

/**********************************************************************/

using TestQtraceSetLevel = TestQtrace;

TEST_F(TestQtraceSetLevel, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init)
		.WillOnce(Invoke(qmchannel_init));
	EXPECT_CALL(mock_io, open).WillOnce(Return(0));
	EXPECT_CALL(mock_io, ioctl)
		.WillOnce(SetIoctlArgAndReturn(QMCHANNEL_TYPE_QTRACE, 0));
	qmhandle hnd = qmu_qtrace_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_io, write).WillOnce(Return(3));
	int ret = qmu_qtrace_set_level(hnd, 0, 3);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestQtraceSetLevel, invalid_channel_type)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init)
		.WillOnce(Invoke(qmchannel_init));
	EXPECT_CALL(mock_io, open).WillOnce(Return(0));
	EXPECT_CALL(mock_io, ioctl)
		.WillOnce(SetIoctlArgAndReturn(QMCHANNEL_TYPE_LOG, 0));
	/* Use log_init to get a wrong handle type */
	qmhandle hnd = qmu_log_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	/* Call qmu_qtrace_set_level with a wrong handle */
	int ret = qmu_qtrace_set_level(hnd, 0, 3);
	/* Expect -EINVAL error as handle type is not valid */
	EXPECT_EQ(ret, -EINVAL);
}
