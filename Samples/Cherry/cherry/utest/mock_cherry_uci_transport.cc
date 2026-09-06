/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "mock_cherry_uci_transport.hh"

MockCherryUciTransport *MockCherryUciTransport::instance_{ nullptr };

MockCherryUciTransport::MockCherryUciTransport()
{
	MockCherryUciTransport::instance_ = this;
}

MockCherryUciTransport::~MockCherryUciTransport()
{
	MockCherryUciTransport::instance_ = nullptr;
}

MockCherryUciTransport *MockCherryUciTransport::Instance()
{
	EXPECT_NE(MockCherryUciTransport::instance_, nullptr);
	return MockCherryUciTransport::instance_;
}

enum cherry_err cherry_init_transport(struct cherry_uci_transport *ctx,
				      struct uci *uci, const char *device)
{
	return MockCherryUciTransport::Instance()->cherry_init_transport(
		ctx, uci, device);
}

void cherry_de_init_transport(struct cherry_uci_transport *ctx)
{
	return MockCherryUciTransport::Instance()->cherry_de_init_transport(
		ctx);
}

int cherry_client_reset(struct cherry_uci_transport *ctx)
{
	return MockCherryUciTransport::Instance()->cherry_client_reset(ctx);
}
