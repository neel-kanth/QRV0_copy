/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "mock_uci.hh"

MockUci *MockUci::instance_{ nullptr };

MockUci::MockUci()
{
	MockUci::instance_ = this;
}

MockUci::~MockUci()
{
	MockUci::instance_ = nullptr;
}

MockUci *MockUci::Instance()
{
	EXPECT_NE(MockUci::instance_, nullptr);
	return MockUci::instance_;
}

void uci_set_log_level(uint8_t level)
{
	return MockUci::Instance()->uci_set_log_level(level);
}

enum qerr uci_init(struct uci *uci, struct uci_allocator *allocator,
		   bool is_client)
{
	return MockUci::Instance()->uci_init(uci, allocator, is_client);
}

void uci_uninit(struct uci *uci)
{
	return MockUci::Instance()->uci_uninit(uci);
}
