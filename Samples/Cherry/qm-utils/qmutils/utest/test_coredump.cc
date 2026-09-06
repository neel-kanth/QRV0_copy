/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "test_coredump.hh"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_channel_callback.hh"

#include <cstddef>
#include <cstring>

#define BUF_SIZE 4096

using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::_;

MATCHER_P2(ElementsAreVoidArray, a, n, "")
{
	for (int i = 0; i < n; i++) {
		if (((char *)arg)[i] != a[i]) {
			return false;
		}
	}
	return true;
}

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

void TestCoredump::SetUp()
{
	handle_buf = new char[BUF_SIZE]();
}

void TestCoredump::TearDown()
{
	if (!mock_alloc.pointers.empty()) {
		// MockQmalloc need all allocated pointers are freed.
		EXPECT_CALL(mock_alloc, qfree_internal(handle_buf)).Times(1);
		qfree(Q2M(handle_buf));
	}
	delete[] handle_buf;
}

/**********************************************************************/

using TestCoredumpInit = TestCoredump;

TEST_F(TestCoredumpInit, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_coredump_init(mock_cb.cb, mock_cb.end_cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);
}

TEST_F(TestCoredumpInit, read_cb_error)
{
	MockChannelCallback mock_cb;

	qmhandle hnd = qmu_coredump_init(NULL, mock_cb.end_cb, &mock_cb);
	EXPECT_EQ(hnd, nullptr);
}

TEST_F(TestCoredumpInit, end_cb_error)
{
	MockChannelCallback mock_cb;

	qmhandle hnd = qmu_coredump_init(mock_cb.cb, NULL, &mock_cb);
	EXPECT_EQ(hnd, nullptr);
}

TEST_F(TestCoredumpInit, qmchannel_init_error)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(-1));
	EXPECT_CALL(mock_alloc, qfree_internal(handle_buf)).WillOnce(Return());
	qmhandle hnd = qmu_coredump_init(mock_cb.cb, mock_cb.end_cb, &mock_cb);
	EXPECT_EQ(hnd, nullptr);
}

/**********************************************************************/

class TestCoredumpSetbuf : public TestCoredump {
    protected:
	char *buf;

	void SetUp() override
	{
		TestCoredump::SetUp();
		buf = new char[BUF_SIZE]();
	}

	void TearDown() override
	{
		TestCoredump::TearDown();
		if (!mock_alloc.pointers.empty()) {
			// MockQmalloc need all allocated pointers are freed.
			EXPECT_CALL(mock_alloc, qfree_internal(buf)).Times(1);
			qfree(Q2M(buf));
		}
		delete[] buf;
	}
};

TEST_F(TestCoredumpSetbuf, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_coredump_init(mock_cb.cb, mock_cb.end_cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	int ret = qmu_coredump_setbuf(hnd, buf, BUF_SIZE);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestCoredumpSetbuf, null_buf_success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_coredump_init(mock_cb.cb, mock_cb.end_cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	int ret = qmu_coredump_setbuf(hnd, NULL, BUF_SIZE);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestCoredumpSetbuf, null_hnd_error)
{
	int ret = qmu_coredump_setbuf(nullptr, buf, BUF_SIZE);
	EXPECT_EQ(ret, -EINVAL);
}

TEST_F(TestCoredumpSetbuf, zero_len_buf_error)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_coredump_init(mock_cb.cb, mock_cb.end_cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	int ret = qmu_coredump_setbuf(hnd, buf, 0);
	EXPECT_EQ(ret, -EINVAL);
}

TEST_F(TestCoredumpSetbuf, size_error)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_coredump_init(mock_cb.cb, mock_cb.end_cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	int ret = qmu_coredump_setbuf(hnd, NULL, 0);
	EXPECT_EQ(ret, 0);
	ret = qmu_coredump_setbuf(hnd, NULL, 1);
	EXPECT_EQ(ret, -EMSGSIZE);
	ret = qmu_coredump_setbuf(hnd, NULL, COREDUMP_HSSPI_BLOCK_LENGTH - 1);
	EXPECT_EQ(ret, -EMSGSIZE);
	ret = qmu_coredump_setbuf(hnd, NULL, COREDUMP_HSSPI_BLOCK_LENGTH);
	EXPECT_EQ(ret, 0);
}

/**********************************************************************/

using TestCoredumpStart = TestCoredump;

TEST_F(TestCoredumpStart, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_coredump_init(mock_cb.cb, mock_cb.end_cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_pthread, pthread_create).WillOnce(Return(0));
	int ret = qmu_coredump_start(hnd);
	EXPECT_EQ(ret, 0);
}

/**********************************************************************/

using TestCoredumpDestroy = TestCoredump;

TEST_F(TestCoredumpDestroy, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_coredump_init(mock_cb.cb, mock_cb.end_cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_qmchannel, qmchannel_close).WillOnce(Return(0));
	EXPECT_CALL(mock_alloc, qfree_internal).WillOnce(Return());
	int ret = qmu_coredump_destroy(hnd);
	EXPECT_EQ(ret, 0);
}

/**********************************************************************/

using TestCoredumpForce = TestCoredump;

TEST_F(TestCoredumpForce, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_coredump_init(mock_cb.cb, mock_cb.end_cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	char COREDUMP_FORCE_CMD[] = { 3 };
	EXPECT_CALL(mock_qmchannel,
		    qmchannel_write(hnd,
				    ElementsAreVoidArray(COREDUMP_FORCE_CMD, 1),
				    1))
		.WillOnce(Return(1));
	int ret = qmu_coredump_force(hnd);
	EXPECT_EQ(ret, 0);
}

/**********************************************************************/

class TestCoredumpHandleData : public TestCoredump {
    protected:
	MockChannelCallback mock_cb;
	char *channel_buf;
	char *coredump_buf;
	qmhandle hnd;

	void SetUp() override
	{
		TestCoredump::SetUp();
		channel_buf = new char[BUF_SIZE]();
		coredump_buf = new char[BUF_SIZE]();

		/* Use qmu_coredump_init() to create a coredump channel. */
		EXPECT_CALL(mock_alloc, qmalloc_internal)
			.WillOnce(Return(handle_buf));
		/* Do not mock qmchannel_init(), but instead mock the system
		 * calls it contains, so the callback is registered. */
		EXPECT_CALL(mock_qmchannel, qmchannel_init)
			.WillOnce(Invoke(qmchannel_init));
		EXPECT_CALL(mock_io, open).WillOnce(Return(42));
		EXPECT_CALL(mock_io, ioctl)
			.WillOnce(SetIoctlArgAndReturn(QMCHANNEL_TYPE_COREDUMP,
						       0));
		hnd = qmu_coredump_init(mock_cb.cb, mock_cb.end_cb, &mock_cb);
		EXPECT_NE(hnd, nullptr);
	}

	void TearDown() override
	{
		TestCoredump::TearDown();
		if (!mock_alloc.pointers.empty()) {
			// MockQmalloc need all allocated pointers are freed.
			EXPECT_CALL(mock_alloc, qfree_internal(coredump_buf))
				.Times(1);
			qfree(Q2M(coredump_buf));
		}
		delete[] coredump_buf;
		delete[] channel_buf;
	}
};

TEST_F(TestCoredumpHandleData, bad_message_error)
{
	/* Call qmchannel_read() to simulate the reception of a bad message. */
	char msg_buf[] = { -1 };

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(msg_buf, sizeof(msg_buf)));
	EXPECT_CALL(mock_alloc, qmalloc_internal(_))
		.WillOnce(Return(channel_buf));
	EXPECT_CALL(mock_alloc, qfree_internal(channel_buf)).WillOnce(Return());
	int ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, -EBADMSG);
}

TEST_F(TestCoredumpHandleData, staticbuf_bad_message_error)
{
	/* Same as above, but using qmchannel_setbuf() to avoid dynamic memory
	 * allocation. */
	int ret = qmchannel_setbuf(hnd, channel_buf, BUF_SIZE);
	EXPECT_EQ(ret, 0);

	/* Call qmchannel_read() to simulate the reception of a bad message. */
	char msg_buf[] = { -1 };

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(msg_buf, sizeof(msg_buf)));
	ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, -EBADMSG);
}

/**********************************************************************/

using TestCoredumpHandleHeader = TestCoredumpHandleData;

TEST_F(TestCoredumpHandleHeader, success)
{
	/* Call qmchannel_read() to simulate the reception of the coredump
	 * header. */
	char header_buf[] = { 0, 4, 0, 0, 0, 0, 0 };

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(header_buf, sizeof(header_buf)));
	EXPECT_CALL(mock_alloc, qmalloc_internal)
		.WillOnce(Return(channel_buf))
		.WillOnce(Return(coredump_buf));
	EXPECT_CALL(mock_alloc, qfree_internal(channel_buf)).WillOnce(Return());
	int ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestCoredumpHandleHeader, staticbuf_success)
{
	/* Same as above, but using qmchannel_setbuf() and qmu_coredump_setbuf() to
	 * avoid dynamic memory allocation. */
	int ret = qmchannel_setbuf(hnd, channel_buf, BUF_SIZE);
	EXPECT_EQ(ret, 0);
	ret = qmu_coredump_setbuf(hnd, coredump_buf, BUF_SIZE);
	EXPECT_EQ(ret, 0);

	/* Call qmchannel_read() to simulate the reception of the coredump
	 * header. */
	char header_buf[] = { 0, 4, 0, 0, 0, 0, 0 };

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(header_buf, sizeof(header_buf)));
	ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestCoredumpHandleHeader, prepare_buf_error)
{
	/* Call qmchannel_setbuf() to avoid dynamic memory allocation. */
	int ret = qmchannel_setbuf(hnd, channel_buf, BUF_SIZE);
	EXPECT_EQ(ret, 0);
	/* Call qmu_coredump_setbuf() to force dynamic buffer size. */
	ret = qmu_coredump_setbuf(hnd, NULL, 137);
	EXPECT_EQ(ret, 0);

	/* Call qmchannel_read() to simulate the reception of the
	 * coredump header. */
	char header_buf[] = { 0, 4, 0, 0, 0, 0, 0 };

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(header_buf, sizeof(header_buf)));
	/* Have malloc() return NULL to simulate an error. */
	EXPECT_CALL(mock_alloc, qmalloc_internal(137 + 16))
		.WillOnce(ReturnNull());
	/* End callback will be called, and acknowledgement will be sent. */
	EXPECT_CALL(mock_cb, end_callback(false)).WillOnce(Return(0));
	char COREDUMP_RCV_STATUS[] = { 2, 0 };
	EXPECT_CALL(mock_qmchannel,
		    qmchannel_write(
			    hnd, ElementsAreVoidArray(COREDUMP_RCV_STATUS, 2),
			    2))
		.WillOnce(Return(2));
	ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, -ENOMEM);

	/* Next, do the same test again, but this time with end_cb() call in
	 * qmu_coredump_send_status() returning an error. */
	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(header_buf, sizeof(header_buf)));
	/* Have malloc() return NULL to simulate an error. */
	EXPECT_CALL(mock_alloc, qmalloc_internal(137 + 16))
		.WillOnce(ReturnNull());
	/* End callback will be called, and acknowledgement will be sent. */
	EXPECT_CALL(mock_cb, end_callback(false)).WillOnce(Return(-EFAULT));
	EXPECT_CALL(mock_qmchannel,
		    qmchannel_write(
			    hnd, ElementsAreVoidArray(COREDUMP_RCV_STATUS, 2),
			    2))
		.WillOnce(Return(2));
	ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, -ENOMEM);

	/* Finally, do the same test again, but this time with qmchannel_write()
	 * call in qmu_coredump_send_status() returning an error. */
	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(header_buf, sizeof(header_buf)));
	/* Have malloc() return NULL to simulate an error. */
	EXPECT_CALL(mock_alloc, qmalloc_internal(137 + 16))
		.WillOnce(ReturnNull());
	/* End callback will be called, and acknowledgement will be sent. */
	EXPECT_CALL(mock_cb, end_callback(false)).WillOnce(Return(0));
	EXPECT_CALL(mock_qmchannel,
		    qmchannel_write(
			    hnd, ElementsAreVoidArray(COREDUMP_RCV_STATUS, 2),
			    2))
		.WillOnce(Return(-EFAULT));
	ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, -ENOMEM);
}

/**********************************************************************/

using TestCoredumpHandleBody = TestCoredumpHandleData;

TEST_F(TestCoredumpHandleBody, success)
{
	/* Call qmchannel_setbuf() to avoid dynamic memory allocation. */
	int ret = qmchannel_setbuf(hnd, channel_buf, BUF_SIZE);
	EXPECT_EQ(ret, 0);

	/* Then, call qmchannel_read() to simulate the reception of the coredump
	 * header. */
	char header_buf[] = { 0, 4, 0, 0, 0, 0, 0 };

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(header_buf, sizeof(header_buf)));
	EXPECT_CALL(mock_alloc, qmalloc_internal(4 + 16))
		.WillOnce(Return(coredump_buf));
	ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, 0);

	/* Call qmchannel_read() to simulate the reception of a coredump body of
	 * length 4. */
	char data_buf[5] = { 1, 0, 0, 0, 0 };

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(data_buf, sizeof(data_buf)));
	/* Callbacks will be called with data and status, and acknowledgement
	 * will be sent. */
	EXPECT_CALL(mock_cb, callback(coredump_buf + 16, 4)).WillOnce(Return(0));
	EXPECT_CALL(mock_cb, end_callback(true)).WillOnce(Return(0));
	char COREDUMP_RCV_STATUS[] = { 2, 1 };
	EXPECT_CALL(mock_qmchannel,
		    qmchannel_write(
			    hnd, ElementsAreVoidArray(COREDUMP_RCV_STATUS, 2),
			    2))
		.WillOnce(Return(2));
	ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestCoredumpHandleBody, partial_coredump_success)
{
	/* Call qmchannel_setbuf() to avoid dynamic memory allocation. */
	int ret = qmchannel_setbuf(hnd, channel_buf, BUF_SIZE);
	EXPECT_EQ(ret, 0);
	/* Call qmu_coredump_setbuf() to force dynamic buffer size. */
	ret = qmu_coredump_setbuf(hnd, NULL, 128);
	EXPECT_EQ(ret, 0);

	/* Then, call qmchannel_read() to simulate the reception of the coredump
	 * header. */
	unsigned char header_buf[] = { 0, 130, 0, 0, 0, 0, 0 };

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(header_buf, sizeof(header_buf)));
	EXPECT_CALL(mock_alloc, qmalloc_internal(128 + 16))
		.WillOnce(Return(coredump_buf));
	ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, 0);

	/* Call qmchannel_read() to simulate the reception of a coredump body of
	 * length 128. */
	char data_buf[129] = { 1 };

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(data_buf, sizeof(data_buf)));
	EXPECT_CALL(mock_alloc, qmalloc_internal(128 + 16))
		.WillOnce(Return(coredump_buf));
	/* Data callback will be called. */
	EXPECT_CALL(mock_cb, callback(coredump_buf + 16, 128))
		.WillOnce(Return(0));
	ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestCoredumpHandleBody, staticbuf_success)
{
	/* Call qmchannel_setbuf() and qmu_coredump_setbuf() to avoid dynamic memory
	 * allocation. */
	int ret = qmchannel_setbuf(hnd, channel_buf, BUF_SIZE);
	EXPECT_EQ(ret, 0);
	ret = qmu_coredump_setbuf(hnd, coredump_buf, BUF_SIZE);
	EXPECT_EQ(ret, 0);

	/* Then, call qmchannel_read() to simulate the reception of the coredump
	 * header. */
	char header_buf[] = { 0, 4, 0, 0, 0, 0, 0 };

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(header_buf, sizeof(header_buf)));
	ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, 0);

	/* Call qmchannel_read() to simulate the reception of a coredump body of
	 * length 4. */
	char data_buf[5] = { 1, 0, 0, 0, 0 };

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(data_buf, sizeof(data_buf)));
	/* Callbacks will be called with data and status, and acknowledgement
	 * will be sent. */
	EXPECT_CALL(mock_cb, callback(coredump_buf, 4)).WillOnce(Return(0));
	EXPECT_CALL(mock_cb, end_callback(true)).WillOnce(Return(0));
	char COREDUMP_RCV_STATUS[] = { 2, 1 };
	EXPECT_CALL(mock_qmchannel,
		    qmchannel_write(
			    hnd, ElementsAreVoidArray(COREDUMP_RCV_STATUS, 2),
			    2))
		.WillOnce(Return(2));
	ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestCoredumpHandleBody, length_error)
{
	/* Call qmchannel_setbuf() to avoid dynamic memory allocation. */
	int ret = qmchannel_setbuf(hnd, channel_buf, BUF_SIZE);
	EXPECT_EQ(ret, 0);

	/* Then, call qmchannel_read() to simulate the reception of the coredump
	 * header. */
	char header_buf[] = { 0, 4, 0, 0, 0, 0, 0 };

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(header_buf, sizeof(header_buf)));
	EXPECT_CALL(mock_alloc, qmalloc_internal(4 + 16))
		.WillOnce(Return(coredump_buf));
	ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, 0);

	/* Call qmchannel_read() to simulate the reception of a coredump body of
	 * length 8. */
	char data_buf[9] = { 1, 0, 0, 0, 0 };

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(data_buf, sizeof(data_buf)));
	/* Check that the core dump buffer is freed on length error. */
	EXPECT_CALL(mock_alloc, qfree_internal(coredump_buf)).WillOnce(Return());
	/* End callback will be called, and acknowledgement will be sent. */
	EXPECT_CALL(mock_cb, end_callback(false)).WillOnce(Return(-EFAULT));
	char COREDUMP_RCV_STATUS[] = { 2, 0 };
	EXPECT_CALL(mock_qmchannel,
		    qmchannel_write(
			    hnd, ElementsAreVoidArray(COREDUMP_RCV_STATUS, 2),
			    2))
		.WillOnce(Return(2));
	ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, -EMSGSIZE);

	/* Next, call qmu_coredump_setbuf() to avoid dynamic memory allocation, and
	 * do the same test again. */
	ret = qmu_coredump_setbuf(hnd, coredump_buf, BUF_SIZE);
	EXPECT_EQ(ret, 0);

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(data_buf, sizeof(data_buf)));
	/* Don't expect free() to be called this time. */
	/* End callback will be called, and acknowledgement will be sent. */
	EXPECT_CALL(mock_cb, end_callback(false)).WillOnce(Return(-EFAULT));
	EXPECT_CALL(mock_qmchannel,
		    qmchannel_write(
			    hnd, ElementsAreVoidArray(COREDUMP_RCV_STATUS, 2),
			    2))
		.WillOnce(Return(2));
	ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, -EMSGSIZE);
}

TEST_F(TestCoredumpHandleBody, app_cb_error)
{
	/* Call qmchannel_setbuf() to avoid dynamic memory allocation. */
	int ret = qmchannel_setbuf(hnd, channel_buf, BUF_SIZE);
	EXPECT_EQ(ret, 0);

	/* Then, call qmchannel_read() to simulate the reception of the coredump
	 * header. */
	char header_buf[] = { 0, 4, 0, 0, 0, 0, 0 };

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(header_buf, sizeof(header_buf)));
	EXPECT_CALL(mock_alloc, qmalloc_internal(4 + 16))
		.WillOnce(Return(coredump_buf));
	ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, 0);

	/* Call qmchannel_read() to simulate the reception of a coredump body of
	 * length 4. */
	char data_buf[5] = { 1, 0, 0, 0, 0 };

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(data_buf, sizeof(data_buf)));
	/* Data callback will be called and return an error. */
	EXPECT_CALL(mock_cb, callback(Q2M(coredump_buf), 4))
		.WillOnce(Return(-EFAULT));
	ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, -EFAULT);
}

TEST_F(TestCoredumpHandleBody, partial_coredump_malloc_error)
{
	/* Call qmchannel_setbuf() to avoid dynamic memory allocation. */
	int ret = qmchannel_setbuf(hnd, channel_buf, BUF_SIZE);
	EXPECT_EQ(ret, 0);
	/* Call qmu_coredump_setbuf() to force dynamic buffer size. */
	ret = qmu_coredump_setbuf(hnd, NULL, 128);
	EXPECT_EQ(ret, 0);

	/* Then, call qmchannel_read() to simulate the reception of the coredump
	 * header. */
	unsigned char header_buf[] = { 0, 130, 0, 0, 0, 0, 0 };

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(header_buf, sizeof(header_buf)));
	EXPECT_CALL(mock_alloc, qmalloc_internal(128 + 16))
		.WillOnce(Return(coredump_buf));
	ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, 0);

	/* Call qmchannel_read() to simulate the reception of a coredump body of
	 * length 128. */
	char data_buf[129] = { 1 };

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(data_buf, sizeof(data_buf)));
	/* Have malloc() return NULL to simulate an error. */
	EXPECT_CALL(mock_alloc, qmalloc_internal(128 + 16))
		.WillOnce(ReturnNull());
	/* Data callback will be called. */
	EXPECT_CALL(mock_cb, callback(Q2M(coredump_buf), 128))
		.WillOnce(Return(0));
	ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, -ENOMEM);
}

TEST_F(TestCoredumpHandleBody, checksum_error)
{
	/* Call qmchannel_setbuf() to avoid dynamic memory allocation. */
	int ret = qmchannel_setbuf(hnd, channel_buf, BUF_SIZE);
	EXPECT_EQ(ret, 0);
	/* Call qmu_coredump_setbuf() to force dynamic buffer size. */
	ret = qmu_coredump_setbuf(hnd, NULL, 256);
	EXPECT_EQ(ret, 0);

	/* Then, call qmchannel_read() to simulate the reception of the coredump
	 * header. */
	char header_buf[] = { 0, 4, 0, 0, 0, 0, 0 };

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(header_buf, sizeof(header_buf)));
	EXPECT_CALL(mock_alloc, qmalloc_internal(256 + 16))
		.WillOnce(Return(coredump_buf));
	ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, 0);

	/* Call qmchannel_read() to simulate the reception of a coredump body of
	 * length 4, with data that do not match the header checksum. */
	char data_buf[5] = { 1, 1, 1, 1, 1 };

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(data_buf, sizeof(data_buf)));
	/* Coredump buffer will be freed. */
	EXPECT_CALL(mock_alloc, qfree_internal(coredump_buf)).WillOnce(Return());
	/* End callback will be called, and acknowledgement will be sent. */
	EXPECT_CALL(mock_cb, end_callback(false)).WillOnce(Return(0));
	char COREDUMP_RCV_STATUS[] = { 2, 0 };
	EXPECT_CALL(mock_qmchannel,
		    qmchannel_write(
			    hnd, ElementsAreVoidArray(COREDUMP_RCV_STATUS, 2),
			    2))
		.WillOnce(Return(2));
	ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestCoredumpHandleBody, staticbuf_checksum_error)
{
	/* Call qmchannel_setbuf() and qmu_coredump_setbuf() to avoid dynamic memory
	 * allocation. */
	int ret = qmchannel_setbuf(hnd, channel_buf, BUF_SIZE);
	EXPECT_EQ(ret, 0);
	ret = qmu_coredump_setbuf(hnd, coredump_buf, BUF_SIZE);
	EXPECT_EQ(ret, 0);

	/* Then, call qmchannel_read() to simulate the reception of the coredump
	 * header. */
	char header_buf[] = { 0, 4, 0, 0, 0, 0, 0 };

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(header_buf, sizeof(header_buf)));
	ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, 0);

	/* Call qmchannel_read() to simulate the reception of a coredump body of
	 * length 4, with data that do not match the header checksum. */
	char data_buf[5] = { 1, 1, 1, 1, 1 };

	EXPECT_CALL(mock_io, read)
		.WillOnce(CopyBufAndReturn(data_buf, sizeof(data_buf)));
	/* End callback will be called, and acknowledgement will be sent. */
	EXPECT_CALL(mock_cb, end_callback(false)).WillOnce(Return(0));
	char COREDUMP_RCV_STATUS[] = { 2, 0 };
	EXPECT_CALL(mock_qmchannel,
		    qmchannel_write(
			    hnd, ElementsAreVoidArray(COREDUMP_RCV_STATUS, 2),
			    2))
		.WillOnce(Return(2));
	ret = qmchannel_read(hnd);
	EXPECT_EQ(ret, 0);
}
