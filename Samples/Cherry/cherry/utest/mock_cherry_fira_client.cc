/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "mock_cherry_fira_client.hh"

MockCherryFiraClient *MockCherryFiraClient::instance_{ nullptr };

MockCherryFiraClient::MockCherryFiraClient()
{
	MockCherryFiraClient::instance_ = this;
}

MockCherryFiraClient::~MockCherryFiraClient()
{
	MockCherryFiraClient::instance_ = nullptr;
}

MockCherryFiraClient *MockCherryFiraClient::Instance()
{
	EXPECT_NE(MockCherryFiraClient::instance_, nullptr);
	return MockCherryFiraClient::instance_;
}

qerr cherry_uci_client_fira_open(
	struct cherry_fira_context **context, struct uci *uci, void *user_data,
	cherry_uci_client_fira_diag_notification_cb_t diag_cb)
{
	return MockCherryFiraClient::Instance()->cherry_uci_client_fira_open(
		context, uci, user_data, diag_cb);
}

void cherry_uci_client_fira_close(struct cherry_fira_context *context)
{
	return MockCherryFiraClient::Instance()->cherry_uci_client_fira_close(
		context);
}

void cherry_uci_client_fira_free_session_status_ntf(
	struct session_status_ntf *ntf)
{
	return MockCherryFiraClient::Instance()
		->cherry_uci_client_fira_free_session_status_ntf(ntf);
}

void cherry_uci_client_fira_free_twr_results(struct twr_ranging_results *results)
{
	return MockCherryFiraClient::Instance()
		->cherry_uci_client_fira_free_twr_results(results);
}

void twr_ranging_result_free(struct twr_ranging_results *ranging_results)
{
	return MockCherryFiraClient::Instance()->twr_ranging_result_free(
		ranging_results);
}

void dl_tdoa_ranging_result_free(struct dl_tdoa_ranging_results *ranging_results)
{
	return MockCherryFiraClient::Instance()->dl_tdoa_ranging_result_free(
		ranging_results);
}

void cherry_uci_client_fira_free_diag(struct diagnostic_info *diag)
{
	return MockCherryFiraClient::Instance()
		->cherry_uci_client_fira_free_diag(diag);
}

uci_status_code cherry_uci_client_fira_update_dt_anchor_ranging_rounds(
	struct cherry_fira_context *context, uint32_t session_id,
	uint8_t number_of_active_ranging_rounds, uint8_t *round_indexes,
	uint8_t *ranging_role, uint8_t *number_of_responders,
	uint16_t responder_address_list[][8],
	uint8_t *responder_slot_scheduling, uint8_t responder_slots[][8],
	uint8_t dl_tdoa_update_ranging_round_array
		[DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE],
	uint8_t *dl_tdoa_update_ranging_round_array_size)
{
	return MockCherryFiraClient::Instance()
		->cherry_uci_client_fira_update_dt_anchor_ranging_rounds(
			context, session_id, number_of_active_ranging_rounds,
			round_indexes, ranging_role, number_of_responders,
			responder_address_list, responder_slot_scheduling,
			responder_slots, dl_tdoa_update_ranging_round_array,
			dl_tdoa_update_ranging_round_array_size);
}

uci_status_code cherry_uci_client_fira_update_dt_tag_ranging_rounds(
	struct cherry_fira_context *context, uint32_t session_id,
	uint8_t number_of_active_ranging_rounds, uint8_t *ranging_round_indexes,
	uint8_t dl_tdoa_update_ranging_round_array
		[DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE],
	uint8_t *dl_tdoa_update_ranging_round_array_size)
{
	return MockCherryFiraClient::Instance()
		->cherry_uci_client_fira_update_dt_tag_ranging_rounds(
			context, session_id, number_of_active_ranging_rounds,
			ranging_round_indexes,
			dl_tdoa_update_ranging_round_array,
			dl_tdoa_update_ranging_round_array_size);
}

enum qerr cherry_uci_client_parse_twr_measurements(
	const struct session_ranging_data *data,
	struct twr_ranging_results *twr_results)
{
	return MockCherryFiraClient::Instance()
		->cherry_uci_client_parse_twr_measurements(data, twr_results);
}

enum qerr cherry_uci_client_parse_dltdoa_measurements(
	const struct session_ranging_data *data,
	struct cherry_fira_session_dt_tag_ranging_report *dl_tdoa_results)
{
	return MockCherryFiraClient::Instance()
		->cherry_uci_client_parse_dltdoa_measurements(data,
							      dl_tdoa_results);
}

enum qerr cherry_uci_client_parse_dltdoa_measurements_v2(
	const struct session_ranging_data *data,
	struct cherry_fira_session_dt_tag_ranging_report *dl_tdoa_results)
{
	return MockCherryFiraClient::Instance()
		->cherry_uci_client_parse_dltdoa_measurements_v2(
			data, dl_tdoa_results);
}
