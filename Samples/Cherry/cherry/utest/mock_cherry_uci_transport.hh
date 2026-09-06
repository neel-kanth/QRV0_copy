/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <gmock/gmock.h>

extern "C" {
#include "cherry/cherry.h"
#include "cherry_uci_transport.h"
#include "uci/uci.h"

struct MockCherryUciTransportInterface {};
}

class MockCherryUciTransport : public MockCherryUciTransportInterface {
    public:
	MOCK_METHOD(enum cherry_err, cherry_init_transport,
		    (struct cherry_uci_transport * context, struct uci *uci,
		     const char *device));
	MOCK_METHOD(void, cherry_de_init_transport,
		    (struct cherry_uci_transport * context));
	MOCK_METHOD(int, cherry_client_reset,
		    (struct cherry_uci_transport * context));

    public:
	MockCherryUciTransport();
	virtual ~MockCherryUciTransport();

	static MockCherryUciTransport *Instance();

    private:
	static MockCherryUciTransport *instance_;
};
