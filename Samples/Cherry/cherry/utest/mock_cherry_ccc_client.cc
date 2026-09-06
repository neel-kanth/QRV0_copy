/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "mock_cherry_ccc_client.hh"

MockCherryCccClient *MockCherryCccClient::instance_{ nullptr };

MockCherryCccClient::MockCherryCccClient()
{
	MockCherryCccClient::instance_ = this;
}

MockCherryCccClient::~MockCherryCccClient()
{
	MockCherryCccClient::instance_ = nullptr;
}

MockCherryCccClient *MockCherryCccClient::Instance()
{
	EXPECT_NE(MockCherryCccClient::instance_, nullptr);
	return MockCherryCccClient::instance_;
}

void cherry_uci_client_ccc_free_data_controller_report(
	struct cherry_ccc_controller_session_report *controller_report)
{
	return MockCherryCccClient::Instance()
		->cherry_uci_client_ccc_free_data_controller_report(
			controller_report);
}

void cherry_uci_client_ccc_free_data_controlee_report(
	struct cherry_ccc_controlee_session_report *controlee_report)
{
	return MockCherryCccClient::Instance()
		->cherry_uci_client_ccc_free_data_controlee_report(
			controlee_report);
}

enum qerr cherry_uci_client_parse_ccc_controller_measurements(
	const struct session_ranging_data *data,
	struct cherry_ccc_controller_session_report *controller_report)
{
	return MockCherryCccClient::Instance()
		->cherry_uci_client_parse_ccc_controller_measurements(
			data, controller_report);
}

enum qerr cherry_uci_client_parse_ccc_controlee_measurements(
	const struct session_ranging_data *data,
	struct cherry_ccc_controlee_session_report *controlee_report)
{
	return MockCherryCccClient::Instance()
		->cherry_uci_client_parse_ccc_controlee_measurements(
			data, controlee_report);
}
