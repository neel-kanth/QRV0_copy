/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "test_bypass.hh"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_channel_callback.hh"

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <sys/ioctl.h> /* for _IOR/_IOW macros */

using ::testing::_;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::ReturnNull;

extern "C" {
#include "qm35_uci_dev_ioctl.h"
#include "qmchannel_bypass.h"
#include "qmutils/qmchannel.h"
}

void TestBypass::SetUp()
{
	/* Setup embedded qmchannel implementation. */
	qmchannel_setup(&qm35);
}

void TestBypass::TearDown()
{
}

/**********************************************************************/

using TestBypassInit = TestBypass;

TEST_F(TestBypassInit, success)
{
	MockChannelCallback mock_cb;
	struct qmchannel chan = {};

	EXPECT_CALL(mock_bypass, qm3x_bypass_open)
		.WillOnce(Return(&mock_bypass.bpc));
	EXPECT_CALL(mock_bypass,
		    qm3x_bypass_control(
			    &mock_bypass.bpc, QM3X_BYPASS_ACTION_MSG_TYPE,
			    Pointee((long)QM3X_TRANSPORT_MSG_COREDUMP)))
		.WillOnce(Return(QM3X_TRANSPORT_MSG_MAX));
	EXPECT_CALL(mock_bypass, qm3x_bypass_recv).Times(0);
	EXPECT_CALL(mock_bypass, qm3x_bypass_close).Times(0);
	EXPECT_CALL(mock_cb, callback).Times(0);

	int ret = qmchannel_init(&chan, QMCHANNEL_TYPE_COREDUMP, mock_cb.cb,
				 &mock_cb);
	EXPECT_EQ(ret, 0);
	EXPECT_EQ(chan.type, QMCHANNEL_TYPE_COREDUMP);
	EXPECT_EQ(chan.cb, mock_cb.cb);
	EXPECT_EQ(chan.cb_data, &mock_cb);
}

TEST_F(TestBypassInit, setup_error)
{
	MockChannelCallback mock_cb;
	struct qmchannel chan = {};

	/* Remove setup made by constructor. */
	qmchannel_setup(nullptr);

	EXPECT_CALL(mock_bypass, qm3x_bypass_open).Times(0);
	EXPECT_CALL(mock_bypass, qm3x_bypass_control).Times(0);
	EXPECT_CALL(mock_bypass, qm3x_bypass_close).Times(0);
	EXPECT_CALL(mock_cb, callback).Times(0);

	int ret = qmchannel_init(&chan, QMCHANNEL_TYPE_COREDUMP, mock_cb.cb,
				 &mock_cb);
	EXPECT_EQ(ret, -EINVAL);
}

TEST_F(TestBypassInit, args_error)
{
	MockChannelCallback mock_cb;
	struct qmchannel chan = {};

	int ret = qmchannel_init(NULL, QMCHANNEL_TYPE_COREDUMP, mock_cb.cb,
				 &mock_cb);
	EXPECT_EQ(ret, -EINVAL);

	ret = qmchannel_init(&chan, QMCHANNEL_TYPE_COREDUMP, NULL, NULL);
	EXPECT_EQ(ret, -EINVAL);
}

TEST_F(TestBypassInit, open_error)
{
	MockChannelCallback mock_cb;
	struct qmchannel chan = {};

	EXPECT_CALL(mock_bypass, qm3x_bypass_open).WillOnce(Return(nullptr));
	EXPECT_CALL(mock_bypass, qm3x_bypass_control).Times(0);
	EXPECT_CALL(mock_bypass, qm3x_bypass_close).Times(0);
	EXPECT_CALL(mock_cb, callback).Times(0);
	int ret = qmchannel_init(&chan, QMCHANNEL_TYPE_COREDUMP, mock_cb.cb,
				 &mock_cb);
	EXPECT_EQ(ret, -ENOMEM);
	EXPECT_EQ(chan.type, 0);
	EXPECT_EQ(chan.cb, nullptr);
	EXPECT_EQ(chan.cb_data, nullptr);
}

TEST_F(TestBypassInit, set_type_error)
{
	MockChannelCallback mock_cb;
	struct qmchannel chan = {};

	EXPECT_CALL(mock_bypass, qm3x_bypass_open)
		.WillOnce(Return(&mock_bypass.bpc));
	EXPECT_CALL(mock_bypass,
		    qm3x_bypass_control(
			    &mock_bypass.bpc, QM3X_BYPASS_ACTION_MSG_TYPE,
			    Pointee((long)QM3X_TRANSPORT_MSG_COREDUMP)))
		.WillOnce(Return(-EBUSY));
	EXPECT_CALL(mock_bypass, qm3x_bypass_close).WillOnce(Return(0));
	EXPECT_CALL(mock_cb, callback).Times(0);
	int ret = qmchannel_init(&chan, QMCHANNEL_TYPE_COREDUMP, mock_cb.cb,
				 &mock_cb);
	EXPECT_EQ(ret, -EBUSY);
	EXPECT_EQ(chan.type, 0);
	EXPECT_EQ(chan.cb, nullptr);
	EXPECT_EQ(chan.cb_data, nullptr);
}

TEST_F(TestBypassInit, success_with_event)
{
	MockChannelCallback mock_cb;
	int buf[4];

	struct qmchannel chan = {};

	EXPECT_CALL(mock_alloc, qmalloc_internal).Times(0);
	EXPECT_CALL(mock_bypass, qm3x_bypass_open)
		.WillOnce(Return(&mock_bypass.bpc));
	EXPECT_CALL(mock_bypass,
		    qm3x_bypass_control(
			    &mock_bypass.bpc, QM3X_BYPASS_ACTION_MSG_TYPE,
			    Pointee((long)QM3X_TRANSPORT_MSG_COREDUMP)))
		.WillOnce(Return(QM3X_TRANSPORT_MSG_MAX));
	EXPECT_CALL(mock_bypass, qm3x_bypass_recv)
		.WillOnce(Return(4))
		.WillOnce(Return(-EAGAIN));
	EXPECT_CALL(mock_bypass, qm3x_bypass_close).Times(0);
	EXPECT_CALL(mock_cb, callback).WillOnce(Return(4));

	int ret = qmchannel_init(&chan, QMCHANNEL_TYPE_COREDUMP, mock_cb.cb,
				 &mock_cb);
	EXPECT_EQ(ret, 0);
	EXPECT_EQ(chan.type, QMCHANNEL_TYPE_COREDUMP);
	EXPECT_EQ(chan.cb, mock_cb.cb);
	EXPECT_EQ(chan.cb_data, &mock_cb);

	chan.buffer = (char *)buf;
	chan.buffer_size = sizeof(buf);

	ret = mock_bypass.call_callback(QM3X_BYPASS_IRQ);
	EXPECT_EQ(ret, 0);
}

/**********************************************************************/

using TestBypassClose = TestBypass;

TEST_F(TestBypassClose, success)
{
	struct qmchannel chan = {};
	chan.handle = &mock_bypass.bpc;

	EXPECT_CALL(mock_bypass, qm3x_bypass_close(chan.handle))
		.WillOnce(Return(0));
	int ret = qmchannel_close(&chan);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestBypassClose, args_error)
{
	int ret = qmchannel_close(NULL);
	EXPECT_EQ(ret, -EINVAL);
}

TEST_F(TestBypassClose, error)
{
	struct qmchannel chan;

	EXPECT_CALL(mock_bypass, qm3x_bypass_close).WillOnce(Return(-EINVAL));
	int ret = qmchannel_close(&chan);
	EXPECT_EQ(ret, -EINVAL);
}

/**********************************************************************/

using TestBypassGetfd = TestBypass;

TEST_F(TestBypassGetfd, notsupported)
{
	struct qmchannel chan = {};

	int ret = qmchannel_getfd(&chan);
	EXPECT_EQ(ret, -ENOTSUP);
}

/**********************************************************************/

using TestBypassGetpollevent = TestBypass;

TEST_F(TestBypassGetpollevent, notsupported)
{
	struct qmchannel chan;

	int ret = qmchannel_getpollevent(&chan);
	EXPECT_EQ(ret, -ENOTSUP);
}

/**********************************************************************/

using TestBypassSetbuf = TestBypass;

TEST_F(TestBypassSetbuf, success)
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

TEST_F(TestBypassSetbuf, args_error)
{
	char buf = 'a';
	struct qmchannel chan = {};

	int ret = qmchannel_setbuf(NULL, &buf, 512);
	EXPECT_EQ(ret, -EINVAL);

	ret = qmchannel_setbuf(&chan, &buf, 0);
	EXPECT_EQ(ret, -EINVAL);
}

TEST_F(TestBypassSetbuf, size_error)
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

using TestBypassRead = TestBypass;

TEST_F(TestBypassRead, success)
{
	MockChannelCallback mock_cb;
	struct qmchannel chan = {};
	char buf[64] __attribute__((aligned(4))) = { 'a' };

	chan.cb = mock_cb.cb;
	chan.cb_data = &mock_cb;
	chan.handle = &mock_bypass.bpc;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(buf));
	EXPECT_CALL(mock_bypass, qm3x_bypass_recv).WillOnce(Return(42));
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
	EXPECT_CALL(mock_bypass, qm3x_bypass_recv).WillOnce(Return(16));
	EXPECT_CALL(mock_alloc, qfree_internal(buf)).Times(1);
	EXPECT_CALL(mock_cb, callback(_, 16))
		.WillOnce([](void *data, size_t len) -> int {
			qmchannel_freebuf(data);
			return len;
		});
	ret = qmchannel_read(&chan);
	EXPECT_EQ(ret, 16);
}

TEST_F(TestBypassRead, staticbuf_success)
{
	MockChannelCallback mock_cb;
	char buf[64] __attribute__((aligned(4))) = { 'a' };
	struct qmchannel chan = {};

	chan.cb = mock_cb.cb;
	chan.cb_data = &mock_cb;
	chan.buffer = buf;
	chan.buffer_size = 1;

	EXPECT_CALL(mock_alloc, qmalloc_internal).Times(0);
	EXPECT_CALL(mock_bypass, qm3x_bypass_recv).WillOnce(Return(42));
	EXPECT_CALL(mock_alloc, qfree_internal).Times(0);
	EXPECT_CALL(mock_cb, callback(_, 42)).WillOnce(Return(42));
	int ret = qmchannel_read(&chan);
	EXPECT_EQ(ret, 42);
}

TEST_F(TestBypassRead, args_error)
{
	int ret = qmchannel_read(NULL);
	EXPECT_EQ(ret, -EINVAL);
}

TEST_F(TestBypassRead, malloc_error)
{
	MockChannelCallback mock_cb;
	struct qmchannel chan = {};

	chan.cb = mock_cb.cb;
	chan.cb_data = &mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(ReturnNull());
	EXPECT_CALL(mock_bypass, qm3x_bypass_recv).Times(0);
	EXPECT_CALL(mock_alloc, qfree_internal).Times(0);
	EXPECT_CALL(mock_cb, callback).Times(0);
	int ret = qmchannel_read(&chan);
	EXPECT_EQ(ret, -ENOMEM);
}

TEST_F(TestBypassRead, read_error)
{
	MockChannelCallback mock_cb;
	struct qmchannel chan = {};
	char buf[64] __attribute__((aligned(4))) = { 'a' };

	chan.cb = mock_cb.cb;
	chan.cb_data = &mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(buf));
	EXPECT_CALL(mock_bypass, qm3x_bypass_recv).WillOnce(Return(-EAGAIN));
	EXPECT_CALL(mock_alloc, qfree_internal(buf)).Times(1);
	EXPECT_CALL(mock_cb, callback).Times(0);
	int ret = qmchannel_read(&chan);
	EXPECT_EQ(ret, -EAGAIN);
}

TEST_F(TestBypassRead, staticbuf_read_error)
{
	MockChannelCallback mock_cb;
	char buf = 'a';
	struct qmchannel chan = {};

	chan.cb = mock_cb.cb;
	chan.cb_data = &mock_cb;
	chan.buffer = &buf;
	chan.buffer_size = 1;

	EXPECT_CALL(mock_alloc, qmalloc_internal).Times(0);
	EXPECT_CALL(mock_bypass, qm3x_bypass_recv).WillOnce(Return(-EAGAIN));
	EXPECT_CALL(mock_alloc, qfree_internal).Times(0);
	EXPECT_CALL(mock_cb, callback).Times(0);
	int ret = qmchannel_read(&chan);
	EXPECT_EQ(ret, -EAGAIN);
}

/**********************************************************************/

using TestBypassWrite = TestBypass;

TEST_F(TestBypassWrite, success)
{
	char buf = 'a';
	struct qmchannel chan;

	EXPECT_CALL(mock_bypass, qm3x_bypass_send).WillOnce(Return(0));
	int ret = qmchannel_write(&chan, &buf, 1);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestBypassWrite, args_error)
{
	char buf = 'a';

	int ret = qmchannel_write(NULL, &buf, 1);
	EXPECT_EQ(ret, -EINVAL);
}

TEST_F(TestBypassWrite, error)
{
	char buf = 'a';
	struct qmchannel chan;

	EXPECT_CALL(mock_bypass, qm3x_bypass_send).WillOnce(Return(-EINVAL));
	int ret = qmchannel_write(&chan, &buf, 1);
	EXPECT_EQ(ret, -EINVAL);
}

/**********************************************************************/

using TestBypassIoctl = TestBypass;

TEST_F(TestBypassIoctl, success_reset)
{
	unsigned int param;
	struct qmchannel chan = {};

	chan.handle = &mock_bypass.bpc;

	EXPECT_CALL(mock_bypass, qm3x_bypass_control).WillOnce(Return(0));
	int ret = qmchannel_ioctl(&chan, QM35_CTRL_RESET, (char *)&param);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestBypassIoctl, success_reset_ext)
{
	unsigned int param = 1;
	struct qmchannel chan = {};

	chan.handle = &mock_bypass.bpc;

	EXPECT_CALL(mock_bypass, qm3x_bypass_control).WillOnce(Return(0));
	int ret = qmchannel_ioctl(&chan, QM35_CTRL_RESET_EXT, (char *)&param);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestBypassIoctl, success_fw_upload_ext)
{
	struct qm35_fwupload_params ext_params;
	struct qmchannel chan = {};

	chan.handle = &mock_bypass.bpc;
	std::strncpy((char *)&ext_params.fw_name, "/path/to/fw",
		     sizeof(ext_params.fw_name));

	EXPECT_CALL(mock_bypass, qm3x_bypass_control).WillOnce(Return(0));
	int ret = qmchannel_ioctl(&chan, QM35_CTRL_FW_UPLOAD_EXT,
				  (char *)&ext_params);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestBypassIoctl, success_power)
{
	unsigned int param = 1;
	struct qmchannel chan = {};

	chan.handle = &mock_bypass.bpc;

	EXPECT_CALL(mock_bypass, qm3x_bypass_control).WillOnce(Return(0));
	int ret = qmchannel_ioctl(&chan, QM35_CTRL_POWER, (char *)&param);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestBypassIoctl, args_error)
{
	unsigned int param = 0;

	EXPECT_CALL(mock_bypass, qm3x_bypass_control).Times(0);
	int ret = qmchannel_ioctl(NULL, 0, (char *)&param);
	EXPECT_EQ(ret, -EINVAL);
}

TEST_F(TestBypassIoctl, args_error_reset_ext)
{
	struct qmchannel chan = {};

	EXPECT_CALL(mock_bypass, qm3x_bypass_control).Times(0);
	int ret = qmchannel_ioctl(&chan, QM35_CTRL_RESET_EXT, nullptr);
	EXPECT_EQ(ret, -EINVAL);
}

TEST_F(TestBypassIoctl, args_error_fw_upoad_ext)
{
	struct qmchannel chan = {};

	EXPECT_CALL(mock_bypass, qm3x_bypass_control).Times(0);
	int ret = qmchannel_ioctl(&chan, QM35_CTRL_FW_UPLOAD_EXT, nullptr);
	EXPECT_EQ(ret, -EINVAL);
}

TEST_F(TestBypassIoctl, args_error_power)
{
	struct qmchannel chan = {};

	EXPECT_CALL(mock_bypass, qm3x_bypass_control).Times(0);
	int ret = qmchannel_ioctl(&chan, QM35_CTRL_POWER, nullptr);
	EXPECT_EQ(ret, -EINVAL);
}

TEST_F(TestBypassIoctl, unsupported_error)
{
	unsigned int param;
	struct qmchannel chan = {};

	chan.handle = &mock_bypass.bpc;

	EXPECT_CALL(mock_bypass, qm3x_bypass_control).Times(0);
	int ret = qmchannel_ioctl(&chan, QM35_CTRL_SET_TYPE, (char *)&param);
	EXPECT_EQ(ret, -ENOTSUP);
}

TEST_F(TestBypassIoctl, error)
{
	unsigned int param;
	struct qmchannel chan = {};

	chan.handle = &mock_bypass.bpc;

	EXPECT_CALL(mock_bypass, qm3x_bypass_control).WillOnce(Return(-EINVAL));
	int ret = qmchannel_ioctl(&chan, QM35_CTRL_FW_UPLOAD, (char *)&param);
	EXPECT_EQ(ret, -EINVAL);
}

/**********************************************************************/

using TestBypassAllocFreeBuf = TestBypass;

TEST_F(TestBypassAllocFreeBuf, success)
{
	char buf[64] __attribute__((aligned(4))) = { 'a' };

	EXPECT_CALL(mock_alloc, qmalloc_internal(42 + 16)).WillOnce(Return(buf));
	EXPECT_CALL(mock_alloc, qfree_internal(buf)).WillOnce(Return());

	void *ret = qmchannel_allocbuf(42);
	EXPECT_EQ(ret, Q2M(buf)); // qmalloc() add a 16bytes qota struct

	int rc = qmchannel_freebuf(ret);
	EXPECT_EQ(rc, 0);
}
