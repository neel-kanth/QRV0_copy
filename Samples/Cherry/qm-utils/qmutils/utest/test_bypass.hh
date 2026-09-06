/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mock_bypass.hh"
#include "mock_qmalloc.hh"

/* Tests use dummy structure only. */
struct qm3x {
	int dummy;
};

class TestBypass : public ::testing::Test {
    protected:
	void SetUp() override;
	void TearDown() override;

    public:
	MockQmalloc mock_alloc;
	MockBypass mock_bypass;

    public:
	struct qm3x qm35;
};
