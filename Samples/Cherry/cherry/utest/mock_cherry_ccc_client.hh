/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <gmock/gmock.h>

extern "C" {
#include "cherry_ccc_client.h"
#include "uci/uci.h"

struct MockCherryCccClientInterface {};
}

class MockCherryCccClient : public MockCherryCccClientInterface {
    public:
	MOCK_METHOD(void, cherry_uci_client_ccc_free_data_controller_report,
		    (struct cherry_ccc_controller_session_report *
		     controller_report));
	MOCK_METHOD(void, cherry_uci_client_ccc_free_data_controlee_report,
		    (struct cherry_ccc_controlee_session_report *
		     controlee_report));
	MOCK_METHOD(enum qerr,
		    cherry_uci_client_parse_ccc_controller_measurements,
		    (const struct session_ranging_data *data,
		     struct cherry_ccc_controller_session_report
			     *controller_report));
	MOCK_METHOD(
		enum qerr, cherry_uci_client_parse_ccc_controlee_measurements,
		(const struct session_ranging_data *data,
		 struct cherry_ccc_controlee_session_report *controlee_report));

    public:
	MockCherryCccClient();
	virtual ~MockCherryCccClient();

	static MockCherryCccClient *Instance();

    private:
	static MockCherryCccClient *instance_;
};
