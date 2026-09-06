/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mock_io.hh"
#include "mock_qmalloc.hh"

class TestMultichan : public ::testing::Test {
    protected:
	void SetUp() override;
	void TearDown() override;

    public:
	MockQmalloc mock_alloc;
	MockIO mock_io;
};
