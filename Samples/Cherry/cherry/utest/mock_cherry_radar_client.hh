/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <gmock/gmock.h>

extern "C" {
#include "cherry_radar_client.h"
#include "uci/uci.h"

struct MockCherryRadarClientInterface {};
}

class MockCherryRadarClient : public MockCherryRadarClientInterface {
    public:
	MOCK_METHOD(qerr, cherry_uci_client_radar_open,
		    (struct cherry_radar_context * *context, struct uci *uci,
		     void *user_data,
		     cherry_uci_client_radar_notification_cb_t radar_cb));
	MOCK_METHOD(void, cherry_uci_client_radar_close,
		    (struct cherry_radar_context * context));
	MOCK_METHOD(void, cherry_uci_client_radar_free_base_report,
		    (struct cherry_uci_radar_ntf * report));
	MOCK_METHOD(void, cherry_uci_client_radar_free_data_report,
		    (struct cherry_radar_session_report * data));

    public:
	MockCherryRadarClient();
	virtual ~MockCherryRadarClient();

	static MockCherryRadarClient *Instance();

    private:
	static MockCherryRadarClient *instance_;
};
