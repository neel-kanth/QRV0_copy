/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "mock_cherry_calib_client.hh"

MockCherryCalibClient *MockCherryCalibClient::instance_{ nullptr };

MockCherryCalibClient::MockCherryCalibClient()
{
	MockCherryCalibClient::instance_ = this;
}

MockCherryCalibClient::~MockCherryCalibClient()
{
	MockCherryCalibClient::instance_ = nullptr;
}

MockCherryCalibClient *MockCherryCalibClient::Instance()
{
	EXPECT_NE(MockCherryCalibClient::instance_, nullptr);
	return MockCherryCalibClient::instance_;
}
uci_status_code
cherry_uci_client_calib_set_key(struct cherry_calib_context *context,
				const char *key, const char *value,
				size_t value_size)
{
	return MockCherryCalibClient::Instance()
		->cherry_uci_client_calib_set_key(context, key, value,
						  value_size);
}
uci_status_code
cherry_uci_client_calib_get_key(struct cherry_calib_context *context,
				const char **key, const uint16_t n_keys,
				struct cherry_calib_cb *calib_cb)
{
	return MockCherryCalibClient::Instance()
		->cherry_uci_client_calib_get_key(context, key, n_keys,
						  calib_cb);
}

qerr cherry_uci_client_calib_open(struct cherry_calib_context **context,
				  struct uci *uci)
{
	return MockCherryCalibClient::Instance()->cherry_uci_client_calib_open(
		context, uci);
}

void cherry_uci_client_calib_close(struct cherry_calib_context *context)
{
	return MockCherryCalibClient::Instance()->cherry_uci_client_calib_close(
		context);
}

struct cherry_uci_client_uwbs_config_set_cmd *
cherry_uci_client_uwbs_config_set_cmd_create(
	struct cherry_calib_context *context)
{
	return MockCherryCalibClient::Instance()
		->cherry_uci_client_uwbs_config_set_cmd_create(context);
}

enum uci_status_code cherry_uci_client_uwbs_config_set_cmd_put(
	struct cherry_uci_client_uwbs_config_set_cmd *cmd, const char *keyname,
	const uint8_t *value, uint8_t value_size)
{
	return MockCherryCalibClient::Instance()
		->cherry_uci_client_uwbs_config_set_cmd_put(cmd, keyname, value,
							    value_size);
}

enum uci_status_code cherry_uci_client_uwbs_config_set_cmd_send(
	struct cherry_uci_client_uwbs_config_set_cmd *cmd)
{
	return MockCherryCalibClient::Instance()
		->cherry_uci_client_uwbs_config_set_cmd_send(cmd);
}

void cherry_uci_client_uwbs_config_set_cmd_abort(
	struct cherry_uci_client_uwbs_config_set_cmd *cmd)
{
	MockCherryCalibClient::Instance()
		->cherry_uci_client_uwbs_config_set_cmd_abort(cmd);
}
