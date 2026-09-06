/*
 * Implementation for fira client functions
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

extern "C" {
#include "cherry_core_client.h"

#include <qmalloc.h>
#include <uci/uci.h>
#include <uci/uci_message.h>
#include <uci/uci_spec_fira.h>
#include <uci_internal.h>
}

#include "mock_qmalloc.hh"
#include "mock_qsemaphore.hh"
#include "mock_uci_transport.h"

#include <cstring>

using testing::Mock;
using testing::Return;
using testing::StrictMock;

#define CORE_RESET 0x00

/* Debug log level. */
unsigned int cherry_log_level = CHERRY_LOG_LEVEL_DEBUG;

static struct uci_blk *simple_acquire(struct uci_allocator *allocator,
				      size_t size_hint, uint8_t flags_hint)
{
	struct uci_blk *p;

	p = (struct uci_blk *)qmalloc(sizeof(*p) + UCI_MAX_PACKET_SIZE);
	if (p) {
		p->data = (uint8_t *)&p[1];
		p->size = UCI_MAX_PACKET_SIZE;
	}
	return p;
}

static void simple_release(struct uci_allocator *allocator,
			   struct uci_blk *packet)
{
	qfree(packet);
}

static struct uci_allocator_ops simple_allocator_ops = {
	.alloc = simple_acquire,
	.free = simple_release,
};

static struct uci_allocator simple_allocator = { .ops = &simple_allocator_ops };

static void device_status_cb(const enum uci_device_state new_state,
			     void *user_data)
{
}

static void boot_cb(const enum uci_qorvo_boot_reason reason, void *user_data)
{
}

//Test suite
class TestCoreClient : public ::testing::Test {
    protected:
	void SetUp() override
	{
		mock_alloc.implicit_call();
		mock_sema.implicit_call();
		ASSERT_EQ(uci_init(&uci, &simple_allocator, true), 0);
		ASSERT_EQ(uci_transport_attach(&uci, &transport), 0);
		ASSERT_EQ(cherry_uci_client_core_open(&core_ctx, &uci, NULL,
						      device_status_cb,
						      boot_cb),
			  0);
	}
	void TearDown() override
	{
		if (expected_timeout)
			mock_sema.implicit_call();
		ASSERT_TRUE(Mock::VerifyAndClearExpectations(&transport));
		uci_transport_detach(&uci);
		cherry_uci_client_core_close(core_ctx);
		uci_uninit(&uci);
	}
	void ExpectTimeout()
	{
		mock_sema.explicit_call();
		EXPECT_CALL(mock_sema, qsemaphore_take)
			.WillOnce(Return(QERR_ETIME));
		EXPECT_CALL(mock_sema, qsemaphore_give).Times(0);
		expected_timeout = true;
	}

	struct uci uci;
	struct cherry_core_context *core_ctx = NULL;
	struct cherry_core_event_device_capabilities *capabilities;
	struct cherry_core_event_device_info *device_info;
	struct cherry_core_event_device_timestamp *device_timestamp;
	struct cherry_core_event_gpio_toggle *gpio_toggle;

	StrictMock<MockUciTransport> transport;
	MockQmalloc mock_alloc;
	MockQsemaphore mock_sema;
	bool expected_timeout = false;

    public:
	static void
	device_status_cb_with_expect(const enum uci_device_state new_state,
				     void *user_data)
	{
		EXPECT_EQ(new_state, expected_state);
		status_cb_cnt++;
	}
	static enum uci_device_state expected_state;
	static uint8_t status_cb_cnt;
	static void boot_cb_with_expect(const enum uci_qorvo_boot_reason reason,
					void *user_data)
	{
		EXPECT_EQ(reason, expected_reason);
		boot_cb_cnt++;
	}
	static enum uci_qorvo_boot_reason expected_reason;
	static uint8_t boot_cb_cnt;
};

class TestCoreClientNtf : public ::TestCoreClient {
	void SetUp() override
	{
		mock_alloc.implicit_call();
		mock_sema.implicit_call();
		ASSERT_EQ(uci_init(&uci, &simple_allocator, true), 0);
		ASSERT_EQ(uci_transport_attach(&uci, &transport), 0);
		ASSERT_EQ(cherry_uci_client_core_open(
				  &core_ctx, &uci, NULL,
				  device_status_cb_with_expect,
				  boot_cb_with_expect),
			  0);
	}
};

enum uci_device_state TestCoreClient::expected_state = UCI_DEVICE_STATE_ERROR;
uint8_t TestCoreClient::status_cb_cnt = 0;
enum uci_qorvo_boot_reason TestCoreClient::expected_reason =
	UCI_QORVO_BOOT_REASON_FATAL_ERROR_RESET;
uint8_t TestCoreClient::boot_cb_cnt = 0;

/**
 * cherry_uci_client_core_device_reset() - reset the device
 *
 * @core_ctx: Core context.
 * @reset: Device Reset.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestCoreClient, core_device_resetNoContext)
{
	EXPECT_EQ(cherry_uci_client_core_device_reset(NULL, CORE_RESET),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_core_device_reset() - reset the device
 *
 * @core_ctx: Core context.
 * @reset: Device Reset.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestCoreClient, core_device_resetOk)
{
	//Set the response back to the cherry_uci_client_core
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_CORE,
					     UCI_OID_CORE_DEVICE_RESET);
	struct uci_blk *resp = uci_blk_alloc(&uci, 5, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 5);
		EXPECT_EQ(blk->total_len, 1);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_CORE_DEVICE_RESET);
		EXPECT_EQ(blk->data[3], 1);
		//FiRa consortium v2.0 section 8.3 App core reset config
		EXPECT_EQ(blk->data[4], CORE_RESET);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_device_reset(core_ctx, CORE_RESET),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_core_device_reset() - reset the device
 *
 * @core_ctx: Core context.
 * @reset: Device Reset.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestCoreClient, core_device_resetTimeout)
{
	//No response back to the cherry_uci_client_core: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 5);
		EXPECT_EQ(blk->total_len, 1);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_CORE_DEVICE_RESET);
		EXPECT_EQ(blk->data[3], 1);
		//FiRa consortium v2.0 section 8.3 App core reset config
		EXPECT_EQ(blk->data[4], CORE_RESET);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_device_reset(core_ctx, CORE_RESET),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_core_device_reset() - reset the device
 *
 * @core_ctx: Core context.
 * @reset: Device Reset.
 *
 * Testcase_Type :
 *      Invalid response case
 **/
TEST_F(TestCoreClient, core_device_resetInvalidResponse)
{
	//Set the response back to the cherry_uci_client_core with one byte missing
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_CORE,
					     UCI_OID_CORE_DEVICE_RESET);
	struct uci_blk *resp = uci_blk_alloc(&uci, 4, 0);
	resp->total_len = 0;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 5);
		EXPECT_EQ(blk->total_len, 1);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_CORE_DEVICE_RESET);
		EXPECT_EQ(blk->data[3], 1);
		//FiRa consortium v2.0 section 8.3 App core reset config
		EXPECT_EQ(blk->data[4], CORE_RESET);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_device_reset(core_ctx, CORE_RESET),
		  UCI_STATUS_SYNTAX_ERROR);
}

/**
 * cherry_uci_client_core_get_device_info() - gets the device information.
 *
 * @core_ctx: Core context.
 * @device_info: FiRa device info
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestCoreClient, get_device_infoInval)
{
	EXPECT_EQ(cherry_uci_client_core_get_device_info(NULL, device_info),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_core_get_device_info(core_ctx, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_core_get_device_info() - gets the device information.
 *
 * @core_ctx: Core context.
 * @device_info: FiRa device info
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestCoreClient, get_device_infoOk)
{
	//Set the response back to the cherry_uci_client_core
	const int uci_blk_len = 100;
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_CORE,
					     UCI_OID_CORE_GET_DEVICE_INFO);
	struct uci_blk *resp = uci_blk_alloc(&uci, uci_blk_len, 0);
	device_info = (struct cherry_core_event_device_info *)qcalloc(
		1, sizeof(struct cherry_core_event_device_info));
	device_info->fw_version =
		(char *)qcalloc(1, CHERRY_DEV_INFO_FW_VERSION_SIZE);
	resp->len = uci_blk_len;
	resp->total_len = resp->len - UCI_PACKET_HEADER_SIZE;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	//Set vendor info length
	resp->data[13] = resp->total_len - 10;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 4);
		EXPECT_EQ(blk->total_len, 0);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_CORE_GET_DEVICE_INFO);
		EXPECT_EQ(blk->data[3], 0);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_get_device_info(core_ctx, device_info),
		  UCI_STATUS_OK);
	if (device_info->fw_version)
		qfree(device_info->fw_version);
	if (device_info)
		qfree(device_info);
}

/**
 * cherry_uci_client_core_get_device_info() - gets the device information without flavor field.
 *
 * @core_ctx: Core context.
 * @device_info: FiRa device info
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestCoreClient, get_device_info_no_flavorOk)
{
	//Set the response back to the cherry_uci_client_core
	const int uci_blk_len = 66;
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_CORE,
					     UCI_OID_CORE_GET_DEVICE_INFO);
	struct uci_blk *resp = uci_blk_alloc(&uci, uci_blk_len, 0);
	device_info = (struct cherry_core_event_device_info *)qcalloc(
		1, sizeof(struct cherry_core_event_device_info));
	device_info->fw_version =
		(char *)qcalloc(1, CHERRY_DEV_INFO_FW_VERSION_SIZE);
	resp->len = uci_blk_len;
	resp->total_len = resp->len - UCI_PACKET_HEADER_SIZE;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	//Set vendor info length
	resp->data[13] = resp->total_len - 10;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 4);
		EXPECT_EQ(blk->total_len, 0);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_CORE_GET_DEVICE_INFO);
		EXPECT_EQ(blk->data[3], 0);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_get_device_info(core_ctx, device_info),
		  UCI_STATUS_OK);
	if (device_info->fw_version)
		qfree(device_info->fw_version);
	if (device_info)
		qfree(device_info);
}

/**
 * cherry_uci_client_core_get_device_info() - gets the device information.
 *
 * @core_ctx: Core context.
 * @device_info: FiRa device info
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestCoreClient, get_device_infoTimeout)
{
	device_info = (struct cherry_core_event_device_info *)qcalloc(
		1, sizeof(struct cherry_core_event_device_info));

	//No response back to the cherry_uci_client_core: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 4);
		EXPECT_EQ(blk->total_len, 0);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_CORE_GET_DEVICE_INFO);
		EXPECT_EQ(blk->data[3], 0);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_get_device_info(core_ctx, device_info),
		  UCI_STATUS_UCI_MESSAGE_RETRY);

	if (device_info)
		qfree(device_info);
}

/**
 * cherry_uci_client_core_get_device_info() - gets the device information.
 *
 * @core_ctx: Core context.
 * @device_info: FiRa device info
 *
 * Testcase_Type :
 *      Invalid response case
 **/
TEST_F(TestCoreClient, get_device_infoInvalidResponse)
{
	//Set the response back to the cherry_uci_client_core with less payload than
	//generic device info
	const int uci_blk_len = 5;
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_CORE,
					     UCI_OID_CORE_GET_DEVICE_INFO);
	struct uci_blk *resp = uci_blk_alloc(&uci, uci_blk_len, 0);
	resp->len = uci_blk_len;
	resp->total_len = resp->len - UCI_PACKET_HEADER_SIZE;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	device_info = (struct cherry_core_event_device_info *)qcalloc(
		1, sizeof(struct cherry_core_event_device_info));

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 4);
		EXPECT_EQ(blk->total_len, 0);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_CORE_GET_DEVICE_INFO);
		EXPECT_EQ(blk->data[3], 0);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_get_device_info(core_ctx, device_info),
		  UCI_STATUS_SYNTAX_ERROR);

	if (device_info)
		qfree(device_info);
}
/*
 * cherry_uci_client_core_get_device_info() - gets the device information.
 *
 * @core_ctx: Core context.
 * @device_info: FiRa device info
 *
 * Testcase_Type :
 *      Vendor info length does not match remaining payload case
 **/
TEST_F(TestCoreClient, get_device_infoBadVendorInfoLength)
{
	//Set the response back to the cherry_uci_client_core
	const int uci_blk_len = 66;
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_CORE,
					     UCI_OID_CORE_GET_DEVICE_INFO);
	struct uci_blk *resp = uci_blk_alloc(&uci, uci_blk_len, 0);
	device_info = (struct cherry_core_event_device_info *)qcalloc(
		1, sizeof(struct cherry_core_event_device_info));
	resp->len = uci_blk_len;
	resp->total_len = resp->len - UCI_PACKET_HEADER_SIZE;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	//Set vendor info length to a number not equal to the remaining payload
	resp->data[13] = 255;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 4);
		EXPECT_EQ(blk->total_len, 0);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_CORE_GET_DEVICE_INFO);
		EXPECT_EQ(blk->data[3], 0);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_get_device_info(core_ctx, device_info),
		  UCI_STATUS_SYNTAX_ERROR);
	if (device_info)
		qfree(device_info);
}

/**
 * cherry_uci_client_core_get_device_info() - gets the device information.
 *
 * @core_ctx: Core context.
 * @device_info: FiRa device info
 *
 * Testcase_Type :
 *      Too much data case
 **/
TEST_F(TestCoreClient, get_device_infoTooMuchData)
{
	//Set the response back to the cherry_uci_client_core with more payload than the
	//length of vendor_specific_info array
	const int uci_blk_len = 128;
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_CORE,
					     UCI_OID_CORE_GET_DEVICE_INFO);
	struct uci_blk *resp = uci_blk_alloc(&uci, uci_blk_len, 0);
	device_info = (struct cherry_core_event_device_info *)qcalloc(
		1, sizeof(struct cherry_core_event_device_info));
	device_info->fw_version =
		(char *)qcalloc(1, CHERRY_DEV_INFO_FW_VERSION_SIZE);
	resp->len = uci_blk_len;
	resp->total_len = resp->len - UCI_PACKET_HEADER_SIZE;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	//Set vendor info length
	resp->data[13] = resp->total_len - 10;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 4);
		EXPECT_EQ(blk->total_len, 0);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_CORE_GET_DEVICE_INFO);
		EXPECT_EQ(blk->data[3], 0);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_get_device_info(core_ctx, device_info),
		  UCI_STATUS_OK);
	if (device_info->fw_version)
		qfree(device_info->fw_version);
	if (device_info)
		qfree(device_info);
}

/**
 * cherry_uci_client_core_get_capabilities() - Get the device capabilities.
 *
 * @core_ctx: Core context.
 * @capabilites: FiRa capabilites.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestCoreClient, get_capabilitiesNoContext)
{
	EXPECT_EQ(cherry_uci_client_core_get_capabilities(NULL, capabilities),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_core_get_capabilities(core_ctx, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/* clang-format off */
uint8_t capabilities_message[] = {
	/* Response status */
	UCI_STATUS_OK,
	/* Number of capabilities */
	0x26,
	/* Type / Length / Value */
	0x00, 0x2, 0x03, 0x01,
	0x01, 0x2, 0xff, 0x00,
	0x02, 0x4, 0x01, 0x01, 0x01, 0x01,
	0x03, 0x4, 0x01, 0x01, 0x01, 0x01,
	0x04, 0x1, 0x03,
	0x05, 0x2, 0x03, 0x00,
	0x06, 0x2, 0x06, 0x00,
	0x07, 0x1, 0x01,
	0x08, 0x1, 0x03,
	0x09, 0x1, 0x02,
	0x0a, 0x1, 0x03,
	0x0b, 0x1, 0x01,
	0x0c, 0x1, 0x01,
	0x0d, 0x1, 0x01,
	0x0e, 0x1, 0x09,
	0x0f, 0x1, 0x0a,
	0x10, 0x1, 0x01,
	0x11, 0x1, 0x08,
	0x12, 0x5, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x13, 0x1, 0x0f,
	0x14, 0x1, 0x00,
	0x15, 0x1, 0x00,
	0x16, 0x1, 0x03,
	0x17, 0x1, 0x21,
	0x18, 0x1, 0x21,
	0x1a, 0x1, 0x02,
	0x1b, 0x2, 0x04, 0x00,
	0x1c, 0x1, 0x01,
	0xA0, 0x1, 0x20,
	0xA1, 0x4, 0x00, 0x01, 0x00, 0x00,
	0xA2, 0x1, 0x02,
	0xA3, 0x1, 0x01,
	0xA4, 0x4, 0x01, 0x00, 0x01, 0x01,
	0xA5, 0x4, 0x00, 0x00, 0x00, 0x01,
	0xA6, 0x5, 0x00, 0x01, 0x02, 0x10, 0x21,
	0xA7, 0x1, 0xFF,
	0xb0, 0x1, 0x01,
	/* Last is for test unknow Type */
	0xf0, 0x01, 0x00,
};

/**
 * cherry_uci_client_core_get_capabilities() - Get the device capabilities.
 *
 * @core_ctx: Core context.
 * @capabilites: FiRa capabilites.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestCoreClient, get_capabilitiesOk)
{
	//Set the response back to the cherry_uci_client_core
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_CORE,
			       UCI_OID_CORE_GET_CAPS_INFO);
	uint8_t data_size = sizeof(capabilities_message);
	struct uci_blk *resp = uci_blk_alloc(&uci, data_size, 0);
	capabilities = (struct cherry_core_event_device_capabilities *)qcalloc(
		1, sizeof(struct cherry_core_event_device_capabilities));
	resp->total_len = data_size;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	memcpy(&resp->data[4], capabilities_message, data_size);

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 4);
		EXPECT_EQ(blk->total_len, 0);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_CORE_GET_CAPS_INFO);
		EXPECT_EQ(blk->data[3], 0);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_get_capabilities(core_ctx, capabilities),
		  UCI_STATUS_OK);
	cherry_uci_client_core_capabilities_free(capabilities);
	if(capabilities)
		qfree(capabilities);
}

/**
 * cherry_uci_client_core_get_capabilities() - Get the device capabilities.
 *
 * @core_ctx: Core context.
 * @capabilites: FiRa capabilites.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestCoreClient, get_capabilitiesTimeout)
{
	//No response back to the cherry_uci_client_core: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 4);
		EXPECT_EQ(blk->total_len, 0);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_CORE_GET_CAPS_INFO);
		EXPECT_EQ(blk->data[3], 0);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_get_capabilities(core_ctx, capabilities),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_core_get_capabilities() - Get the device capabilities.
 *
 * @core_ctx: Core context.
 * @capabilites: FiRa capabilites.
 *
 * Testcase_Type :
 *      Invalid response case
 **/
TEST_F(TestCoreClient, get_capabilitiesInvalidResponse)
{
	//Set the response back to the cherry_uci_client_core with 4 bytes missing
	//This stop parsing in middle of TLV
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_CORE,
			       UCI_OID_CORE_GET_CAPS_INFO);
	capabilities = (struct cherry_core_event_device_capabilities *)qcalloc(
		1, sizeof(struct cherry_core_event_device_capabilities));
	uint8_t data_size = sizeof(capabilities_message) - 7;
	struct uci_blk *resp = uci_blk_alloc(&uci, data_size, 0);
	resp->total_len = data_size;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	memcpy(&resp->data[4], capabilities_message, data_size);

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 4);
		EXPECT_EQ(blk->total_len, 0);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_CORE_GET_CAPS_INFO);
		EXPECT_EQ(blk->data[3], 0);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_get_capabilities(core_ctx, capabilities),
		  UCI_STATUS_SYNTAX_ERROR);
	cherry_uci_client_core_capabilities_free(capabilities);
	if(capabilities)
		qfree(capabilities);
}

/**
 * cherry_uci_client_core_get_capabilities() - Get the device capabilities.
 *
 * @core_ctx: Core context.
 * @capabilites: FiRa capabilites.
 *
 * Testcase_Type :
 *      Too short response case
 **/
TEST_F(TestCoreClient, get_capabilitiesTooShort)
{
	//Set the response back to the cherry_uci_client_core with response too short
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_CORE,
			       UCI_OID_CORE_GET_CAPS_INFO);
	struct uci_blk *resp = uci_blk_alloc(&uci, 1, 0);
	capabilities = (struct cherry_core_event_device_capabilities *)qcalloc(
		1, sizeof(struct cherry_core_event_device_capabilities));
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 4);
		EXPECT_EQ(blk->total_len, 0);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_CORE_GET_CAPS_INFO);
		EXPECT_EQ(blk->data[3], 0);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_get_capabilities(core_ctx, capabilities),
		  UCI_STATUS_SYNTAX_ERROR);
	cherry_uci_client_core_capabilities_free(capabilities);
	if(capabilities)
		qfree(capabilities);
}

/**
 * cherry_uci_client_core_get_capabilities() - Get the device capabilities.
 *
 * @core_ctx: Core context.
 * @capabilites: FiRa capabilites.
 *
 * Testcase_Type :
 *      UCI Status Error case
 **/
 TEST_F(TestCoreClient, get_capabilitiesUciStatusError)
 {
	//Set the response back to the cherry_uci_client_core
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_CORE,
			       UCI_OID_CORE_GET_CAPS_INFO);
	uint8_t data_size = sizeof(capabilities_message);
	struct uci_blk *resp = uci_blk_alloc(&uci, data_size, 0);
	capabilities = (struct cherry_core_event_device_capabilities *)qcalloc(
		1, sizeof(struct cherry_core_event_device_capabilities));
	resp->total_len = data_size;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_UNKNOWN;

	 transport.SetReply(resp);

	 //Verify the Command Out
	 EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		 EXPECT_EQ(blk->len, 4);
		 EXPECT_EQ(blk->total_len, 0);
		 EXPECT_TRUE(uci_blk_has_header(blk));
		 EXPECT_FALSE(uci_blk_is_segment(blk));
		 uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		 EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		 EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
		 EXPECT_EQ(UCI_OID(mt_gid_oid),
			   UCI_OID_CORE_GET_CAPS_INFO);
		 EXPECT_EQ(blk->data[3], 0);
		 EXPECT_FALSE(blk->next);
		 return 0;
	 });

	 EXPECT_EQ(cherry_uci_client_core_get_capabilities(core_ctx, capabilities),
		   UCI_STATUS_UNKNOWN);

    if(capabilities)
		 qfree(capabilities);
 }

/**
 * cherry_uci_client_core_get_device_timestamp() - gets the device timestamp.
 *
 * @core_ctx: Core context.
 * @device_timestamp: Device timestamp.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestCoreClient, get_device_timestampInval)
{
	EXPECT_EQ(cherry_uci_client_core_get_device_timestamp(NULL,
			device_timestamp),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_core_get_device_timestamp(core_ctx, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_core_get_device_timestamp() - gets the device timestamp.
 *
 * @core_ctx: Core context.
 * @device_timestamp: Device timestamp.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestCoreClient, get_device_timestampOk)
{
	//Set the response back to the cherry_uci_client_core
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_CORE,
			       UCI_OID_CORE_QUERY_UWBS_TIMESTAMP);
	uint8_t timestamp_message[9] = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88
	};
	uint8_t data_size = sizeof(timestamp_message);

	struct uci_blk *resp = uci_blk_alloc(&uci, data_size, 0);
	device_timestamp = (struct cherry_core_event_device_timestamp *)qcalloc(
	1, sizeof(struct cherry_core_event_device_timestamp));
	resp->total_len = data_size;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	memcpy(&resp->data[4], timestamp_message, data_size);

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 4);
		EXPECT_EQ(blk->total_len, 0);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_CORE_QUERY_UWBS_TIMESTAMP);
		EXPECT_EQ(blk->data[3], 0);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_get_device_timestamp(core_ctx,
				device_timestamp),
		  UCI_STATUS_OK);

	if (device_timestamp)
		qfree(device_timestamp);
}

/**
 * cherry_uci_client_core_get_device_timestamp() - gets the device timestamp.
 *
 * @core_ctx: Core context.
 * @device_timestamp: Device timestamp
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestCoreClient, get_device_timestampTimeout)
{
	device_timestamp = (struct cherry_core_event_device_timestamp *)qcalloc(
		1, sizeof(struct cherry_core_event_device_timestamp));

	//No response back to the cherry_uci_client_core: timeout and retry status

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 4);
		EXPECT_EQ(blk->total_len, 0);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_CORE_QUERY_UWBS_TIMESTAMP);
		EXPECT_EQ(blk->data[3], 0);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_get_device_timestamp(core_ctx,
				device_timestamp),
		  UCI_STATUS_UCI_MESSAGE_RETRY);

	if (device_timestamp)
		qfree(device_timestamp);
}

/**
 * cherry_uci_client_core_get_device_timestamp() - gets the device timestamp.
 *
 * @core_ctx: Core context.
 * @device_timestamp: Device timestamp
 *
 * Testcase_Type :
 *      Invalid response case
 **/
TEST_F(TestCoreClient, get_device_timestampInvalidResponse)
{
	//Set the response back to the cherry_uci_client_core with less payload
	//than expected
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_CORE,
			       UCI_OID_CORE_QUERY_UWBS_TIMESTAMP);
	uint8_t timestamp_message[5] = {
		0x00, 0x11, 0x22, 0x33, 0x44
	};
	uint8_t data_size = sizeof(timestamp_message);

	struct uci_blk *resp = uci_blk_alloc(&uci, data_size, 0);
	device_timestamp = (struct cherry_core_event_device_timestamp *)qcalloc(
	1, sizeof(struct cherry_core_event_device_timestamp));
	resp->total_len = data_size;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	memcpy(&resp->data[4], timestamp_message, data_size);

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 4);
		EXPECT_EQ(blk->total_len, 0);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_CORE_QUERY_UWBS_TIMESTAMP);
		EXPECT_EQ(blk->data[3], 0);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_get_device_timestamp(core_ctx,
				device_timestamp),
		  UCI_STATUS_SYNTAX_ERROR);

	if (device_timestamp)
		qfree(device_timestamp);
}

/**
 * cherry_uci_client_core_get_device_timestamp() - gets the device timestamp.
 *
 * @core_ctx: Core context.
 * @device_info: Device timestamp
 *
 * Testcase_Type :
 *      Too much data case
 **/
TEST_F(TestCoreClient, get_device_timestampTooMuchData)
{
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_CORE,
						UCI_OID_CORE_QUERY_UWBS_TIMESTAMP);
	uint8_t timestamp_message[20] = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0x33, 0x44
	};
	uint8_t data_size = sizeof(timestamp_message);

	struct uci_blk *resp = uci_blk_alloc(&uci, data_size, 0);
	device_timestamp = (struct cherry_core_event_device_timestamp *)qcalloc(
	1, sizeof(struct cherry_core_event_device_timestamp));
	resp->total_len = data_size;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	memcpy(&resp->data[4], timestamp_message, data_size);

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 4);
		EXPECT_EQ(blk->total_len, 0);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_CORE_QUERY_UWBS_TIMESTAMP);
		EXPECT_EQ(blk->data[3], 0);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_get_device_timestamp(core_ctx,
				device_timestamp),
		  UCI_STATUS_OK);

	if (device_timestamp)
		qfree(device_timestamp);
}

/**
 * cherry_uci_client_core_set_gpio_toggle_mode() - Set mode and get the GPIO toggle timestamp.
 *
 * @core_ctx: Core context.
 * @gpio_toggle: GPIO toggle timestamp.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestCoreClient, set_gpio_toggle_modeInval)
{
	EXPECT_EQ(cherry_uci_client_core_set_gpio_toggle_mode(NULL, 0,
			gpio_toggle),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_core_set_gpio_toggle_mode(core_ctx, 0, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_core_set_gpio_toggle_mode() - toggle gpio and get timestamp.
 *
 * @core_ctx: Core context.
 * @gpio_toggle: GPIO toggle timestamp.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestCoreClient, set_gpio_toggle_timestampOk)
{
	//Set the response back to the cherry_uci_client_core
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_QORVO_EXT2,
			UCI_OID_QORVO_CORE_TOGGLE_GPIO_TIMESYNC);
	uint8_t gpio_toggle_messsage[13] = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x11, 0x22, 0x33, 0x44 };
	uint8_t data_size = sizeof(gpio_toggle_messsage);
	uint8_t mode = 0;

	struct uci_blk *resp = uci_blk_alloc(&uci, data_size, 0);
	gpio_toggle = (struct cherry_core_event_gpio_toggle *)qcalloc(
	1, sizeof(struct cherry_core_event_gpio_toggle));
	resp->total_len = data_size;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	memcpy(&resp->data[4], gpio_toggle_messsage, data_size-4);

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 5);
		EXPECT_EQ(blk->total_len, 1);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_QORVO_EXT2);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_QORVO_CORE_TOGGLE_GPIO_TIMESYNC);
		EXPECT_EQ(blk->data[3], 1);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_set_gpio_toggle_mode(core_ctx, mode,
				gpio_toggle), UCI_STATUS_OK);

	if (gpio_toggle)
		qfree(gpio_toggle);
}

/**
 * cherry_uci_client_core_set_gpio_toggle_mode() - toggle gpio and get timestamp.
 *
 * @core_ctx: Core context.
 * @gpio_toggle: GPIO toggle timestamp.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestCoreClient, set_gpio_toggle_timestampTimeout)
{
	uint8_t mode = 0;

	gpio_toggle = (struct cherry_core_event_gpio_toggle *)qcalloc(
		1, sizeof(struct cherry_core_event_gpio_toggle));

	//No response back to the cherry_uci_client_core: timeout and retry status

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 5);
		EXPECT_EQ(blk->total_len, 1);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_QORVO_EXT2);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_QORVO_CORE_TOGGLE_GPIO_TIMESYNC);
		EXPECT_EQ(blk->data[3], 1);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_set_gpio_toggle_mode(core_ctx, mode,
		gpio_toggle), UCI_STATUS_UCI_MESSAGE_RETRY);

	if (gpio_toggle)
		qfree(gpio_toggle);
}

/**
 * cherry_uci_client_core_set_gpio_toggle_mode() - toggle gpio and get timestamp.
 *
 * @core_ctx: Core context.
 * @gpio_toggle: GPIO toggle timestamp.
 *
 * Testcase_Type :
 *      Invalid response case
 **/
TEST_F(TestCoreClient, set_gpio_toggle_timestampInvalidResponse)
{
	//Set the response back to the cherry_uci_client_core with less payload
	//than expected
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_QORVO_EXT2,
			UCI_OID_QORVO_CORE_TOGGLE_GPIO_TIMESYNC);
	uint8_t gpio_toggle_messsage[5] = {
		0x00, 0x11, 0x22, 0x33, 0x44
	};
	uint8_t data_size = sizeof(gpio_toggle_messsage);
	uint8_t mode = 0;

	struct uci_blk *resp = uci_blk_alloc(&uci, data_size, 0);
	gpio_toggle = (struct cherry_core_event_gpio_toggle *)qcalloc(
	1, sizeof(struct cherry_core_event_gpio_toggle));
	resp->total_len = data_size;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	memcpy(&resp->data[4], gpio_toggle_messsage, data_size);

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 5);
		EXPECT_EQ(blk->total_len, 1);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_QORVO_EXT2);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_QORVO_CORE_TOGGLE_GPIO_TIMESYNC);
		EXPECT_EQ(blk->data[3], 1);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_set_gpio_toggle_mode(core_ctx, mode,
				gpio_toggle), UCI_STATUS_SYNTAX_ERROR);

	if (gpio_toggle)
		qfree(gpio_toggle);
}

/**
 * cherry_uci_client_core_set_gpio_toggle_mode() - toggle gpio and get timestamp.
 *
 * @core_ctx: Core context.
 * @gpio_toggle: GPIO toggle timestamp.
 *
 * Testcase_Type :
 *      Too much data case
 **/
TEST_F(TestCoreClient, set_gpio_toggle_timestampTooMuchData)
{
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_QORVO_EXT2,
			UCI_OID_QORVO_CORE_TOGGLE_GPIO_TIMESYNC);
	uint8_t gpio_toggle_messsage[20] = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0x33, 0x44
	};
	uint8_t data_size = sizeof(gpio_toggle_messsage);
	uint8_t mode = 0;

	struct uci_blk *resp = uci_blk_alloc(&uci, data_size, 0);
	gpio_toggle = (struct cherry_core_event_gpio_toggle *)qcalloc(
	1, sizeof(struct cherry_core_event_gpio_toggle));
	resp->total_len = data_size;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	memcpy(&resp->data[4], gpio_toggle_messsage, data_size);

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 5);
		EXPECT_EQ(blk->total_len, 1);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_QORVO_EXT2);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_QORVO_CORE_TOGGLE_GPIO_TIMESYNC);
		EXPECT_EQ(blk->data[3], 1);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_set_gpio_toggle_mode(core_ctx, mode,
		gpio_toggle), UCI_STATUS_OK);

	if (gpio_toggle)
		qfree(gpio_toggle);
}

class GetUwbDeviceStats : public TestCoreClient  {
    protected:
	struct cherry_core_event_device_stats stats;
};

/**
 * cherry_uci_client_core_get_uwb_device_stats() - get UWBS device stats.
 *
 * @core_ctx: Core context.
 * @stats: UWBS device stats.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(GetUwbDeviceStats, WhenNoMem_ReturnError)
{
	EXPECT_EQ(cherry_uci_client_core_get_uwb_device_stats(NULL, &stats),
		  UCI_STATUS_INVALID_PARAM);

	EXPECT_EQ(cherry_uci_client_core_get_uwb_device_stats(core_ctx, NULL),
		  UCI_STATUS_INVALID_PARAM);
}
/**
 * cherry_uci_client_core_get_uwb_device_stats() - get UWBS device stats.
 *
 * @core_ctx: Core context.
 * @stats: UWBS device stats.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(GetUwbDeviceStats, Nominal)
{
	//Set the response back to the cherry_uci_client_core
	const int uci_blk_len = 7;
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_QORVO_EXT2,
			       UCI_OID_QORVO_CORE_GET_DEVICE_STATS);
	struct uci_blk *resp = uci_blk_alloc(&uci, uci_blk_len, 0);
	resp->len = uci_blk_len;
	resp->total_len = resp->len - UCI_PACKET_HEADER_SIZE;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 4);
		EXPECT_EQ(blk->total_len, 0);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_QORVO_EXT2);
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_QORVO_CORE_GET_DEVICE_STATS);
		EXPECT_EQ(blk->data[3], 0);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_get_uwb_device_stats(core_ctx, &stats),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_core_get_uwb_device_stats() - get UWBS device stats.
 *
 * @core_ctx: Core context.
 * @stats: UWBS device stats.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(GetUwbDeviceStats, WhenTimeout_ReturnError)
{
	//No response back to the cherry_uci_client_core: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 4);
		EXPECT_EQ(blk->total_len, 0);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_QORVO_EXT2);
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_QORVO_CORE_GET_DEVICE_STATS);
		EXPECT_EQ(blk->data[3], 0);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_get_uwb_device_stats(core_ctx, &stats),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_core_get_uwb_device_stats() - get UWBS device stats.
 *
 * @core_ctx: Core context.
 * @stats: UWBS device stats.
 *
 * Testcase_Type :
 *      Invalid response case
 **/
TEST_F(GetUwbDeviceStats, WhenInvalidResponse_ReturnError)
{
	//Set the response back to the cherry_uci_client_core with less payload than
	//generic device info
	const int uci_blk_len = 5;
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_QORVO_EXT2,
			       UCI_OID_QORVO_CORE_GET_DEVICE_STATS);
	struct uci_blk *resp = uci_blk_alloc(&uci, uci_blk_len, 0);
	resp->len = uci_blk_len;
	resp->total_len = resp->len - UCI_PACKET_HEADER_SIZE;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 4);
		EXPECT_EQ(blk->total_len, 0);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_QORVO_EXT2);
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_QORVO_CORE_GET_DEVICE_STATS);
		EXPECT_EQ(blk->data[3], 0);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_get_uwb_device_stats(core_ctx, &stats),
		  UCI_STATUS_SYNTAX_ERROR);
}

/**
 * cherry_uci_client_core_get_uwb_device_stats() - get UWBS device stats.
 *
 * @core_ctx: Core context.
 * @stats: UWBS device stats.
 *
 * Testcase_Type :
 *      Too much data case
 **/
TEST_F(GetUwbDeviceStats, WhenTooMuchData_ReturnError)
{
	//Set the response back to the cherry_uci_client_core
	const int uci_blk_len = 100;
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_QORVO_EXT2,
			       UCI_OID_QORVO_CORE_GET_DEVICE_STATS);
	struct uci_blk *resp = uci_blk_alloc(&uci, uci_blk_len, 0);
	resp->len = uci_blk_len;
	resp->total_len = resp->len - UCI_PACKET_HEADER_SIZE;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 4);
		EXPECT_EQ(blk->total_len, 0);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_QORVO_EXT2);
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_QORVO_CORE_GET_DEVICE_STATS);
		EXPECT_EQ(blk->data[3], 0);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_core_get_uwb_device_stats(core_ctx, &stats),
		  UCI_STATUS_OK);
}

/**
 * uci_device_status_handler() - get UWBS device status notification.
 *
 * Testcase_Type :
 *      Nominal case
 **/
TEST_F(TestCoreClientNtf, test_nominal_ntf_device_status)
{
	//Set the notification
	uint16_t mt_gid_oid = UCI_MT_GID_OID(
			UCI_MESSAGE_TYPE_NOTIFICATION, UCI_GID_CORE,
			UCI_OID_CORE_DEVICE_STATUS);
	struct uci_blk *notif = uci_blk_alloc(&uci, UCI_PACKET_HEADER_SIZE+1, 0);
	notif->len = UCI_PACKET_HEADER_SIZE+1;
	notif->total_len = 1;
	uci_blk_put_control_header(notif, mt_gid_oid, notif->total_len);
	notif->data[4] = UCI_DEVICE_STATE_READY;

	expected_state = UCI_DEVICE_STATE_READY;
	status_cb_cnt = 0;
	transport.SendNotif(notif);

	EXPECT_EQ(status_cb_cnt, 1);
}

/**
 * uci_device_status_handler() - get UWBS device status notification.
 *
 * Testcase_Type :
 *      Invalid notification case
 **/
TEST_F(TestCoreClientNtf, test_nominal_invalid_ntf_device_status)
{
	//Set the notification
	uint16_t mt_gid_oid = UCI_MT_GID_OID(
			UCI_MESSAGE_TYPE_NOTIFICATION, UCI_GID_CORE,
			UCI_OID_CORE_DEVICE_STATUS);
	struct uci_blk *notif = uci_blk_alloc(&uci, UCI_PACKET_HEADER_SIZE, 0);
	notif->len = UCI_PACKET_HEADER_SIZE;
	notif->total_len = 0;
	uci_blk_put_control_header(notif, mt_gid_oid, notif->total_len);

	expected_state = UCI_DEVICE_STATE_ERROR;
	status_cb_cnt = 0;
	transport.SendNotif(notif);

	EXPECT_EQ(status_cb_cnt, 1);
}

/**
 * uci_device_status_handler() - get UWBS device status notification.
 *
 * Testcase_Type :
 *      No callback case
 **/
TEST_F(TestCoreClient, test_nominal_no_ntf_device_status_cb)
{
	//Set the notification
	uint16_t mt_gid_oid = UCI_MT_GID_OID(
			UCI_MESSAGE_TYPE_NOTIFICATION, UCI_GID_CORE,
			UCI_OID_CORE_DEVICE_STATUS);
	struct uci_blk *notif = uci_blk_alloc(&uci, UCI_PACKET_HEADER_SIZE, 0);
	notif->len = UCI_PACKET_HEADER_SIZE;
	notif->total_len = 0;
	uci_blk_put_control_header(notif, mt_gid_oid, notif->total_len);

	status_cb_cnt = 0;

	transport.SendNotif(notif);

	EXPECT_EQ(status_cb_cnt, 0);
}

/**
 * uci_boot_handler() - get UWBS boot notification.
 *
 * Testcase_Type :
 *      Nominal case
 **/
TEST_F(TestCoreClientNtf, test_nominal_ntf_boot)
{
	//Set the notification
	uint16_t mt_gid_oid = UCI_MT_GID_OID(
			UCI_MESSAGE_TYPE_NOTIFICATION, UCI_GID_QORVO_EXT2,
			UCI_OID_QORVO_CORE_DEVICE_BOOT);
	struct uci_blk *notif = uci_blk_alloc(&uci, UCI_PACKET_HEADER_SIZE+1, 0);
	notif->len = UCI_PACKET_HEADER_SIZE+1;
	notif->total_len = 1;
	uci_blk_put_control_header(notif, mt_gid_oid, notif->total_len);
	notif->data[4] = UCI_QORVO_BOOT_REASON_UNKNOWN;

	expected_reason = UCI_QORVO_BOOT_REASON_UNKNOWN;
	boot_cb_cnt = 0;
	transport.SendNotif(notif);

	EXPECT_EQ(boot_cb_cnt, 1);
}

/**
 * uci_boot_handler() - get UWBS boot notification.
 *
 * Testcase_Type :
 *      Invalid notification case
 **/
TEST_F(TestCoreClientNtf, test_nominal_invalid_ntf_boot)
{
	//Set the notification
	uint16_t mt_gid_oid = UCI_MT_GID_OID(
			UCI_MESSAGE_TYPE_NOTIFICATION, UCI_GID_QORVO_EXT2,
			UCI_OID_QORVO_CORE_DEVICE_BOOT);
	struct uci_blk *notif = uci_blk_alloc(&uci, UCI_PACKET_HEADER_SIZE+1, 0);
	notif->len = UCI_PACKET_HEADER_SIZE;
	notif->total_len = 0;
	uci_blk_put_control_header(notif, mt_gid_oid, notif->total_len);

	expected_reason = UCI_QORVO_BOOT_REASON_FATAL_ERROR_RESET;
	boot_cb_cnt = 0;
	transport.SendNotif(notif);

	EXPECT_EQ(boot_cb_cnt, 1);
}

/**
 * uci_boot_handler() - get UWBS boot notification.
 *
 * Testcase_Type :
 *      No callback case
 **/
TEST_F(TestCoreClient, test_nominal_no_ntf_boot_cb)
{
	//Set the notification
	uint16_t mt_gid_oid = UCI_MT_GID_OID(
			UCI_MESSAGE_TYPE_NOTIFICATION, UCI_GID_QORVO_EXT2,
			UCI_OID_QORVO_CORE_DEVICE_BOOT);
	struct uci_blk *notif = uci_blk_alloc(&uci, UCI_PACKET_HEADER_SIZE+1, 0);
	notif->len = UCI_PACKET_HEADER_SIZE;
	notif->total_len = 0;
	uci_blk_put_control_header(notif, mt_gid_oid, notif->total_len);

	boot_cb_cnt = 0;

	transport.SendNotif(notif);

	EXPECT_EQ(boot_cb_cnt, 0);
}
