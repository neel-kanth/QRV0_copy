/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "test_misc.hh"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_channel_callback.hh"

#include <cstddef>
#include <cstring>

#define BUF_SIZE 4096

using ::testing::Return;
using ::testing::_;

extern "C" {
#include "qm35_uci_dev_ioctl.h"
#include "qmutils/qmutils.h"
}

void TestMisc::SetUp()
{
	calloc_buf = new char[BUF_SIZE]();
}

void TestMisc::TearDown()
{
	if (!mock_alloc.pointers.empty()) {
		// MockQmalloc need all allocated pointers are freed.
		EXPECT_CALL(mock_alloc, qfree_internal(calloc_buf)).Times(1);
		qfree(Q2M(calloc_buf));
	}
	delete[] calloc_buf;
}

/**********************************************************************/

using TestMiscReset = TestMisc;

TEST_F(TestMiscReset, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(calloc_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_uci_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_io, ioctl(_, QM35_CTRL_RESET_EXT, _))
		.WillOnce(Return(0));
	int ret = qmu_reset(hnd, true);
	EXPECT_EQ(ret, 0);
}

/**********************************************************************/

using TestMiscFwUpdate = TestMisc;

TEST_F(TestMiscFwUpdate, no_ext_filename_success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(calloc_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_uci_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_io, ioctl(_, QM35_CTRL_FW_UPLOAD, _))
		.WillOnce(Return(0));
	int ret = qmu_fwupdate(hnd, NULL);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestMiscFwUpdate, ext_filename_success)
{
	MockChannelCallback mock_cb;
	char filename[64] = "qm358xx.bin";

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(calloc_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_uci_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	/* Check that the function returns 0, when the ioctl returns a positive
	 * integer. */
	EXPECT_CALL(mock_io, ioctl(_, QM35_CTRL_FW_UPLOAD_EXT, _))
		.WillOnce(Return(2));
	int ret = qmu_fwupdate(hnd, (char *)filename);
	EXPECT_EQ(ret, 0);
}

TEST_F(TestMiscFwUpdate, ext_filename_too_long_warning)
{
	MockChannelCallback mock_cb;
	char filename[72] =
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz1234567890abcdefg";

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(calloc_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_uci_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_io, ioctl(_, QM35_CTRL_FW_UPLOAD_EXT, _))
		.WillOnce(Return(0));
	int ret = qmu_fwupdate(hnd, (char *)filename);
	EXPECT_EQ(ret, 0);
}

/**********************************************************************/

using TestMiscDisableIrq = TestMisc;

TEST_F(TestMiscDisableIrq, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(calloc_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_uci_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_io, ioctl(_, QM35_CTRL_IRQ, _)).WillOnce(Return(0));
	int ret = qmu_disable_irq(hnd);
	EXPECT_EQ(ret, 0);
}

/**********************************************************************/

using TestMiscEnableIrq = TestMisc;

TEST_F(TestMiscEnableIrq, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(calloc_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_uci_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_io, ioctl(_, QM35_CTRL_IRQ, _)).WillOnce(Return(0));
	int ret = qmu_enable_irq(hnd);
	EXPECT_EQ(ret, 0);
}

/**********************************************************************/

using TestMiscWaitForIrqLine = TestMisc;

TEST_F(TestMiscWaitForIrqLine, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(calloc_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_uci_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_io, ioctl(_, QM35_CTRL_WAIT_IRQ, _))
		.WillOnce(Return(0));
	int ret = qmu_wait_for_irq_line(hnd, 42);
	EXPECT_EQ(ret, 0);
}

/**********************************************************************/

using TestMiscRawTransfer = TestMisc;

TEST_F(TestMiscRawTransfer, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(calloc_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_uci_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_io, ioctl(_, QM35_CTRL_SPI_TRANSFER, _))
		.WillOnce(Return(0));
	int ret = qmu_raw_transfer(hnd, NULL, NULL, 42);
	EXPECT_EQ(ret, 0);
}
