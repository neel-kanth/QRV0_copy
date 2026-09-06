/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <gmock/gmock.h>

extern "C" {
#include "cherry_calib_client.h"
#include "uci/uci.h"

struct MockCherryCalibClientInterface {};
}

class MockCherryCalibClient : public MockCherryCalibClientInterface {
    public:
	MOCK_METHOD(uci_status_code, cherry_uci_client_calib_set_key,
		    (struct cherry_calib_context * context, const char *key,
		     const char *value, size_t value_size));
	MOCK_METHOD(uci_status_code, cherry_uci_client_calib_get_key,
		    (struct cherry_calib_context * context, const char **keys,
		     const uint16_t n_keys, struct cherry_calib_cb *calib_cb));
	MOCK_METHOD(qerr, cherry_uci_client_calib_open,
		    (struct cherry_calib_context * *context, struct uci *uci));
	MOCK_METHOD(void, cherry_uci_client_calib_close,
		    (struct cherry_calib_context * context));
	MOCK_METHOD(struct cherry_uci_client_uwbs_config_set_cmd *,
		    cherry_uci_client_uwbs_config_set_cmd_create,
		    (struct cherry_calib_context * context));
	MOCK_METHOD(enum uci_status_code,
		    cherry_uci_client_uwbs_config_set_cmd_put,
		    (struct cherry_uci_client_uwbs_config_set_cmd * cmd,
		     const char *keyname, const uint8_t *value,
		     uint8_t value_size));
	MOCK_METHOD(enum uci_status_code,
		    cherry_uci_client_uwbs_config_set_cmd_send,
		    (struct cherry_uci_client_uwbs_config_set_cmd * cmd));
	MOCK_METHOD(void, cherry_uci_client_uwbs_config_set_cmd_abort,
		    (struct cherry_uci_client_uwbs_config_set_cmd * cmd));

    public:
	MockCherryCalibClient();
	virtual ~MockCherryCalibClient();

	static MockCherryCalibClient *Instance();

    private:
	static MockCherryCalibClient *instance_;
};
