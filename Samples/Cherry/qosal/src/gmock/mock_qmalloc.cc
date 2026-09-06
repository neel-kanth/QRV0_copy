/**
 * @file      mock_qmalloc.cc
 *
 * @brief     Implementations for mock qmalloc
 *
 * @author    Qorvo Paris Applications
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */

#include "mock_qmalloc.hh"

#include <assert.h>
#include <gtest/gtest.h>

extern "C" {
#include <limits.h>
#include <qmalloc.h>
#include <stdint.h>
#include <stdlib.h>
}

uint32_t allocation_quotas[] = {
	UINT_MAX,
#ifdef CONFIG_MEM_QUOTA_ID1
	CONFIG_MEM_QUOTA_ID1,
#else
#error Ouch
#endif
#ifdef CONFIG_MEM_QUOTA_ID2
	CONFIG_MEM_QUOTA_ID2,
#endif
#ifdef CONFIG_MEM_QUOTA_ID3
	CONFIG_MEM_QUOTA_ID3,
#endif
#ifdef CONFIG_MEM_QUOTA_ID4
	CONFIG_MEM_QUOTA_ID4,
#endif
};

MockQmalloc *MockQmalloc::instance_ = nullptr;

void *qmalloc_internal(size_t size)
{
	auto mock = MockQmalloc::get_singleton();
	void *ptr;

	if (mock->qmalloc_internal_mocked)
		ptr = mock->qmalloc_internal(size);
	else
		ptr = malloc(size);
	if (ptr != NULL) {
		mock->pointers.insert(ptr);
	}
	return ptr;
}

void *qrealloc_internal(void *ptr, size_t new_size)
{
	auto mock = MockQmalloc::get_singleton();
	void *new_ptr;
	auto it = mock->pointers.find(ptr);

	/* realloc() can be called with NULL ptr. */
	if (ptr) {
		EXPECT_NE(it, mock->pointers.end());
	}

	if (mock->qrealloc_internal_mocked)
		new_ptr = mock->qrealloc_internal(ptr, new_size);
	else
		new_ptr = realloc(ptr, new_size);

	if (ptr) {
		mock->pointers.erase(it);
	}
	if (new_ptr != nullptr) {
		mock->pointers.insert(new_ptr);
	}
	return new_ptr;
}

void qfree_internal(void *ptr)
{
	auto mock = MockQmalloc::get_singleton();

	if (ptr != NULL) {
		auto it = mock->pointers.find(ptr);
		EXPECT_NE(it, mock->pointers.end());
		mock->pointers.erase(it);
	}
	if (mock->qfree_internal_mocked)
		return mock->qfree_internal(ptr);
	else
		free(ptr);
}

MockQmalloc::MockQmalloc()
{
	/* Check singleton instance. */
	assert(!MockQmalloc::instance_);
	MockQmalloc::instance_ = this;
}

MockQmalloc::~MockQmalloc()
{
	/* Check that all pointers are free. */
	assert(pointers.empty());
	/* Clear singleton pointer.*/
	MockQmalloc::instance_ = nullptr;
}

MockQmalloc *MockQmalloc::get_singleton()
{
	/* Check singleton instance. */
	assert(MockQmalloc::instance_);
	/* Return the singleton object. */
	return MockQmalloc::instance_;
}

void MockQmalloc::explicit_call()
{
	auto mock = MockQmalloc::get_singleton();

	mock->qmalloc_internal_mocked = true;
	mock->qcalloc_internal_mocked = true;
	mock->qrealloc_internal_mocked = true;
	mock->qfree_internal_mocked = true;
}

void MockQmalloc::implicit_call()
{
	auto mock = MockQmalloc::get_singleton();

	mock->qmalloc_internal_mocked = false;
	mock->qcalloc_internal_mocked = false;
	mock->qrealloc_internal_mocked = false;
	mock->qfree_internal_mocked = false;
}
