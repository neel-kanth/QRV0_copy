/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "mock_cherry_radar_client.hh"

MockCherryRadarClient *MockCherryRadarClient::instance_{ nullptr };

MockCherryRadarClient::MockCherryRadarClient()
{
	MockCherryRadarClient::instance_ = this;
}

MockCherryRadarClient::~MockCherryRadarClient()
{
	MockCherryRadarClient::instance_ = nullptr;
}

MockCherryRadarClient *MockCherryRadarClient::Instance()
{
	EXPECT_NE(MockCherryRadarClient::instance_, nullptr);
	return MockCherryRadarClient::instance_;
}

qerr cherry_uci_client_radar_open(
	struct cherry_radar_context **context, struct uci *uci, void *user_data,
	cherry_uci_client_radar_notification_cb_t radar_cb)
{
	return MockCherryRadarClient::Instance()->cherry_uci_client_radar_open(
		context, uci, user_data, radar_cb);
}

void cherry_uci_client_radar_close(struct cherry_radar_context *context)
{
	return MockCherryRadarClient::Instance()->cherry_uci_client_radar_close(
		context);
}

void cherry_uci_client_radar_free_base_report(
	struct cherry_uci_radar_ntf *report)
{
	return MockCherryRadarClient::Instance()
		->cherry_uci_client_radar_free_base_report(report);
}

void cherry_uci_client_radar_free_data_report(
	struct cherry_radar_session_report *data)
{
	return MockCherryRadarClient::Instance()
		->cherry_uci_client_radar_free_data_report(data);
}
