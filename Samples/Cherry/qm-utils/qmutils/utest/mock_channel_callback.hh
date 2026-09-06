/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <cstddef>
#include <gmock/gmock.h>

class MockChannelCallback {
    public:
	/* clang-format off */
	MOCK_METHOD(int, callback, (void *buf, size_t len));
	MOCK_METHOD(int, end_callback, (bool status));
	/* clang-format on */

    public:
	MockChannelCallback();
	virtual ~MockChannelCallback();

    public:
	static int cb(void *user, void *buf, size_t len);
	static int end_cb(void *user, bool status);
};
