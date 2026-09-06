/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <gmock/gmock.h>

extern "C" {
#include "cherry_fira_client.h"
#include "uci/uci.h"

struct MockCherryFiraClientInterface {};
}

class MockCherryFiraClient : public MockCherryFiraClientInterface {
    public:
	MOCK_METHOD(qerr, cherry_uci_client_fira_open,
		    (struct cherry_fira_context * *context, struct uci *uci,
		     void *user_data,
		     cherry_uci_client_fira_diag_notification_cb_t diag_cb));
	MOCK_METHOD(void, cherry_uci_client_fira_close,
		    (struct cherry_fira_context * context));
	MOCK_METHOD(uci_status_code, cherry_uci_client_fira_start_session,
		    (struct cherry_fira_context * context,
		     uint32_t session_handle));
	MOCK_METHOD(void, cherry_uci_client_fira_free_session_status_ntf,
		    (struct session_status_ntf * ntf));
	MOCK_METHOD(void, cherry_uci_client_fira_free_twr_results,
		    (struct twr_ranging_results * results));
	MOCK_METHOD(void, twr_ranging_result_free,
		    (struct twr_ranging_results * ranging_results));
	MOCK_METHOD(void, dl_tdoa_ranging_result_free,
		    (struct dl_tdoa_ranging_results * ranging_results));
	MOCK_METHOD(void, cherry_uci_client_fira_free_diag,
		    (struct diagnostic_info * diag));
	MOCK_METHOD(uci_status_code,
		    cherry_uci_client_fira_update_dt_anchor_ranging_rounds,
		    (struct cherry_fira_context * context, uint32_t session_id,
		     uint8_t number_of_active_ranging_rounds,
		     uint8_t *round_indexes, uint8_t *ranging_role,
		     uint8_t *number_of_responders,
		     uint16_t responder_address_list[][8],
		     uint8_t *responder_slot_scheduling,
		     uint8_t responder_slots[][8],
		     uint8_t dl_tdoa_update_ranging_round_array
			     [DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE],
		     uint8_t *dl_tdoa_update_ranging_round_array_size));
	MOCK_METHOD(uci_status_code,
		    cherry_uci_client_fira_update_dt_tag_ranging_rounds,
		    (struct cherry_fira_context * context, uint32_t session_id,
		     uint8_t number_of_active_ranging_rounds,
		     uint8_t *ranging_round_indexes,
		     uint8_t dl_tdoa_update_ranging_round_array
			     [DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE],
		     uint8_t *dl_tdoa_update_ranging_round_array_size));
	MOCK_METHOD(enum qerr, cherry_uci_client_parse_twr_measurements,
		    (const struct session_ranging_data *data,
		     struct twr_ranging_results *twr_results));
	MOCK_METHOD(enum qerr, cherry_uci_client_parse_dltdoa_measurements,
		    (const struct session_ranging_data *data,
		     struct cherry_fira_session_dt_tag_ranging_report
			     *dl_tdoa_results));
	MOCK_METHOD(enum qerr, cherry_uci_client_parse_dltdoa_measurements_v2,
		    (const struct session_ranging_data *data,
		     struct cherry_fira_session_dt_tag_ranging_report
			     *dl_tdoa_results));

    public:
	MockCherryFiraClient();
	virtual ~MockCherryFiraClient();

	static MockCherryFiraClient *Instance();

    private:
	static MockCherryFiraClient *instance_;
};
