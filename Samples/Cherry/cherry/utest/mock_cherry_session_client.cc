/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "mock_cherry_session_client.hh"

MockCherrySessionClient *MockCherrySessionClient::instance_{ nullptr };

MockCherrySessionClient::MockCherrySessionClient()
{
	MockCherrySessionClient::instance_ = this;
}

MockCherrySessionClient::~MockCherrySessionClient()
{
	MockCherrySessionClient::instance_ = nullptr;
}

MockCherrySessionClient *MockCherrySessionClient::Instance()
{
	EXPECT_NE(MockCherrySessionClient::instance_, nullptr);
	return MockCherrySessionClient::instance_;
}

qerr cherry_uci_client_session_open(
	struct cherry_session_context **context, struct uci *uci,
	void *user_data,
	cherry_uci_client_session_status_cb_t session_status_cb,
	cherry_uci_client_session_ranging_ntf_cb_t ranging_ntf_cb)
{
	return MockCherrySessionClient::Instance()
		->cherry_uci_client_session_open(context, uci, user_data,
						 session_status_cb,
						 ranging_ntf_cb);
}

void cherry_uci_client_session_close(struct cherry_session_context *context)
{
	return MockCherrySessionClient::Instance()
		->cherry_uci_client_session_close(context);
}

uci_status_code
cherry_uci_client_session_stop_session(struct cherry_session_context *context,
				       uint32_t session_handle)
{
	return MockCherrySessionClient::Instance()
		->cherry_uci_client_session_stop_session(context,
							 session_handle);
}

uci_status_code cherry_uci_client_session_init_session(
	struct cherry_session_context *context, uint32_t session_id,
	uint8_t session_type, uint32_t *session_handle)
{
	return MockCherrySessionClient::Instance()
		->cherry_uci_client_session_init_session(
			context, session_id, session_type, session_handle);
}

uci_status_code
cherry_uci_client_session_deinit_session(struct cherry_session_context *context,
					 uint32_t session_handle)
{
	return MockCherrySessionClient::Instance()
		->cherry_uci_client_session_deinit_session(context,
							   session_handle);
}

struct cherry_uci_client_session_set_app_config_cmd *
cherry_uci_client_session_set_app_config_cmd_create(
	struct cherry_session_context *context)
{
	return MockCherrySessionClient::Instance()
		->cherry_uci_client_session_set_app_config_cmd_create(context);
}

enum uci_status_code cherry_uci_client_session_set_app_config_cmd_put(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint16_t param_id, const void *data, uint8_t size)
{
	return MockCherrySessionClient::Instance()
		->cherry_uci_client_session_set_app_config_cmd_put(
			cmd, param_id, data, size);
}

enum uci_status_code cherry_uci_client_session_set_app_config_cmd_send(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint32_t session_handle)
{
	return MockCherrySessionClient::Instance()
		->cherry_uci_client_session_set_app_config_cmd_send(
			cmd, session_handle);
}

void cherry_uci_client_session_set_app_config_cmd_abort(
	struct cherry_uci_client_session_set_app_config_cmd *cmd)
{
	return MockCherrySessionClient::Instance()
		->cherry_uci_client_session_set_app_config_cmd_abort(cmd);
}

enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_session_time_base(
	struct cherry_uci_client_session_set_app_config_cmd *cmd, bool enable,
	bool continue_session, bool resync, uint32_t session_handle,
	uint32_t offset_us)
{
	return MockCherrySessionClient::Instance()
		->cherry_uci_client_session_set_app_config_cmd_put_session_time_base(
			cmd, enable, continue_session, resync, session_handle,
			offset_us);
}

enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_dest_mac_address(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	const struct dst_mac_addresses *dest_mac_add)
{
	return MockCherrySessionClient::Instance()
		->cherry_uci_client_session_set_app_config_cmd_put_dest_mac_address(
			cmd, dest_mac_add);
}

enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_location(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	const struct cherry_fira_anchor_location *dl_tdoa_anchor_location)
{
	return MockCherrySessionClient::Instance()
		->cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_location(
			cmd, dl_tdoa_anchor_location);
}
uci_status_code
cherry_uci_client_session_start_session(struct cherry_session_context *context,
					uint32_t session_handle)
{
	return MockCherrySessionClient::Instance()
		->cherry_uci_client_session_start_session(context,
							  session_handle);
}

uint32_t
session_status_ntf_get_session_handle(const struct session_status_ntf *ntf)
{
	return MockCherrySessionClient::Instance()
		->session_status_ntf_get_session_handle(ntf);
}

enum uci_session_state
session_status_ntf_get_session_state(const struct session_status_ntf *ntf)
{
	return MockCherrySessionClient::Instance()
		->session_status_ntf_get_session_state(ntf);
}

enum uci_session_reason_code
session_status_ntf_get_reason_code(const struct session_status_ntf *ntf)
{
	return MockCherrySessionClient::Instance()
		->session_status_ntf_get_reason_code(ntf);
}

void cherry_uci_client_session_free_status_ntf(struct session_status_ntf *ntf)
{
	return MockCherrySessionClient::Instance()
		->cherry_uci_client_session_free_status_ntf(ntf);
}

enum cherry_common_frame_status
cherry_session_frame_status_to_cherry_format(enum fira_status code)
{
	return MockCherrySessionClient::Instance()
		->cherry_session_frame_status_to_cherry_format(code);
}
