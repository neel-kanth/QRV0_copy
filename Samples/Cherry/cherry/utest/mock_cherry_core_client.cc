/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "mock_cherry_core_client.hh"

MockCherryCoreClient *MockCherryCoreClient::instance_{ nullptr };

MockCherryCoreClient::MockCherryCoreClient()
{
	MockCherryCoreClient::instance_ = this;
}

MockCherryCoreClient::~MockCherryCoreClient()
{
	MockCherryCoreClient::instance_ = nullptr;
}

MockCherryCoreClient *MockCherryCoreClient::Instance()
{
	EXPECT_NE(MockCherryCoreClient::instance_, nullptr);
	return MockCherryCoreClient::instance_;
}

qerr cherry_uci_client_core_open(
	struct cherry_core_context **context, struct uci *uci, void *user_data,
	cherry_uci_client_core_device_status_cb_t device_status_cb,
	cherry_uci_client_core_boot_cb_t boot_cb)
{
	return MockCherryCoreClient::Instance()->cherry_uci_client_core_open(
		context, uci, user_data, device_status_cb, boot_cb);
}

void cherry_uci_client_core_close(struct cherry_core_context *context)
{
	return MockCherryCoreClient::Instance()->cherry_uci_client_core_close(
		context);
}
uci_status_code
cherry_uci_client_core_device_reset(struct cherry_core_context *context,
				    uint8_t reset)
{
	return MockCherryCoreClient::Instance()
		->cherry_uci_client_core_device_reset(context, reset);
}
uci_status_code cherry_uci_client_core_get_device_info(
	struct cherry_core_context *context,
	struct cherry_core_event_device_info *device_info)
{
	return MockCherryCoreClient::Instance()
		->cherry_uci_client_core_get_device_info(context, device_info);
}

uci_status_code cherry_uci_client_core_get_capabilities(
	struct cherry_core_context *context,
	struct cherry_core_event_device_capabilities *device_caps)
{
	return MockCherryCoreClient::Instance()
		->cherry_uci_client_core_get_capabilities(context, device_caps);
}

uci_status_code cherry_uci_client_core_get_uwb_device_stats(
	struct cherry_core_context *context,
	struct cherry_core_event_device_stats *stats)
{
	return MockCherryCoreClient::Instance()
		->cherry_uci_client_core_get_uwb_device_stats(context, stats);
}
uci_status_code cherry_uci_client_core_get_device_timestamp(
	struct cherry_core_context *context,
	struct cherry_core_event_device_timestamp *device_timestamp)
{
	return MockCherryCoreClient::Instance()
		->cherry_uci_client_core_get_device_timestamp(context,
							      device_timestamp);
}
uci_status_code
cherry_uci_client_core_get_uwbs_state(struct cherry_core_context *context,
				      enum uci_device_state *uwbs_state)
{
	return MockCherryCoreClient::Instance()
		->cherry_uci_client_core_get_uwbs_state(context, uwbs_state);
}
uci_status_code cherry_uci_client_core_set_gpio_toggle_mode(
	struct cherry_core_context *context, uint8_t mode,
	struct cherry_core_event_gpio_toggle *gpio_toggle)
{
	return MockCherryCoreClient::Instance()
		->cherry_uci_client_core_set_gpio_toggle_mode(context, mode,
							      gpio_toggle);
}

void cherry_uci_client_core_capabilities_free(
	struct cherry_core_event_device_capabilities *device_caps)
{
	return MockCherryCoreClient::Instance()
		->cherry_uci_client_core_capabilities_free(device_caps);
}
