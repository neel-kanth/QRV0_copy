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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>
#include <qmalloc.h>
#include <set>
#include <stddef.h>
#include <stdlib.h>

/**
 * MockQmalloc - Mock for calls defined in qosal/include/qmalloc.h.
 * By default all calls rely on normal allocation functions.
 * But if a MockQmalloc instance exist, then all call are stubbed using the
 * functions defined in the mock instance.
 */
class MockQmalloc {
    public:
	MOCK_METHOD(void *, qmalloc_internal, (size_t size));
	MOCK_METHOD(void *, qrealloc_internal, (void *ptr, size_t new_size));
	MOCK_METHOD(void, qfree_internal, (void *ptr));

    public:
	MockQmalloc();
	virtual ~MockQmalloc();
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
	/*
	 * @deprecated.
	 * Test vectors in C++, should not use the qmalloc interface.
	 */
	void *alloc_test_data(size_t size)
	{
		auto mock = MockQmalloc::get_singleton();

		std::cerr << __FUNCTION__ << " function is deprecated" << std::endl;
		auto *q = reinterpret_cast<quota_alloc_prefix *>(
			calloc(1, size + sizeof(quota_alloc_prefix)));
		mock->pointers.insert(q);
		q->magic = QUOTA_MAGIC;
		return Q2M(q);
	}
	/*
	 * @deprecated.
	 * Test vectors in C++, should not use the qmalloc interface.
	 */
	void delete_test_data(void *ptr)
	{
		auto mock = MockQmalloc::get_singleton();

		std::cerr << __FUNCTION__ << " function is deprecated" << std::endl;
		auto it = mock->pointers.find(M2Q(ptr));
		EXPECT_NE(it, mock->pointers.end());
		mock->pointers.erase(it);
		return free(M2Q(ptr));
	}

    public:
	static MockQmalloc *get_singleton();

    public:
	bool qmalloc_internal_mocked = true;
	bool qcalloc_internal_mocked = true;
	bool qrealloc_internal_mocked = true;
	bool qfree_internal_mocked = true;
	std::set<void *> pointers;

    private:
	static MockQmalloc *instance_;
};

/**
 * Explanation about the deprecated interface:
 *
 * 1. A structure private must not be exposed in public header for the needs of
 *    the utest ! And a utest should only use public headers.
 *
 * 2. The implicit_call have been improved with an std::set, and track every
 *    allocations/frees.
 *    No need to worry about alloc/free, which works(SUCCESS) in all tests not
 *    related to memory failure (the majority of tests).
 *
 * Recommended
 * ===========
 *  - Use `implicit_call` behavior of the mock in test suites.
 *  - For TEST with a allocation failure, use the:
 *    `qmalloc_internal_mocked = true` + the EXPECT_CALL direct.
 *
 *    Note:
 *      Be smart and change the mock mode just before calling the targeted api.
 *      And so avoid many successful malloc to explicitly expect.
 *
 * Example for a private structure
 * -------------------------------
 *
 *  TEST_F(Foo, Failure)
 *  {
 *      mock.qmalloc_internal_mocked = true;
 *      EXPECT_CALL(mock, qmalloc_internal).WillOnce(Return(nullptr));
 *  }
 *
 * Example for a public structure (not only for the needs of the utest so)
 * -----------------------------------------------------------------------
 *
 *  TEST_F(Foo, Failure)
 *  {
 *      mock.qmalloc_internal_mocked = true;
 *      EXPECT_CALL(mock, qmalloc_internal(QMALLOC_SIZE(public_struct)))
 *          .WillOnce(Return(nullptr));
 *  }
 */

/*
 * QMALLOC_SIZE - Overhead added by qmalloc library.
 * Note: The quota struct is public as it is exposed in public header.
 */
#define QMALLOC_SIZE(o_) (sizeof(quota_alloc_prefix) + (o_))

/*
 * @deprecated: See explanation above in this file.
 *
 * EXPECT_QCALLOC_QFREE - Create an expectation for a qcalloc and the associated
 * qfree. We use a macro instead of a function to get better error message (the
 * caller file and name will be in the error message).
 */
#define EXPECT_QCALLOC_QFREE(mock, nb_items, item_size)                        \
	std::cerr << "EXPECT_QCALLOC_QFREE is deprecated" << std::endl;        \
	EXPECT_CALL(mock, qmalloc_internal(QMALLOC_SIZE(nb_items *item_size))) \
		.WillOnce([&](size_t size_) {                                  \
			void *ptr = malloc(size_);                             \
			EXPECT_CALL(mock, qfree_internal(ptr)).WillOnce(free); \
			return ptr;                                            \
		})

/**
 * @deprecated: See explanation above in this file.
 */
#define EXPECT_QCALLOC_QFREE_PRIVATE(mock)                                      \
	std::cerr << "EXPECT_QCALLOC_QFREE_PRIVATE is deprecated" << std::endl; \
	EXPECT_CALL(mock, qmalloc_internal).WillOnce([&](size_t size) {         \
		void *ptr = malloc(size);                                       \
		EXPECT_CALL(mock, qfree_internal(ptr)).WillOnce(free);          \
		return ptr;                                                     \
	})

/**
 * @deprecated: See explanation above in this file.
 *
 * EXPECT_QCALLOC_QFREE_M - Create an expectation for a qcalloc and the
 * associated qfree multiple times.
 */
#define EXPECT_QCALLOC_QFREE_M(mock, nb_items, item_size, n)                   \
	std::cerr << "EXPECT_QCALLOC_QFREE_M is deprecated" << std::endl;      \
	EXPECT_CALL(mock, qmalloc_internal(QMALLOC_SIZE(nb_items *item_size))) \
		.Times(n)                                                      \
		.WillRepeatedly([&](size_t size_) {                            \
			void *ptr = malloc(size_);                             \
			EXPECT_CALL(mock, qfree_internal(ptr)).WillOnce(free); \
			return ptr;                                            \
		})

/**
 * @deprecated: See explanation above in this file.
 */
#define EXPECT_QCALLOC(mock, nb_items, item_size)                 \
	std::cerr << "EXPECT_QCALLOC is deprecated" << std::endl; \
	EXPECT_CALL(mock, qmalloc_internal(QMALLOC_SIZE(nb_items *item_size))).WillOnce(malloc)

/**
 * @deprecated: See explanation above in this file.
 */
#define EXPECT_QCALLOC_RETURN(mock, nb_items, item_size, retptr)               \
	std::cerr << "EXPECT_QCALLOC_RETURN is deprecated" << std::endl;       \
	EXPECT_CALL(mock, qmalloc_internal(QMALLOC_SIZE(nb_items *item_size))) \
		.WillOnce(Return(retptr ? M2Q(retptr) : NULL))

/**
 * @deprecated: See explanation above in this file.
 *
 * EXPECT_QMALLOC_QFREE - Create an expectation for a qmalloc and the associated
 * qfree. We use a macro instead of a function to get better error message (the
 * caller file and name will be in the error message).
 */
#define EXPECT_QMALLOC_QFREE(mock, size)                                                     \
	std::cerr << "EXPECT_QMALLOC_QFREE is deprecated" << std::endl;                      \
	EXPECT_CALL(mock, qmalloc_internal(QMALLOC_SIZE(size))).WillOnce([&](size_t size_) { \
		void *ptr = malloc(size_);                                                   \
		EXPECT_CALL(mock, qfree_internal(ptr)).WillOnce(free);                       \
		return ptr;                                                                  \
	})

/**
 * @deprecated: See explanation above in this file.
 *
 * EXPECT_QMALLOC_QFREE_M - Create an expectation for a qmalloc and the
 * associated qfree multiple times.
 */
#define EXPECT_QMALLOC_QFREE_M(mock, size, n)                                  \
	std::cerr << "EXPECT_QMALLOC_QFREE_M is deprecated" << std::endl;      \
	EXPECT_CALL(mock, qmalloc_internal(QMALLOC_SIZE(size)))                \
		.Times(n)                                                      \
		.WillRepeatedly([&](size_t size_) {                            \
			void *ptr = malloc(size_);                             \
			EXPECT_CALL(mock, qfree_internal(ptr)).WillOnce(free); \
			return ptr;                                            \
		})

/**
 * @deprecated: See explanation above in this file.
 */
#define EXPECT_QMALLOC(mock, size)                                \
	std::cerr << "EXPECT_QMALLOC is deprecated" << std::endl; \
	EXPECT_CALL(mock, qmalloc_internal(QMALLOC_SIZE(size))).WillOnce(malloc)

/**
 * @deprecated: See explanation above in this file.
 */
#define EXPECT_QMALLOC_RETURN(mock, size, retptr)                        \
	std::cerr << "EXPECT_QMALLOC_RETURN is deprecated" << std::endl; \
	EXPECT_CALL(mock, qmalloc_internal(QMALLOC_SIZE(size)))          \
		.WillOnce(Return(retptr ? M2Q(retptr) : NULL))

/**
 * @deprecated: See explanation above in this file.
 *
 * EXPECT_QFREE - Create an expectation for a qmalloc and the associated qfree.
 * We use a macro instead of a function to get better error message (the caller
 * file and name will be in the error message).
 */
#define EXPECT_QFREE(mock, ptr)                                 \
	std::cerr << "EXPECT_QFREE is deprecated" << std::endl; \
	EXPECT_CALL(mock, qfree_internal(M2Q(ptr))).WillOnce(free)

/**
 * @deprecated: See explanation above in this file.
 */
#define EXPECT_QFREE_PRIVATE(mock)                                      \
	std::cerr << "EXPECT_QFREE_PRIVATE is deprecated" << std::endl; \
	EXPECT_CALL(mock, qfree_internal(NotNull())).WillOnce(free)
