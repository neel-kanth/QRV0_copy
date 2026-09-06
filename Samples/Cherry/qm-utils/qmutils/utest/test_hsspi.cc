/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "test_hsspi.hh"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_channel_callback.hh"

#define BUF_SIZE 4096

using ::testing::Return;

extern "C" {
#include "qmutils/qmutils.h"
}

void TestHsspi::SetUp()
{
	handle_buf = new char[BUF_SIZE]();
}

void TestHsspi::TearDown()
{
	delete[] handle_buf;
}

/**********************************************************************/

using TestHsspiInit = TestHsspi;

TEST_F(TestHsspiInit, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_hsspi_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	// MockQmalloc need all allocated pointers are freed.
	EXPECT_CALL(mock_alloc, qfree_internal(handle_buf)).Times(1);
	qfree(Q2M(handle_buf));
}

/**********************************************************************/

using TestHsspiStart = TestHsspi;

TEST_F(TestHsspiStart, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_hsspi_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_pthread, pthread_create).WillOnce(Return(0));
	int ret = qmu_hsspi_start(hnd);
	EXPECT_EQ(ret, 0);

	// MockQmalloc need all allocated pointers are freed.
	EXPECT_CALL(mock_alloc, qfree_internal(handle_buf)).Times(1);
	qfree(Q2M(handle_buf));
}

/**********************************************************************/

using TestHsspiDestroy = TestHsspi;

TEST_F(TestHsspiDestroy, success)
{
	MockChannelCallback mock_cb;

	EXPECT_CALL(mock_alloc, qmalloc_internal).WillOnce(Return(handle_buf));
	EXPECT_CALL(mock_qmchannel, qmchannel_init).WillOnce(Return(0));
	qmhandle hnd = qmu_hsspi_init(mock_cb.cb, &mock_cb);
	EXPECT_NE(hnd, nullptr);

	EXPECT_CALL(mock_qmchannel, qmchannel_close).WillOnce(Return(0));
	EXPECT_CALL(mock_alloc, qfree_internal(handle_buf)).WillOnce(Return());
	int ret = qmu_hsspi_destroy(hnd);
	EXPECT_EQ(ret, 0);
}
