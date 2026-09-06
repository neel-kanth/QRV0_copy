/*
 * Copyright (c) 2021–2022 Qorvo, Inc
 *
 * All rights reserved.
 *
 * NOTICE: All information contained herein is, and remains the property
 * of Qorvo, Inc. and its suppliers, if any. The intellectual and technical
 * concepts herein are proprietary to Qorvo, Inc. and its suppliers, and
 * may be covered by patents, patent applications, and are protected by
 * trade secret and/or copyright law. Dissemination of this information
 * or reproduction of this material is strictly forbidden unless prior written
 * permission is obtained from Qorvo, Inc.
 *
 */

#pragma once

#include <cstdint>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <qerr.h>
#include <qsemaphore.h>
#include <set>

/**
 * MockQsemaphore - Mock for calls defined in qosal/include/qsemaphore.h.
 * By default all calls rely on normal semaphore functions.
 * But if a MockQsemaphore instance exist, then all call are stubbed using the
 * functions defined in the mock instance.
 */
class MockQsemaphore {
    public:
	MOCK_METHOD(struct qsemaphore *, qsemaphore_init,
		    (uint32_t init_count, uint32_t max_count));
	MOCK_METHOD(void, qsemaphore_deinit, (struct qsemaphore * sem));
	MOCK_METHOD(enum qerr, qsemaphore_take, (struct qsemaphore * sem, uint32_t timeout_ms));
	MOCK_METHOD(enum qerr, qsemaphore_give, (struct qsemaphore * sem));

    public:
	MockQsemaphore();
	virtual ~MockQsemaphore();
	/**
	 * explicit_call() - Require the EXPECT_CALL in the TEST.
	 *
	 * It is used for tests that wish to have an allocation failure during
	 * execution.
	 */
	void explicit_call();
	/**
	 * implicit_call - Skip the EXPECT_CALL, and forward all to standard
	 * library.
	 *
	 * Since most TESTs do not involve memory failure, this should be the
	 * default behavior during initialization(WIP).
	 */
	void implicit_call();

    public:
	static MockQsemaphore *get_singleton();

    public:
	bool qsemaphore_init_mocked = false;
	bool qsemaphore_deinit_mocked = false;
	bool qsemaphore_give_mocked = false;
	bool qsemaphore_take_mocked = false;
	std::set<struct qsemaphore *> pointers;

    private:
	static MockQsemaphore *instance_;
};
