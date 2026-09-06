/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "test_multichan.hh"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_channel_callback.hh"
#include "mock_io.hh"
#include "mock_qmalloc.hh"

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ios>

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::SetErrnoAndReturn;

ACTION_P2(SetIoctlArgAndReturn, a, r)
{
	*(unsigned int *)arg2 = a;
	return r;
}

extern "C" {
#include "qmchannel_multichan.h"
#include "qmutils/qmchannel.h"
}

void TestMultichan::SetUp()
{
}

void TestMultichan::TearDown()
{
}

/**********************************************************************/

using TestMultichanInit = TestMultichan;

TEST_F(TestMultichanInit, success)
{
	MockChannelCallback mock_cb;
	struct qmchannel chan = {};

	EXPECT_CALL(mock_io, open).WillOnce(Return(0));
	EXPECT_CALL(mock_io, ioctl)
		.WillOnce(SetIoctlArgAndReturn(QMCHANNEL_TYPE_UCI, 0))
		.WillOnce(Return(0));
	EXPECT_CALL(mock_io, close).Times(0);
	EXPECT_CALL(mock_cb, callback).Times(0);
	int ret = qmchannel_init(&chan, QMCHANNEL_TYPE_COREDUMP, mock_cb.cb,
				 &mock_cb);
	EXPECT_EQ(ret, 0);
	EXPECT_EQ(chan.type, QMCHANNEL_TYPE_COREDUMP);
	EXPECT_EQ(chan.cb, mock_cb.cb);
	EXPECT_EQ(chan.cb_data, &mock_cb);
}

TEST_F(TestMultichanInit, same_type_success)
{
	MockChannelCallback mock_cb;
	struct qmchannel chan = {};

	EXPECT_CALL(mock_io, open).WillOnce(Return(0));
	EXPECT_CALL(mock_io, ioctl)
		.WillOnce(SetIoctlArgAndReturn(QMCHANNEL_TYPE_UCI, 0));
	EXPECT_CALL(mock_io, close).Times(0);
	EXPECT_CALL(mock_cb, callback).Times(0);
	int ret =
		qmchannel_init(&chan, QMCHANNEL_TYPE_UCI, mock_cb.cb, &mock_cb);
	EXPECT_EQ(ret, 0);
	EXPECT_EQ(chan.type, QMCHANNEL_TYPE_UCI);
	EXPECT_EQ(chan.cb, mock_cb.cb);
	EXPECT_EQ(chan.cb_data, &mock_cb);
}

TEST_F(TestMultichanInit, args_error)
{
	MockChannelCallback mock_cb;
	struct qmchannel chan = {};

	int ret = qmchannel_init(NULL, QMCHANNEL_TYPE_COREDUMP, mock_cb.cb,
				 &mock_cb);
	EXPECT_EQ(ret, -EINVAL);

	ret = qmchannel_init(&chan, QMCHANNEL_TYPE_COREDUMP, NULL, NULL);
	EXPECT_EQ(ret, -EINVAL);
}

TEST_F(TestMultichanInit, open_error)
{
	MockChannelCallback mock_cb;
	struct qmchannel chan = {};

	EXPECT_CALL(mock_io, open).WillOnce(SetErrnoAndReturn(42, -1));
	EXPECT_CALL(mock_io, ioctl).Times(0);
	EXPECT_CALL(mock_io, close).Times(0);
	EXPECT_CALL(mock_cb, callback).Times(0);
	int ret = qmchannel_init(&chan, QMCHANNEL_TYPE_COREDUMP, mock_cb.cb,
				 &mock_cb);
	EXPECT_EQ(ret, -42);
	EXPECT_EQ(chan.type, 0);
	EXPECT_EQ(chan.cb, nullptr);
	EXPECT_EQ(chan.cb_data, nullptr);
}

TEST_F(TestMultichanInit, get_type_ioctl_error)
{
	MockChannelCallback mock_cb;
	struct qmchannel chan = {};

	EXPECT_CALL(mock_io, open).WillOnce(Return(0));
	EXPECT_CALL(mock_io, ioctl).WillOnce(SetErrnoAndReturn(42, -1));
	EXPECT_CALL(mock_io, close);
	EXPECT_CALL(mock_cb, callback).Times(0);
	int ret = qmchannel_init(&chan, QMCHANNEL_TYPE_COREDUMP, mock_cb.cb,
				 &mock_cb);
	EXPECT_EQ(ret, -42);
	EXPECT_EQ(chan.type, 0);
	EXPECT_EQ(chan.cb, nullptr);
	EXPECT_EQ(chan.cb_data, nullptr);
}

TEST_F(TestMultichanInit, set_type_ioctl_error)
{
	MockChannelCallback mock_cb;
	struct qmchannel chan = {};

	EXPECT_CALL(mock_io, open).WillOnce(Return(0));
	EXPECT_CALL(mock_io, ioctl)
		.WillOnce(SetIoctlArgAndReturn(QMCHANNEL_TYPE_UCI, 0))
		.WillOnce(SetErrnoAndReturn(42, -1));
	EXPECT_CALL(mock_io, close);
	EXPECT_CALL(mock_cb, callback).Times(0);
	int ret = qmchannel_init(&chan, QMCHANNEL_TYPE_COREDUMP, mock_cb.cb,
				 &mock_cb);
	EXPECT_EQ(ret, -42);
	EXPECT_EQ(chan.type, 0);
	EXPECT_EQ(chan.cb, nullptr);
	EXPECT_EQ(chan.cb_data, nullptr);
}

/**********************************************************************/

using TestMultichanClose = TestMultichan;

TEST_F(TestMultichanClose, success)
{
	struct qmchannel chan;

	EXPECT_CALL(mock_io, close).WillOnce(Return(0));
	int ret = qmchannel_close(&chan);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestMultichanClose, args_error)
{
	int ret = qmchannel_close(NULL);
	EXPECT_EQ(ret, -EINVAL);
}

TEST_F(TestMultichanClose, error)
{
	struct qmchannel chan;

	EXPECT_CALL(mock_io, close).WillOnce(SetErrnoAndReturn(42, -1));
	int ret = qmchannel_close(&chan);
	EXPECT_EQ(ret, -42);
}

/**********************************************************************/

using TestMultichanGetfd = TestMultichan;

TEST_F(TestMultichanGetfd, success)
{
	struct qmchannel chan = {};

	chan.fd = 42;

	int ret = qmchannel_getfd(&chan);
	EXPECT_EQ(ret, 42);
}

TEST_F(TestMultichanGetfd, args_error)
{
	int ret = qmchannel_getfd(NULL);
	EXPECT_EQ(ret, -EINVAL);
}

/**********************************************************************/

using TestMultichanGetpollevent = TestMultichan;

TEST_F(TestMultichanGetpollevent, success)
{
	struct qmchannel chan;

	int ret = qmchannel_getpollevent(&chan);
	EXPECT_EQ(ret, POLL_IN);
}

/**********************************************************************/

using TestMultichanSetbuf = TestMultichan;

TEST_F(TestMultichanSetbuf, success)
{
	char buf = 'a';
	struct qmchannel chan = {};

	int ret = qmchannel_setbuf(&chan, &buf, 512);
	EXPECT_EQ(ret, 0);
	EXPECT_EQ(chan.buffer, &buf);
	EXPECT_EQ(chan.buffer_size, 512);

	ret = qmchannel_setbuf(&chan, NULL, 768);
	EXPECT_EQ(ret, 0);
	EXPECT_EQ(chan.buffer, nullptr);
	EXPECT_EQ(chan.buffer_size, 768);

	chan.type = QMCHANNEL_TYPE_COREDUMP;
	ret = qmchannel_setbuf(&chan, NULL, 0);
	EXPECT_EQ(ret, 0);
	EXPECT_EQ(chan.buffer, nullptr);
	EXPECT_EQ(chan.buffer_size, 0);

	chan.type = QMCHANNEL_TYPE_LOG;
	ret = qmchannel_setbuf(&chan, NULL, 0);
	EXPECT_EQ(ret, 0);
	EXPECT_EQ(chan.buffer, nullptr);
	EXPECT_EQ(chan.buffer_size, 0);

	chan.type = QMCHANNEL_TYPE_QTRACE;
	ret = qmchannel_setbuf(&chan, NULL, 0);
	EXPECT_EQ(ret, 0);
	EXPECT_EQ(chan.buffer, nullptr);
	EXPECT_EQ(chan.buffer_size, 0);

	chan.type = QMCHANNEL_TYPE_UCI;
	ret = qmchannel_setbuf(&chan, NULL, 0);
	EXPECT_EQ(ret, 0);
	EXPECT_EQ(chan.buffer, nullptr);
	EXPECT_EQ(chan.buffer_size, 0);
}

TEST_F(TestMultichanSetbuf, args_error)
{
	char buf = 'a';
	struct qmchannel chan = {};

	int ret = qmchannel_setbuf(NULL, &buf, 512);
	EXPECT_EQ(ret, -EINVAL);

	ret = qmchannel_setbuf(&chan, &buf, 0);
	EXPECT_EQ(ret, -EINVAL);
}

TEST_F(TestMultichanSetbuf, size_error)
{
	struct qmchannel chan = {};

	chan.type = QMCHANNEL_TYPE_COREDUMP;
	int ret = qmchannel_setbuf(&chan, NULL, COREDUMP_MAX_PACKET_SIZE - 1);
	EXPECT_EQ(ret, -EMSGSIZE);
	ret = qmchannel_setbuf(&chan, NULL, COREDUMP_MAX_PACKET_SIZE);
	EXPECT_EQ(ret, 0);

	chan.type = QMCHANNEL_TYPE_LOG;
	ret = qmchannel_setbuf(&chan, NULL, LOG_MAX_PACKET_SIZE - 1);
	EXPECT_EQ(ret, -EMSGSIZE);
	ret = qmchannel_setbuf(&chan, NULL, LOG_MAX_PACKET_SIZE);
	EXPECT_EQ(ret, 0);

	chan.type = QMCHANNEL_TYPE_QTRACE;
	ret = qmchannel_setbuf(&chan, NULL, QTRACE_MAX_PACKET_SIZE - 1);
	EXPECT_EQ(ret, -EMSGSIZE);
	ret = qmchannel_setbuf(&chan, NULL, QTRACE_MAX_PACKET_SIZE);
	EXPECT_EQ(ret, 0);

	chan.type = QMCHANNEL_TYPE_UCI;
	ret = qmchannel_setbuf(&chan, NULL, UCI_MAX_PACKET_SIZE - 1);
	EXPECT_EQ(ret, -EMSGSIZE);
	ret = qmchannel_setbuf(&chan, NULL, UCI_MAX_PACKET_SIZE);
	EXPECT_EQ(ret, 0);
}

/**********************************************************************/

using TestMultichanRead = TestMultichan;

TEST_F(TestMultichanRead, success)
{
	MockChannelCallback mock_cb;
	struct qmchannel chan = {};
	char buf[64] __attribute__((aligned(4))) = { 'a' };

	chan.cb = mock_cb.cb;
	chan.cb_data = &mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(buf));
	EXPECT_CALL(mock_io, read).WillOnce(Return(42));
	EXPECT_CALL(mock_alloc, qfree_internal(buf)).Times(1);
	EXPECT_CALL(mock_cb, callback(_, 42))
		.WillOnce([](void *data, size_t len) -> int {
			qmchannel_freebuf(data);
			return len;
		});
	int ret = qmchannel_read(&chan);
	EXPECT_EQ(ret, 42);

	/* Run the same test again, but with buffer_size set, this time. */
	chan.buffer_size = 16;
	EXPECT_CALL(mock_alloc, qmalloc_internal(16 + 16)).WillOnce(Return(buf));
	EXPECT_CALL(mock_io, read).WillOnce(Return(16));
	EXPECT_CALL(mock_alloc, qfree_internal(buf)).Times(1);
	EXPECT_CALL(mock_cb, callback(_, 16))
		.WillOnce([](void *data, size_t len) -> int {
			qmchannel_freebuf(data);
			return len;
		});
	ret = qmchannel_read(&chan);
	EXPECT_EQ(ret, 16);
}

TEST_F(TestMultichanRead, staticbuf_success)
{
	MockChannelCallback mock_cb;
	char buf = 'a';
	struct qmchannel chan = {};

	chan.cb = mock_cb.cb;
	chan.cb_data = &mock_cb;
	chan.buffer = &buf;
	chan.buffer_size = 1;

	EXPECT_CALL(mock_alloc, qmalloc_internal).Times(0);
	EXPECT_CALL(mock_io, read).WillOnce(Return(42));
	EXPECT_CALL(mock_alloc, qfree_internal).Times(0);
	EXPECT_CALL(mock_cb, callback(_, 42)).WillOnce(Return(42));
	int ret = qmchannel_read(&chan);
	EXPECT_EQ(ret, 42);
}

TEST_F(TestMultichanRead, args_error)
{
	int ret = qmchannel_read(NULL);
	EXPECT_EQ(ret, -EINVAL);
}

TEST_F(TestMultichanRead, malloc_error)
{
	MockChannelCallback mock_cb;
	struct qmchannel chan = {};

	chan.cb = mock_cb.cb;
	chan.cb_data = &mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(ReturnNull());
	EXPECT_CALL(mock_io, read).Times(0);
	EXPECT_CALL(mock_alloc, qfree_internal).Times(0);
	EXPECT_CALL(mock_cb, callback).Times(0);
	int ret = qmchannel_read(&chan);
	EXPECT_EQ(ret, -ENOMEM);
}

TEST_F(TestMultichanRead, read_error)
{
	MockChannelCallback mock_cb;
	struct qmchannel chan = {};
	char buf[64] __attribute__((aligned(4))) = { 'a' };

	chan.cb = mock_cb.cb;
	chan.cb_data = &mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(buf));
	EXPECT_CALL(mock_io, read).WillOnce(SetErrnoAndReturn(42, -1));
	EXPECT_CALL(mock_alloc, qfree_internal(buf)).WillOnce(Return());
	EXPECT_CALL(mock_cb, callback).Times(0);
	int ret = qmchannel_read(&chan);
	EXPECT_EQ(ret, -42);
}

TEST_F(TestMultichanRead, staticbuf_read_error)
{
	MockChannelCallback mock_cb;
	char buf = 'a';
	struct qmchannel chan = {};

	chan.cb = mock_cb.cb;
	chan.cb_data = &mock_cb;
	chan.buffer = &buf;
	chan.buffer_size = 1;

	EXPECT_CALL(mock_alloc, qmalloc_internal).Times(0);
	EXPECT_CALL(mock_io, read).WillOnce(SetErrnoAndReturn(42, -1));
	EXPECT_CALL(mock_alloc, qfree_internal).Times(0);
	EXPECT_CALL(mock_cb, callback).Times(0);
	int ret = qmchannel_read(&chan);
	EXPECT_EQ(ret, -42);
}

/**********************************************************************/

using TestMultichanWrite = TestMultichan;

TEST_F(TestMultichanWrite, success)
{
	char buf = 'a';
	struct qmchannel chan;

	EXPECT_CALL(mock_io, write).WillOnce(Return(0));
	int ret = qmchannel_write(&chan, &buf, 1);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestMultichanWrite, args_error)
{
	char buf = 'a';

	int ret = qmchannel_write(NULL, &buf, 1);
	EXPECT_EQ(ret, -EINVAL);
}

TEST_F(TestMultichanWrite, error)
{
	char buf = 'a';
	struct qmchannel chan;

	EXPECT_CALL(mock_io, write).WillOnce(SetErrnoAndReturn(42, -1));
	int ret = qmchannel_write(&chan, &buf, 1);
	EXPECT_EQ(ret, -42);
}

/**********************************************************************/

using TestMultichanIoctl = TestMultichan;

TEST_F(TestMultichanIoctl, success)
{
	unsigned int param;
	struct qmchannel chan;

	EXPECT_CALL(mock_io, ioctl).WillOnce(Return(0));
	int ret = qmchannel_ioctl(&chan, 0, (char *)&param);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestMultichanIoctl, args_error)
{
	unsigned int param;

	int ret = qmchannel_ioctl(NULL, 0, (char *)&param);
	EXPECT_EQ(ret, -EINVAL);
}

TEST_F(TestMultichanIoctl, error)
{
	unsigned int param;
	struct qmchannel chan;

	EXPECT_CALL(mock_io, ioctl).WillOnce(SetErrnoAndReturn(42, -1));
	int ret = qmchannel_ioctl(&chan, 0, (char *)&param);
	EXPECT_EQ(ret, -42);
}

/**********************************************************************/

using TestMultichanAllocFreeBuf = TestMultichan;

TEST_F(TestMultichanAllocFreeBuf, success)
{
	char buf[64] __attribute__((aligned(4))) = { 'a' };

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(buf));
	EXPECT_CALL(mock_alloc, qfree_internal(buf)).WillOnce(Return());

	void *ret = qmchannel_allocbuf(42);
	EXPECT_EQ(ret, Q2M(buf)); // qmalloc() add a 16bytes qota struct

	int rc = qmchannel_freebuf(ret);
	EXPECT_EQ(rc, 0);
}
