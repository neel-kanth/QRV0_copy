/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <gmock/gmock.h>

extern "C" {
#include "cherry_session_client.h"
#include "uci/uci.h"

struct MockCherrySessionClientInterface {};
}

class MockCherrySessionClient : public MockCherrySessionClientInterface {
    public:
	MOCK_METHOD(qerr, cherry_uci_client_session_open,
		    (struct cherry_session_context * *context, struct uci *uci,
		     void *user_data,
		     cherry_uci_client_session_status_cb_t session_status_cb,
		     cherry_uci_client_session_ranging_ntf_cb_t ranging_ntf_cb));
	MOCK_METHOD(void, cherry_uci_client_session_close,
		    (struct cherry_session_context * context));
	MOCK_METHOD(uci_status_code, cherry_uci_client_session_stop_session,
		    (struct cherry_session_context * context,
		     uint32_t session_handle));
	MOCK_METHOD(uci_status_code, cherry_uci_client_session_init_session,
		    (struct cherry_session_context * context,
		     uint32_t session_id, uint8_t session_type,
		     uint32_t *session_handle));
	MOCK_METHOD(uci_status_code, cherry_uci_client_session_deinit_session,
		    (struct cherry_session_context * context,
		     uint32_t session_handle));
	MOCK_METHOD(
		enum uci_status_code,
		cherry_uci_client_session_set_app_config_cmd_put_dest_mac_address,
		(struct cherry_uci_client_session_set_app_config_cmd * cmd,
		 const struct dst_mac_addresses *dest_mac_add));
	MOCK_METHOD(
		enum uci_status_code,
		cherry_uci_client_session_set_app_config_cmd_put_session_time_base,
		(struct cherry_uci_client_session_set_app_config_cmd * cmd,
		 bool enable, bool continue_session, bool resync,
		 uint32_t session_handle, uint32_t offset_us));
	MOCK_METHOD(uci_status_code, cherry_uci_client_session_start_session,
		    (struct cherry_session_context * context,
		     uint32_t session_handle));
	MOCK_METHOD(struct cherry_uci_client_session_set_app_config_cmd *,
		    cherry_uci_client_session_set_app_config_cmd_create,
		    (struct cherry_session_context * context));
	MOCK_METHOD(enum uci_status_code,
		    cherry_uci_client_session_set_app_config_cmd_put,
		    (struct cherry_uci_client_session_set_app_config_cmd * cmd,
		     uint16_t param_id, const void *data, uint8_t size));
	MOCK_METHOD(enum uci_status_code,
		    cherry_uci_client_session_set_app_config_cmd_send,
		    (struct cherry_uci_client_session_set_app_config_cmd * cmd,
		     uint32_t session_handle));
	MOCK_METHOD(void, cherry_uci_client_session_set_app_config_cmd_abort,
		    (struct cherry_uci_client_session_set_app_config_cmd *
		     cmd));
	MOCK_METHOD(
		enum uci_status_code,
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_location,
		(struct cherry_uci_client_session_set_app_config_cmd * cmd,
		 const struct cherry_fira_anchor_location
			 *dl_tdoa_anchor_location));
	MOCK_METHOD(uint32_t, session_status_ntf_get_session_handle,
		    (const struct session_status_ntf *ntf));
	MOCK_METHOD(enum uci_session_state,
		    session_status_ntf_get_session_state,
		    (const struct session_status_ntf *ntf));
	MOCK_METHOD(enum uci_session_reason_code,
		    session_status_ntf_get_reason_code,
		    (const struct session_status_ntf *ntf));
	MOCK_METHOD(void, cherry_uci_client_session_free_status_ntf,
		    (struct session_status_ntf * ntf));
	MOCK_METHOD(enum cherry_common_frame_status,
		    cherry_session_frame_status_to_cherry_format,
		    (enum fira_status code));

    public:
	MockCherrySessionClient();
	virtual ~MockCherrySessionClient();

	static MockCherrySessionClient *Instance();

    private:
	static MockCherrySessionClient *instance_;
};
