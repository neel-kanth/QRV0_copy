/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <gmock/gmock.h>

extern "C" {
#include "uci/uci.h"

struct MockUciInterface {};
}

class MockUci : public MockUciInterface {
    public:
	MOCK_METHOD(void, uci_set_log_level, (uint8_t level));

	MOCK_METHOD(enum qerr, uci_init,
		    (struct uci * uci, struct uci_allocator *allocator,
		     bool is_client));
	MOCK_METHOD(void, uci_uninit, (struct uci * uci));

    public:
	MockUci();
	virtual ~MockUci();

	static MockUci *Instance();

    private:
	static MockUci *instance_;
};
