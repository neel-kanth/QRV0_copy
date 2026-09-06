/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <gmock/gmock.h>

extern "C" {
#include "cherry_core_client.h"
#include "uci/uci.h"

struct MockCherryCoreClientInterface {};
}

class MockCherryCoreClient : public MockCherryCoreClientInterface {
    public:
	MOCK_METHOD(qerr, cherry_uci_client_core_open,
		    (struct cherry_core_context * *context, struct uci *uci,
		     void *user_data,
		     cherry_uci_client_core_device_status_cb_t device_status_cb,
		     cherry_uci_client_core_boot_cb_t boot_cb));
	MOCK_METHOD(void, cherry_uci_client_core_close,
		    (struct cherry_core_context * context));
	MOCK_METHOD(uci_status_code, cherry_uci_client_core_device_reset,
		    (struct cherry_core_context * context, uint8_t reset));
	MOCK_METHOD(uci_status_code, cherry_uci_client_core_get_device_info,
		    (struct cherry_core_context * context,
		     struct cherry_core_event_device_info *device_info));
	MOCK_METHOD(uci_status_code,
		    cherry_uci_client_core_get_uwb_device_stats,
		    (struct cherry_core_context * context,
		     struct cherry_core_event_device_stats *stats));
	MOCK_METHOD(
		uci_status_code, cherry_uci_client_core_get_device_timestamp,
		(struct cherry_core_context * context,
		 struct cherry_core_event_device_timestamp *device_timestamp));
	MOCK_METHOD(uci_status_code, cherry_uci_client_core_get_capabilities,
		    (struct cherry_core_context * context,
		     struct cherry_core_event_device_capabilities *device_caps));
	MOCK_METHOD(uci_status_code, cherry_uci_client_core_get_uwbs_state,
		    (struct cherry_core_context * context,
		     enum uci_device_state *uwbs_state));
	MOCK_METHOD(uci_status_code,
		    cherry_uci_client_core_set_gpio_toggle_mode,
		    (struct cherry_core_context * context, uint8_t mode,
		     struct cherry_core_event_gpio_toggle *gpio_toggle));
	MOCK_METHOD(void, cherry_uci_client_core_capabilities_free,
		    (struct cherry_core_event_device_capabilities *
		     device_caps));

    public:
	MockCherryCoreClient();
	virtual ~MockCherryCoreClient();

	static MockCherryCoreClient *Instance();

    private:
	static MockCherryCoreClient *instance_;
};
