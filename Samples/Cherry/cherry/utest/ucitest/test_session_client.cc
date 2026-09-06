/*
 * Implementation for session client functions
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

extern "C" {
#include "cherry_session_client.h"

#include <qmalloc.h>
#include <uci/uci.h>
#include <uci/uci_message.h>
#include <uci/uci_spec_fira.h>
#include <uci/uci_unit_converter.h>
#include <uci_internal.h>
}

#include "mock_qmalloc.hh"
#include "mock_qsemaphore.hh"
#include "mock_uci_allocator.h"
#include "mock_uci_transport.h"

#include <cstring>

using testing::InSequence;
using testing::Mock;
using testing::Return;
using testing::StrictMock;

#define UNICAST_MODE 0X00
#define NO_OF_CONTROLEES 0X01
#define TIME_SCHEDULED_RANGING 0X01
#define CHANNEL_NUMBER 0X09
#define PREAMBLE_CODE_IDX 0X0A
#define REPORT_RSSI 0x00
#define MAX_NUM_OF_MEASUREMENTS 0x00
#define MAX_RR_RETRY 0x00
#define RFRAME_SP1 0X01
#define BPRF_MODE 0X00
#define SFD_ID 0X00
#define DEFAULT_PREAMBLE_DURATION 0X01
#define STS_SEGMENTS 0X42
#define DEFAULT_PSDU_DATA_RATE 0X00
#define ENABLE_RANGE_DATA_NTF 0X01
#define DEFAULT_STS_LENGTH 0X01
#define TWO_BYTE_MAC_ADDRESS_MODE 0X00
#define STS_CONFIG 0X00
#define STS_STATIC 0X00
#define KEY_ROTATION_DEFAULT 0x00
#define KEY_ROTATION_RATE_DEFAULT 0x00
#define DATA_REPETITION_COUNT 0x00
#define DL_TDOA_RANGING_METHOD 0x01
#define DL_TDOA_TX_TIMESTAMP_CONF 0b00000011
#define DL_TDOA_HOP_COUNT 0x00
#define DL_TDOA_ANCHOR_CFO 0x01
#define DL_TDOA_TX_ACTIVE_RANGING_ROUNDS 0x00
#define DL_TDOA_BLOCK_SKIPPING 0x00
#define DL_TDOA_TIME_REFERENCE_ANCHOR 0x00
#define DL_TDOA_RESPONDER_TOF 0x00
#define DL_TDOA_ANCHOR_LOCATION_NOT_PRESENT 0x00
#define DL_TDOA_ANCHOR_LOCATION_PRESENT 0x01
#define DL_TDOA_ANCHOR_LOCATION_X 0x5A5A5A5
#define DL_TDOA_ANCHOR_LOCATION_Y 0xA5A5A5A
#define DL_TDOA_ANCHOR_LOCATION_Z 0x1A5A5A5
#define DL_TDOA_ANCHOR_LOCATION_LATITUDE 0x1A5A5A5A5
#define DL_TDOA_ANCHOR_LOCATION_LONGITUDE 0x05A5A5A5A
#define DL_TDOA_ANCHOR_LOCATION_ALTITUDE 0x25A5A5A5

#define CONTROLLER_ENABLED

#ifdef CONTROLLER_ENABLED
#define DEV_MAC_ADDRESS 0X5665
#define DEST_MAC_ADDRESS 0X1221
#else
#define DEV_MAC_ADDRESS 0X1221
#define DEST_MAC_ADDRESS 0X5665
#endif

#define NO_OF_APP 0X01
#define DATA_LENGTH 0X01

#define KEY_SIZE 16

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

static void generate_session_key(unsigned char *key, int size)
{
	int i;
	for (i = 0; i < size; i++) {
		// Generate random byte values (0-255)
		key[i] = rand() % 256;
	}
}

static struct uci_allocator simple_allocator = { .ops = &simple_allocator_ops };

static void session_status_cb(const struct session_status_ntf *ntf,
			      void *user_data)
{
}

static void ranging_ntf_cb(const struct session_ranging_data *data)
{
}

//Test suite
class TestSessionBaseClient : public ::testing::Test {
    protected:
	void SetUp() override
	{
		mock_alloc.implicit_call();
		mock_sema.implicit_call();
		ASSERT_EQ(uci_init(&uci, &simple_allocator, true),
			  QERR_SUCCESS);
		ASSERT_EQ(uci_transport_attach(&uci, &transport), QERR_SUCCESS);
		ASSERT_EQ(cherry_uci_client_session_open(&context, &uci, NULL,
							 session_status_cb,
							 ranging_ntf_cb),
			  QERR_SUCCESS);
	}
	void TearDown() override
	{
		if (expected_timeout)
			mock_sema.implicit_call();
		ASSERT_TRUE(Mock::VerifyAndClearExpectations(&transport));
		uci_transport_detach(&uci);
		cherry_uci_client_session_close(context);
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
	uint8_t device_type, device_role, range_round_usage, schedule_mode,
		channel_number, preamble_code_idx, report_rssi,
		range_data_ntf_config, sts_length, sts_config, sts_segment,
		multi_node_mode, prf_mode, psdu_data_rate, sfd_id,
		preamble_duration, rframe_config, no_of_controlees,
		data_repetition_count, dl_tdoa_ranging_method,
		dl_tdoa_tx_timestamp_conf, dl_tdoa_hop_count,
		dl_tdoa_anchor_cfo, dl_tdoa_tx_active_ranging_rounds,
		dl_tdoa_block_skipping, dl_tdoa_time_reference_anchor,
		dl_tdoa_responder_tof, dl_tdoa_anchor_location;
	int count, state;
	const void *key = NULL;
	uint8_t size = 0;
	uint16_t device_mac_address, max_number_of_measurements, max_rr_retry;
	unsigned char session_key[KEY_SIZE], sub_session_key[KEY_SIZE];
	struct cherry_session_context *context = NULL;
	StrictMock<MockUciTransport> transport;
	uint32_t session_handle = 0;
	MockQmalloc mock_alloc;
	MockQsemaphore mock_sema;
	bool expected_timeout = false;
};

class TestSessionClient : public TestSessionBaseClient {
    protected:
	void SetUp() override
	{
		TestSessionBaseClient::SetUp();
		session_init();
	}

	void TearDown() override
	{
		TestSessionBaseClient::TearDown();
	}

	void session_init()
	{
		uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
						     UCI_GID_SESSION_CONFIG,
						     UCI_OID_SESSION_INIT);
		struct uci_message_builder builder =
			UCI_MESSAGE_BUILDER_INITIALIZER(&uci);
		uci_message_put_8bit(&builder, UCI_STATUS_OK);
		uci_message_put_32bit(&builder, 1);
		uci_blk_put_control_header(builder.message, mt_gid_oid,
					   builder.message->total_len);
		transport.SetReply(builder.message);

		//Verify the Command Out
		EXPECT_CALL(transport, Out)
			.WillOnce([](struct uci_blk *blk) -> int {
				EXPECT_EQ(blk->len, 9);
				EXPECT_EQ(blk->total_len, 5);
				EXPECT_TRUE(uci_blk_has_header(blk));
				EXPECT_FALSE(uci_blk_is_segment(blk));
				uint16_t mt_gid_oid =
					uci_blk_get_mt_gid_oid(blk);
				EXPECT_EQ(UCI_MT(mt_gid_oid),
					  UCI_MESSAGE_TYPE_COMMAND);
				EXPECT_EQ(UCI_GID(mt_gid_oid),
					  UCI_GID_SESSION_CONFIG);
				EXPECT_EQ(UCI_OID(mt_gid_oid),
					  UCI_OID_SESSION_INIT);
				EXPECT_EQ(blk->data[3], 5);
				EXPECT_EQ(*(int *)&blk->data[4], 1);
				EXPECT_EQ(blk->data[8],
					  UCI_SESSION_TYPE_RANGING);
				EXPECT_FALSE(blk->next);
				return 0;
			});

		EXPECT_EQ(cherry_uci_client_session_init_session(
				  context, 1, UCI_SESSION_TYPE_RANGING,
				  &session_handle),
			  UCI_STATUS_OK);
	}
};

/**
 * cherry_uci_client_session_init_session() - Initialize a Session session.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @session_type: Session type.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionBaseClient, InitSessionNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_init_session(
			  NULL, 1, UCI_SESSION_TYPE_RANGING, &session_handle),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_init_session() - Initialize a Session session.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @session_type: Session type.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionBaseClient, InitSessionOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_INIT);
	uint32_t session_handle = 0x01234567;
	struct uci_message_builder builder =
		UCI_MESSAGE_BUILDER_INITIALIZER(&uci);
	uci_message_put_8bit(&builder, UCI_STATUS_OK);
	uci_message_put_32bit(&builder, session_handle);
	uci_blk_put_control_header(builder.message, mt_gid_oid,
				   builder.message->total_len);
	transport.SetReply(builder.message);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 9);
		EXPECT_EQ(blk->total_len, 5);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_INIT);
		EXPECT_EQ(blk->data[3], 5);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], UCI_SESSION_TYPE_RANGING);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	uint32_t ex_session_handle;
	EXPECT_EQ(cherry_uci_client_session_init_session(
			  context, 1, UCI_SESSION_TYPE_RANGING,
			  &ex_session_handle),
		  0);
	EXPECT_EQ(ex_session_handle, session_handle);
}

/**
 * cherry_uci_client_session_init_session() - Initialize a Session session.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @session_type: Session type.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionBaseClient, InitSessionTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 9);
		EXPECT_EQ(blk->total_len, 5);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_INIT);
		EXPECT_EQ(blk->data[3], 5);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], UCI_SESSION_TYPE_RANGING);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_init_session(
			  context, 1, UCI_SESSION_TYPE_RANGING,
			  &session_handle),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_channel_number() - Sets the channel number.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @channel_number: channel_number.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_channel_numberNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_channel_number(
			NULL, CHANNEL_NUMBER),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_channel_number() - Sets the channel number.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @channel_number: channel_number.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_channel_numberOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);

	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_CHANNEL_NUMBER);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App channel number config
		EXPECT_EQ(blk->data[11], CHANNEL_NUMBER);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_channel_number(
			cmd, CHANNEL_NUMBER),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_channel_number() - Sets the channel number.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @channel_number: channel_number.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_channel_numberTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_CHANNEL_NUMBER);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App channel number config
		EXPECT_EQ(blk->data[11], CHANNEL_NUMBER);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_channel_number(
			cmd, CHANNEL_NUMBER),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_preamble_code_index() - Sets preamble code index.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @preamble_code_index: preamble_code_index.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_preamble_code_indexNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_preamble_code_index(
			NULL, PREAMBLE_CODE_IDX),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_preamble_code_index() - Sets preamble code index.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @preamble_code_index: preamble_code_index.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_preamble_code_indexOk)
{
	//Set the response back to the cherry_uci_client_session:
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);

	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_PREAMBLE_CODE_INDEX);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App preamble code index config
		EXPECT_EQ(blk->data[11], PREAMBLE_CODE_IDX);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_preamble_code_index(
			cmd, PREAMBLE_CODE_IDX),
		0);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_preamble_code_index() - Sets preamble code index.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @preamble_code_index: preamble_code_index.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_preamble_code_indexTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_PREAMBLE_CODE_INDEX);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App preamble code index config
		EXPECT_EQ(blk->data[11], PREAMBLE_CODE_IDX);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_preamble_code_index(
			cmd, PREAMBLE_CODE_IDX),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_report_rssi() - Sets the report rssi.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @report_rssi: report_rssi.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_report_rssiNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_report_rssi(
			  NULL, REPORT_RSSI),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_report_rssi() - Sets the report rssi.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @report_rssi: report_rssi.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_report_rssiOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);

	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_RSSI_REPORTING);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App report rssi config
		EXPECT_EQ(blk->data[11], REPORT_RSSI);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_report_rssi(
			  cmd, REPORT_RSSI),
		  UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_get_app_config_report_rssi() - Sets the report rssi.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @report_rssi: report_rssi.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_report_rssiTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_RSSI_REPORTING);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App report rssi config
		EXPECT_EQ(blk->data[11], REPORT_RSSI);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_report_rssi(
			  cmd, REPORT_RSSI),
		  UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_max_number_of_measurements() - Sets the max number of measurements.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @max_number_of_measurements: max_number_of_measurements.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_max_number_of_measurementsNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_max_number_of_measurements(
			NULL, MAX_NUM_OF_MEASUREMENTS),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_max_number_of_measurements() - Sets the max number of measurements.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @max_number_of_measurements: max_number_of_measurements.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_max_number_of_measurementsOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);

	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 13);
		EXPECT_EQ(blk->total_len, 9);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 9);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_MAX_NUMBER_OF_MEASUREMENTS);
		EXPECT_EQ(blk->data[10], 0x02);
		//Session consortium v2.0 section 8.3 App max num of measurements config
		EXPECT_EQ(blk->data[11], 0x00);
		EXPECT_EQ(blk->data[12], 0x00);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_max_number_of_measurements(
			cmd, MAX_NUM_OF_MEASUREMENTS),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_get_app_config_max_number_of_measurements() - Sets the max number of measurements.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @max_number_of_measurements: max_number_of_measurements.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_max_number_of_measurementsTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 13);
		EXPECT_EQ(blk->total_len, 9);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 9);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_MAX_NUMBER_OF_MEASUREMENTS);
		EXPECT_EQ(blk->data[10], 0x02);
		//Session consortium v2.0 section 8.3 App max num of measurements config
		EXPECT_EQ(blk->data[11], 0x00);
		EXPECT_EQ(blk->data[12], 0x00);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_max_number_of_measurements(
			cmd, MAX_NUM_OF_MEASUREMENTS),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_max_rr_retry() - Sets the max rr retry.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @max_rr_retry: max_rr_retry.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_max_rr_retryNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_max_rr_retry(
			  NULL, MAX_RR_RETRY),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_max_rr_retry() - Sets the max rr retry.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @max_rr_retry: max_rr_retry.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_max_rr_retryOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);

	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 13);
		EXPECT_EQ(blk->total_len, 9);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 9);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_MAX_RR_RETRY);
		EXPECT_EQ(blk->data[10], 0x02);
		//Session consortium v2.0 section 8.3 App max rr retry config
		EXPECT_EQ(blk->data[11], 0x00);
		EXPECT_EQ(blk->data[12], 0x00);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_max_rr_retry(
			  cmd, MAX_RR_RETRY),
		  UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_max_rr_retry() - Sets the max rr retry.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @max_rr_retry: max_rr_retry.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_max_rr_retryTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 13);
		EXPECT_EQ(blk->total_len, 9);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 9);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_MAX_RR_RETRY);
		EXPECT_EQ(blk->data[10], 0x02);
		//Session consortium v2.0 section 8.3 App max rr retry config
		EXPECT_EQ(blk->data[11], 0x00);
		EXPECT_EQ(blk->data[12], 0x00);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_max_rr_retry(
			  cmd, MAX_RR_RETRY),
		  UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_preamble_duration() - Sets preamble duration.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @preamble_duration: preamble_duration.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_preamble_duration_NoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_preamble_duration(
			NULL, DEFAULT_PREAMBLE_DURATION),
		UCI_STATUS_INVALID_PARAM);
}

/**
 *cherry_uci_client_session_set_app_config_cmd_put_session_preamble_duration() - Sets preamble duration.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @preamble_duration: preamble duration
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_preamble_durationOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_PREAMBLE_DURATION);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App preamble duration config
		EXPECT_EQ(blk->data[11], DEFAULT_PREAMBLE_DURATION);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_preamble_duration(
			cmd, DEFAULT_PREAMBLE_DURATION),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_preamble_duration() - Sets preamble duration.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @preamble_duration: preamble duration
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_preamble_durationTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_PREAMBLE_DURATION);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App preamble duration config
		EXPECT_EQ(blk->data[11], DEFAULT_PREAMBLE_DURATION);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_preamble_duration(
			cmd, DEFAULT_PREAMBLE_DURATION),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 *
 * cherry_uci_client_session_set_app_config_cmd_put_session_prf_mode() - Sets prf mode.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @prf_mode: prf_mode pulse repetition frequency.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_prf_modeNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_prf_mode(
			  NULL, BPRF_MODE),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_prf_mode() - Sets prf mode.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @prf_mode: prf_mode pulse repetition frequency.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_prf_modeOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_PRF_MODE);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App prf mode config
		EXPECT_EQ(blk->data[11], BPRF_MODE);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_prf_mode(
			  cmd, BPRF_MODE),
		  0);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_prf_mode() - Sets prf mode.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @prf_mode: prf_mode pulse repetition frequency.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_prf_modeTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_PRF_MODE);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App prf mode config
		EXPECT_EQ(blk->data[11], BPRF_MODE);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_prf_mode(
			  cmd, BPRF_MODE),
		  UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_number_of_sts_segments() - Sets the number
 * of sts segments.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_number_of_sts_segmentsNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_number_of_sts_segments(
			NULL, STS_SEGMENTS),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_number_of_sts_segments() - Sets the number
 * of sts segments.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_number_of_sts_segmentsOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1); //Verify session id
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_NUMBER_OF_STS_SEGMENTS);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App sts segments config
		EXPECT_EQ(blk->data[11], STS_SEGMENTS);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_number_of_sts_segments(
			cmd, STS_SEGMENTS),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_number_of_sts_segments() - Sets the number
 * of sts segments.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_number_of_sts_segmentsTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1); //Verify session id
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_NUMBER_OF_STS_SEGMENTS);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App sts segments config
		EXPECT_EQ(blk->data[11], STS_SEGMENTS);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_number_of_sts_segments(
			cmd, STS_SEGMENTS),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_sfd_id() - Sets sfd_id.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @sfd_id: 0 or 2 in BPRF, 1-4 in HPRF
 *
 * Testcase_Type :
 *      Negative case
 */
TEST_F(TestSessionClient, set_session_sfd_idNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_sfd_id(
			  NULL, SFD_ID),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_sfd_id() - Sets sfd_id.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @sfd_id: 0 or 2 in BPRF, 1-4 in HPRF
 *
 * Testcase_Type :
 *      Positive case
 */
TEST_F(TestSessionClient, set_session_sfd_idOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_SFD_ID);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App sfd id config
		EXPECT_EQ(blk->data[11], SFD_ID);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_sfd_id(
			  cmd, SFD_ID),
		  0);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_sfd_id() - Sets sfd_id.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @sfd_id: 0 or 2 in BPRF, 1-4 in HPRF
 *
 * Testcase_Type :
 *      Timeout case
 */
TEST_F(TestSessionClient, set_session_sfd_idTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_SFD_ID);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App sfd id config
		EXPECT_EQ(blk->data[11], SFD_ID);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_sfd_id(
			  cmd, SFD_ID),
		  UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_device_type() - Sets the device type.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @device_type: 0 - CONTROLEE, 1 - CONTROLLER,
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_device_typeNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_device_type(
			  NULL, UCI_DEVICE_TYPE_CONTROLLER),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_device_type(
			  NULL, UCI_DEVICE_TYPE_CONTROLEE),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_device_type() - Sets the device type.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @device_type: 0 - CONTROLEE, 1 - CONTROLLER,
 *
 * Testcase_Type :
 *      Positive case
 */
TEST_F(TestSessionClient, set_session_device_typeOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 5, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_DEVICE_TYPE);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
//Session consortium v2.0 section 8.3 App device type config
#ifdef CONTROLLER_ENABLED
		EXPECT_EQ(blk->data[11], UCI_DEVICE_TYPE_CONTROLLER);
#else
		EXPECT_EQ(blk->data[11], UCI_DEVICE_TYPE_CONTROLEE);
#endif
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
#ifdef CONTROLLER_ENABLED
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_device_type(
			  cmd, UCI_DEVICE_TYPE_CONTROLLER),
		  UCI_STATUS_OK);
#else
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_device_type(
			  cmd, UCI_DEVICE_TYPE_CONTROLEE),
		  UCI_STATUS_OK);
#endif
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_device_type() - Sets the device type.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @device_type: 0 - CONTROLEE, 1 - CONTROLLER,
 *
 * Testcase_Type :
 *      Timeout case
 */
TEST_F(TestSessionClient, set_session_device_typeTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_DEVICE_TYPE);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
//Session consortium v2.0 section 8.3 App device type config
#ifdef CONTROLLER_ENABLED
		EXPECT_EQ(blk->data[11], UCI_DEVICE_TYPE_CONTROLLER);
#else
		EXPECT_EQ(blk->data[11], UCI_DEVICE_TYPE_CONTROLEE);
#endif
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
#ifdef CONTROLLER_ENABLED
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_device_type(
			  cmd, UCI_DEVICE_TYPE_CONTROLLER),
		  UCI_STATUS_OK);
#else
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_device_type(
			  cmd, UCI_DEVICE_TYPE_CONTROLEE),
		  UCI_STATUS_OK);
#endif
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_multi_node_mode() - The multi-node mode used
 * during a round.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @multi_node_mode: Multi_node_mode
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_multi_node_modeNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_multi_node_mode(
			NULL, UNICAST_MODE),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_multi_node_mode() - The multi-node mode used
 * during a round.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @multi_node_mode: Multi_node_mode
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_multi_node_modeOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_MULTI_NODE_MODE);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App multi mode config
		EXPECT_EQ(blk->data[11], UNICAST_MODE);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_multi_node_mode(
			cmd, UNICAST_MODE),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_multi_node_mode() - The multi-node mode used
 * during a round.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @multi_node_mode: Multi_node_mode
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_multi_node_modeTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_MULTI_NODE_MODE);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App multi mode config
		EXPECT_EQ(blk->data[11], UNICAST_MODE);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_multi_node_mode(
			cmd, UNICAST_MODE),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_device_role()- Sets the device role
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @device_role: Device role.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_device_roleNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_device_role(
			  NULL, UCI_DEVICE_ROLE_INITIATOR),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_device_role(
			  NULL, UCI_DEVICE_ROLE_RESPONDER),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_device_role()- Sets the device role
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @device_role: Device role.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_device_roleOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 5, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_DEVICE_ROLE);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
//Session consortium v2.0 section 8.3 App device role config
#ifdef CONTROLLER_ENABLED
		EXPECT_EQ(blk->data[11], UCI_DEVICE_ROLE_INITIATOR);
#else
		EXPECT_EQ(blk->data[11], UCI_DEVICE_ROLE_RESPONDER);
#endif
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
#ifdef CONTROLLER_ENABLED
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_device_role(
			  cmd, UCI_DEVICE_ROLE_INITIATOR),
		  0);
#else
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_device_role(
			  cmd, UCI_DEVICE_ROLE_RESPONDER),
		  0);
#endif
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_device_role()- Sets the device role
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @device_role: Device role.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_device_roleTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_DEVICE_ROLE);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
//Session consortium v2.0 section 8.3 App device role config
#ifdef CONTROLLER_ENABLED
		EXPECT_EQ(blk->data[11], UCI_DEVICE_ROLE_INITIATOR);
#else
		EXPECT_EQ(blk->data[11], UCI_DEVICE_ROLE_RESPONDER);
#endif
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
#ifdef CONTROLLER_ENABLED
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_device_role(
			  cmd, UCI_DEVICE_ROLE_INITIATOR),
		  UCI_STATUS_OK);
#else
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_device_role(
			  cmd, UCI_DEVICE_ROLE_RESPONDER),
		  UCI_STATUS_OK);
#endif
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_ranging_round_usage() - Sets ranging round usage.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @ranging_round_usage: Values
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_ranging_round_usageNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_ranging_round_usage(
			NULL, UCI_DSTWR_DEFERRED),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_ranging_round_usage() - Sets ranging round usage.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @ranging_round_usage: Values
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_ranging_round_usageOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;
	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_RANGING_ROUND_USAGE);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App ranging round config
		EXPECT_EQ(blk->data[11], UCI_DSTWR_DEFERRED);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_ranging_round_usage(
			cmd, UCI_DSTWR_DEFERRED),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_ranging_round_usage() - Sets ranging round usage.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @ranging_round_usage: Values
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_ranging_round_usageTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_RANGING_ROUND_USAGE);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App ranging round config
		EXPECT_EQ(blk->data[11], UCI_DSTWR_DEFERRED);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_ranging_round_usage(
			cmd, UCI_DSTWR_DEFERRED),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_rframe_config() - Sets rframe_config.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @rframe_config: rframe_config.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_rframe_configNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_rframe_config(
			NULL, RFRAME_SP1),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_rframe_config() - Sets rframe_config.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @rframe_config: rframe_config.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_rframe_configOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_RFRAME_CONFIG);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App rframe config
		EXPECT_EQ(blk->data[11], RFRAME_SP1);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_rframe_config(
			cmd, RFRAME_SP1),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_rframe_config() - Sets rframe_config.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @rframe_config: rframe_config.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_rframe_configTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_RFRAME_CONFIG);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App rframe config
		EXPECT_EQ(blk->data[11], RFRAME_SP1);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_rframe_config(
			cmd, RFRAME_SP1),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_short_sts_config() - Sets scrambled timestamp
 * sequence configuration.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @sts_config: Values
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_short_sts_configNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_sts_config(
			  NULL, STS_CONFIG),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_short_sts_config() - Sets scrambled timestamp
 * sequence configuration.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @sts_config: Values
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_short_sts_configOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 5, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_STS_CONFIG);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium V 2.0 section 8.3 App short sts config
		EXPECT_EQ(blk->data[11], STS_CONFIG);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_sts_config(
			  cmd, STS_CONFIG),
		  0);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_short_sts_config() - Sets scrambled timestamp
 * sequence configuration.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @sts_config: Values
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_short_sts_configTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_STS_CONFIG);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium V 2.0 section 8.3 App short sts config
		EXPECT_EQ(blk->data[11], STS_CONFIG);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_sts_config(
			  cmd, STS_CONFIG),
		  UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_key_rotation() - Enable/disable key rotation.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @key_rotation: key_rotation
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_key_rotationNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_key_rotation(
			  NULL, KEY_ROTATION_DEFAULT),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_key_rotation() - Enable/disable key rotation.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @key_rotation: key_rotation
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_key_rotationOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 5, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_KEY_ROTATION);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium V 2.0 section 8.3 App key rotation config
		EXPECT_EQ(blk->data[11], KEY_ROTATION_DEFAULT);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_key_rotation(
			  cmd, KEY_ROTATION_DEFAULT),
		  0);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_key_rotation() - Enable/disable key rotation.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @key_rotation: key_rotation
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_key_rotationTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_KEY_ROTATION);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium V 2.0 section 8.3 App key rotation config
		EXPECT_EQ(blk->data[11], KEY_ROTATION_DEFAULT);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_key_rotation(
			  cmd, KEY_ROTATION_DEFAULT),
		  UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_key_rotation_rate() - Sets key rotation rate.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @key_rotation_rate: key_rotation_rate
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_key_rotation_rateNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_key_rotation_rate(
			NULL, KEY_ROTATION_RATE_DEFAULT),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_key_rotation_rate() - Sets key rotation rate.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @key_rotation_rate: key_rotation_rate
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_key_rotation_rateOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 5, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_KEY_ROTATION_RATE);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium V 2.0 section 8.3 App key rotation rate config
		EXPECT_EQ(blk->data[11], KEY_ROTATION_RATE_DEFAULT);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_key_rotation_rate(
			cmd, KEY_ROTATION_RATE_DEFAULT),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_key_rotation_rate() - Sets key rotation rate.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @key_rotation_rate: key_rotation_rate
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_key_rotation_rateTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_KEY_ROTATION_RATE);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium V 2.0 section 8.3 App key rotation rate config
		EXPECT_EQ(blk->data[11], KEY_ROTATION_RATE_DEFAULT);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_key_rotation_rate(
			cmd, KEY_ROTATION_RATE_DEFAULT),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_deinit_session() - Deinitialize a Session session.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, DeinitSessionNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_deinit_session(NULL, 1),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_deinit_session() - Deinitialize a Session session.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, DeinitSessionOk)
{
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_DEINIT);
	struct uci_blk *resp = uci_blk_alloc(&uci, 5, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 8);
		EXPECT_EQ(blk->total_len, 4);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_DEINIT);
		EXPECT_EQ(blk->data[3], 4);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_deinit_session(context, 1), 0);
}
/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_sts_length() - Sets sts length.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @sts_length: values.
 *
 * Testcase_Type :
 *      Negative case
 **/

TEST_F(TestSessionClient, set_session_sts_lengthNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_sts_length(
			  NULL, DEFAULT_STS_LENGTH),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_sts_length() - Sets sts length.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @sts_length: values.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_sts_lengthOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 5, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_STS_LENGTH);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App sts length config
		EXPECT_EQ(blk->data[11], DEFAULT_STS_LENGTH);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_sts_length(
			  cmd, DEFAULT_STS_LENGTH),
		  UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_sts_length() - Sets sts length.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @sts_length: values.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_sts_lengthTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_STS_LENGTH);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App sts length config
		EXPECT_EQ(blk->data[11], DEFAULT_STS_LENGTH);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_sts_length(
			  cmd, DEFAULT_STS_LENGTH),
		  UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_info_ntf_config() - Sets ntf config.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @range_data_ntf_config: values.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_info_ntf_configNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_info_ntf_config(
			NULL, ENABLE_RANGE_DATA_NTF),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_info_ntf_config() - Sets ntf config.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @range_data_ntf_config: values.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_info_ntf_configOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 5, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_SESSION_INFO_NTF_CONFIG);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App range data ntf config
		EXPECT_EQ(blk->data[11], ENABLE_RANGE_DATA_NTF);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_info_ntf_config(
			cmd, ENABLE_RANGE_DATA_NTF),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_info_ntf_config() - Sets ntf config.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @range_data_ntf_config: values.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_info_ntf_configTimeout)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_SESSION_INFO_NTF_CONFIG);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App range data ntf config
		EXPECT_EQ(blk->data[11], ENABLE_RANGE_DATA_NTF);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_info_ntf_config(
			cmd, ENABLE_RANGE_DATA_NTF),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_scheduled_mode() - Sets scheduled mode parameter.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @scheduled_mode: Schedule mode.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_scheduled_modeNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_schedule_mode(
			NULL, TIME_SCHEDULED_RANGING),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_scheduled_mode() - Sets scheduled mode parameter.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @scheduled_mode: Schedule mode.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_scheduled_modeOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;
	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_SCHEDULE_MODE);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App scheduled mode config
		EXPECT_EQ(blk->data[11], TIME_SCHEDULED_RANGING);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_schedule_mode(
			cmd, TIME_SCHEDULED_RANGING),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_scheduled_mode() - Sets scheduled mode parameter.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @scheduled_mode: Schedule mode.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_scheduled_modeTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_SCHEDULE_MODE);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App scheduled mode config
		EXPECT_EQ(blk->data[11], TIME_SCHEDULED_RANGING);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_schedule_mode(
			cmd, TIME_SCHEDULED_RANGING),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_number_of_sts_segments() - Gets the number_of_sts_segments.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @sts_segments: variable to store the value.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, get_session_number_of_sts_segmentsNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_number_of_sts_segments(
			NULL, 1, &sts_segment),
		UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_number_of_sts_segments(
			context, 1, NULL),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_number_of_sts_segments() - Gets the number_of_sts_segments.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @sts_segments: variable to store the value.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, get_session_number_of_sts_segmentsOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_NUMBER_OF_STS_SEGMENTS;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = STS_SEGMENTS;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_NUMBER_OF_STS_SEGMENTS);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_number_of_sts_segments(
			context, 1, &sts_segment),
		0);
	EXPECT_EQ(STS_SEGMENTS, sts_segment);
}

/**
 * cherry_uci_client_session_get_app_config_number_of_sts_segments() - Gets the number_of_sts_segments.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @sts_segments: variable to store the value.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, get_session_number_of_sts_segmentsTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_NUMBER_OF_STS_SEGMENTS);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_number_of_sts_segments(
			context, 1, &sts_segment),
		UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_ranging_round_usage() - Gets the ranging round usage.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @ranging_round_usage: variable to store the value.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, get_session_ranging_round_usageNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_get_app_config_ranging_round_usage(
			  NULL, 1, &range_round_usage),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_app_config_ranging_round_usage(
			  context, 1, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_ranging_round_usage() - Gets the ranging round usage.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @ranging_round_usage: variable to store the value.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, get_session_ranging_round_usageOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_RANGING_ROUND_USAGE;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = TIME_SCHEDULED_RANGING;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_RANGING_ROUND_USAGE);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_ranging_round_usage(
			  context, 1, &range_round_usage),
		  0);
}

/**
 * cherry_uci_client_session_get_app_config_ranging_round_usage() - Gets the ranging round usage.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @ranging_round_usage: variable to store the value.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, get_session_ranging_round_usageTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_RANGING_ROUND_USAGE);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_ranging_round_usage(
			  context, 1, &range_round_usage),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_info_ntf_config() - Gets range notification.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @range_data_ntf_config: variable to store the value.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, get_session_info_ntf_configNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_get_app_config_info_ntf_config(
			  NULL, 1, &range_data_ntf_config),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_app_config_info_ntf_config(
			  context, 1, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_info_ntf_config() - Gets range notification.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @range_data_ntf_config: variable to store the value.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, get_session_info_ntf_configOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_SESSION_INFO_NTF_CONFIG;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = ENABLE_RANGE_DATA_NTF;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_SESSION_INFO_NTF_CONFIG);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_info_ntf_config(
			  context, 1, &range_data_ntf_config),
		  0);
}

/**
 * cherry_uci_client_session_get_app_config_info_ntf_config() - Gets range notification.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @range_data_ntf_config: variable to store the value.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, get_session_info_ntf_configTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_SESSION_INFO_NTF_CONFIG);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_info_ntf_config(
			  context, 1, &range_data_ntf_config),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_short_sts_config() - Gets the sts config.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @sts_config: variable to store the value.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, get_session_short_sts_configNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_get_app_config_short_sts_config(
			  NULL, 1, &sts_config),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_app_config_short_sts_config(
			  context, 1, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_short_sts_config() - Gets the sts config.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @sts_config: variable to store the value.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, get_session_short_sts_configOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_STS_CONFIG;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = STS_STATIC;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_STS_CONFIG);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_short_sts_config(
			  context, 1, &sts_config),
		  0);
}

/**
 * cherry_uci_client_session_get_app_config_short_sts_config() - Gets the sts config.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @sts_config: variable to store the value.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, get_session_short_sts_configTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_STS_CONFIG);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_short_sts_config(
			  context, 1, &sts_config),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_sts_length() - gets sts length.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @sts_length: variable to store the value.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, get_session_sts_lengthNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_get_app_config_sts_length(
			  NULL, 1, &sts_length),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_app_config_sts_length(context,
								      1, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_sts_length() - gets sts length.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @sts_length: variable to store the value.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, get_session_sts_lengthOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_STS_LENGTH;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = DEFAULT_STS_LENGTH;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_STS_LENGTH);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_sts_length(
			  context, 1, &sts_length),
		  0);
}

/**
 * cherry_uci_client_session_get_app_config_sts_length() - gets sts length.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @sts_length: variable to store the value.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, get_session_sts_lengthTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_STS_LENGTH);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_sts_length(
			  context, 1, &sts_length),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_psdu_data_rate() - Gets the psdu data rate.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @psdu_data_rate: psdu_data_rate.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, get_session_psdu_data_rateNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_get_app_config_psdu_data_rate(
			  NULL, 1, &psdu_data_rate),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_app_config_psdu_data_rate(
			  context, 1, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_psdu_data_rate() - Gets the psdu data rate.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @psdu_data_rate: psdu_data_rate.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, get_session_psdu_data_rateOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_PSDU_DATA_RATE;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = DEFAULT_PSDU_DATA_RATE;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_PSDU_DATA_RATE);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_psdu_data_rate(
			  context, 1, &psdu_data_rate),
		  0);
}

/**
 * cherry_uci_client_session_get_app_config_psdu_data_rate() - Gets the psdu data rate.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @psdu_data_rate: psdu_data_rate.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, get_session_psdu_data_rateTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_PSDU_DATA_RATE);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_psdu_data_rate(
			  context, 1, &psdu_data_rate),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_device_type() - Gets the device type.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @device_type: variable to store the value.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, Get_session_device_typeSessionOkNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_get_app_config_device_type(
			  NULL, 1, &device_type),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_app_config_device_type(context,
								       1, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_device_type() - Gets the device type.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @device_type: variable to store the value.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, get_session_device_typeOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_DEVICE_TYPE;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = UCI_DEVICE_TYPE_CONTROLLER;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_DEVICE_TYPE);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_device_type(
			  context, 1, &device_type),
		  0);
}

/**
 * cherry_uci_client_session_get_app_config_device_type() - Gets the device type.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @device_type: variable to store the value.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, get_session_device_typeTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_DEVICE_TYPE);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_device_type(
			  context, 1, &device_type),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_multi_node_mode() - Gets the multi node mode.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @multi_node_mode: variable to store the value.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, get_session_multi_node_modeNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_get_app_config_multi_node_mode(
			  NULL, 1, &multi_node_mode),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_app_config_multi_node_mode(
			  context, 1, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_multi_node_mode() - Gets the multi node mode.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @multi_node_mode: variable to store the value.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, get_session_multi_node_modeOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_MULTI_NODE_MODE;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = UNICAST_MODE;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_MULTI_NODE_MODE);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_multi_node_mode(
			  context, 1, &multi_node_mode),
		  0);
}

/**
 * cherry_uci_client_session_get_app_config_multi_node_mode() - Gets the multi node mode.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @multi_node_mode: variable to store the value.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, get_session_multi_node_modeTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_MULTI_NODE_MODE);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_multi_node_mode(
			  context, 1, &multi_node_mode),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_scheduled_mode() - Gets scheduled mode parameter.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @scheduled_mode: Schedule mode.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, get_session_scheduled_modeNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_get_app_config_schedule_mode(
			  NULL, 1, &schedule_mode),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_app_config_schedule_mode(
			  context, 1, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_scheduled_mode() - Gets scheduled mode parameter.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @scheduled_mode: Schedule mode.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, get_session_scheduled_modeOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_SCHEDULE_MODE;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = TIME_SCHEDULED_RANGING;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_SCHEDULE_MODE);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_schedule_mode(
			  context, 1, &schedule_mode),
		  0);
}

/**
 * cherry_uci_client_session_get_app_config_scheduled_mode() - Gets scheduled mode parameter.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @scheduled_mode: Schedule mode.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, get_session_scheduled_modeTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_SCHEDULE_MODE);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_schedule_mode(
			  context, 1, &schedule_mode),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_channel_number() - Gets the channel used in this session.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @channel_number: variable to store the value.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, get_session_channel_numberNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_get_app_config_channel_number(
			  NULL, 1, &channel_number),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_app_config_channel_number(
			  context, 1, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_channel_number() - Gets the channel used in this session.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @channel_number: variable to store the value.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, get_session_channel_numberOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_CHANNEL_NUMBER;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = CHANNEL_NUMBER;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_CHANNEL_NUMBER);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_channel_number(
			  context, 1, &channel_number),
		  0);
}

/**
 * cherry_uci_client_session_get_app_config_channel_number() - Gets the channel used in this session.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @channel_number: variable to store the value.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, get_session_channel_numberTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_CHANNEL_NUMBER);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_channel_number(
			  context, 1, &channel_number),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_preamble_code_index() - Gets preamble code index.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @preamble_code_index: variable to store the value.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, get_session_preamble_code_indexNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_get_app_config_preamble_code_index(
			  NULL, 1, &preamble_code_idx),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_app_config_preamble_code_index(
			  context, 1, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_preamble_code_index() - Gets preamble code index.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @preamble_code_index: variable to store the value.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, get_session_preamble_code_indexOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_PREAMBLE_CODE_INDEX;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = PREAMBLE_CODE_IDX;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_PREAMBLE_CODE_INDEX);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_preamble_code_index(
			  context, 1, &preamble_code_idx),
		  0);
}

/**
 * cherry_uci_client_session_get_app_config_preamble_code_index() - Gets preamble code index.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @preamble_code_index: variable to store the value.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, get_session_preamble_code_indexTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_PREAMBLE_CODE_INDEX);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_preamble_code_index(
			  context, 1, &preamble_code_idx),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_report_rssi() - Gets rssi report.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @report_rssi: variable to store the value.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, get_session_report_rssiNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_get_app_config_report_rssi(
			  NULL, 1, &report_rssi),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_app_config_report_rssi(context,
								       1, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_report_rssi() - Gets rssi report.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @preamble_code_index: variable to store the value.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, get_session_report_rssiOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_RSSI_REPORTING;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = REPORT_RSSI;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_RSSI_REPORTING);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_report_rssi(
			  context, 1, &report_rssi),
		  0);
}

/**
 * cherry_uci_client_session_get_app_config_report_rssi() - Gets rssi report.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @preamble_code_index: variable to store the value.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, get_session_report_rssiTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_RSSI_REPORTING);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_report_rssi(
			  context, 1, &report_rssi),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_max_number_of_measurements() - Gets the number of measurements.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @max_number_of_measurements: variable to store the value.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, get_session_max_number_of_measurementsNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_max_number_of_measurements(
			NULL, 1, &max_number_of_measurements),
		UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_max_number_of_measurements(
			context, 1, NULL),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_max_number_of_measurements() - Gets the number of measurements.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @max_number_of_measurements: variable to store the value.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, get_session_max_number_of_measurementsOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 10, 0);
	uint8_t data_length = 0x02;
	resp->total_len = 6;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_MAX_NUMBER_OF_MEASUREMENTS;
	resp->data[7] = data_length;
	resp->data[8] = 0x00;
	resp->data[9] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_MAX_NUMBER_OF_MEASUREMENTS);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_max_number_of_measurements(
			context, 1, &max_number_of_measurements),
		0);
}

/**
 * cherry_uci_client_session_get_app_config_max_number_of_measurements() - Gets the number of measurements.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @max_number_of_measurements: variable to store the value.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, get_session_max_number_of_measurementsTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_MAX_NUMBER_OF_MEASUREMENTS);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_max_number_of_measurements(
			context, 1, &max_number_of_measurements),
		UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_max_rr_retry() - Gets the maximum rangring rounds retry.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @max_rr_retry: variable to store the value.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, get_session_max_rr_retryNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_get_app_config_max_rr_retry(
			  NULL, 1, &max_rr_retry),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_app_config_max_rr_retry(
			  context, 1, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_max_rr_retry() - Gets the maximum rangring rounds retry.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @max_rr_retry: variable to store the value.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, get_session_max_rr_retryOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 10, 0);
	uint8_t data_length = 0x02;
	resp->total_len = 6;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_MAX_RR_RETRY;
	resp->data[7] = data_length;
	resp->data[8] = 0x00;
	resp->data[9] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_MAX_RR_RETRY);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_max_rr_retry(
			  context, 1, &max_rr_retry),
		  0);
}

/**
 * cherry_uci_client_session_get_app_config_max_rr_retry() - Gets the maximum rangring rounds retry.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @max_rr_retry: variable to store the value.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, get_session_max_rr_retryTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_MAX_RR_RETRY);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_max_rr_retry(
			  context, 1, &max_rr_retry),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_no_of_controlees() - Sets the number of controlees.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @no_of_controlees: Number of controlees.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_no_of_controleesNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_no_of_controlees(
			NULL, NO_OF_CONTROLEES),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_no_of_controlees() - Sets the number of controlees.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @no_of_controlees: Number of controlees.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_no_of_controleesOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 5, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_NUMBER_OF_CONTROLEES);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App no of controlees config
		EXPECT_EQ(blk->data[11], NO_OF_CONTROLEES);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_no_of_controlees(
			cmd, NO_OF_CONTROLEES),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_no_of_controlees() - Sets the number of controlees.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @no_of_controlees: Number of controlees.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_no_of_controleesTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_NUMBER_OF_CONTROLEES);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App no of controlees config
		EXPECT_EQ(blk->data[11], NO_OF_CONTROLEES);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_no_of_controlees(
			cmd, NO_OF_CONTROLEES),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_prf_mode() - gets the prf mode.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @prf_mode: prf_mode. pulse repetition frequency.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, get_session_prf_mode_NoContext)
{
	EXPECT_EQ(cherry_uci_client_session_get_app_config_prf_mode(NULL, 1,
								    &prf_mode),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_app_config_prf_mode(context, 1,
								    NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_prf_mode() - gets the prf mode.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @prf_mode: prf_mode. pulse repetition frequency.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, get_session_prf_modeOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_PRF_MODE;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = BPRF_MODE;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_PRF_MODE);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_prf_mode(context, 1,
								    &prf_mode),
		  0);
}

/**
 * cherry_uci_client_session_get_app_config_prf_mode() - gets the prf mode.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @prf_mode: prf_mode. pulse repetition frequency.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, get_session_prf_mode_TimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_PRF_MODE);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_prf_mode(context, 1,
								    &prf_mode),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_sfd_id() - Gets sfd_id.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @sfd_id: sfd_id. variable to store the value.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, get_session_sfd_idNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_get_app_config_sfd_id(NULL, 1,
								  &sfd_id),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_app_config_sfd_id(context, 1,
								  NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_sfd_id() - Gets sfd_id.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @sfd_id: sfd_id. variable to store the value.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, get_session_sfd_idOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_SFD_ID;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = SFD_ID;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_SFD_ID);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_sfd_id(context, 1,
								  &sfd_id),
		  0);
}

/**
 * cherry_uci_client_session_get_app_config_sfd_id() - Gets sfd_id.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @sfd_id: sfd_id. variable to store the value.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, get_session_sfd_idTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_SFD_ID);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_sfd_id(context, 1,
								  &sfd_id),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_preamble_duration() - Gets the preamble duration.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @preamble_duration: variable to store the value.
 * 0x00: 32 symbols or 0x01: 64 symbols (default).
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_preamble_duration_NoContext)
{
	EXPECT_EQ(cherry_uci_client_session_get_app_config_preamble_duration(
			  NULL, 1, &preamble_duration),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_app_config_preamble_duration(
			  context, 1, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_preamble_duration() - Gets the preamble duration.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @preamble_duration: variable to store the value.
 * 0x00: 32 symbols or 0x01: 64 symbols (default).
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_preamble_durationOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_PREAMBLE_DURATION;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = DEFAULT_PREAMBLE_DURATION;
	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_PREAMBLE_DURATION);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_preamble_duration(
			  context, 1, &preamble_duration),
		  0);
}

/**
 * cherry_uci_client_session_get_app_config_preamble_duration() - Gets the preamble duration.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @preamble_duration: variable to store the value.
 * 0x00: 32 symbols or 0x01: 64 symbols (default).
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_preamble_durationTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_PREAMBLE_DURATION);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_preamble_duration(
			  context, 1, &preamble_duration),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_device_role()- Gets the device role.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @device_role: device_role. [not implemented/used].
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_device_role_NoContext)
{
	EXPECT_EQ(cherry_uci_client_session_get_app_config_device_role(
			  NULL, 1, &device_role),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_app_config_device_role(context,
								       1, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_device_role()- Gets the device role.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @device_role: device_role. [not implemented/used].
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_device_roleOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_DEVICE_ROLE;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = UCI_DEVICE_ROLE_INITIATOR;
	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_DEVICE_ROLE);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_device_role(
			  context, 1, &device_role),
		  0);
}

/**
 * cherry_uci_client_session_get_app_config_device_role()- Gets the device role.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @device_role: device_role. [not implemented/used].
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_device_roleTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_DEVICE_ROLE);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_device_role(
			  context, 1, &device_role),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_rframe_config() - Gets the rframe_config.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @rframe_config: variable to store the value.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_rframe_config_NoContext)
{
	EXPECT_EQ(cherry_uci_client_session_get_app_config_rframe_config(
			  NULL, 1, &rframe_config),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_app_config_rframe_config(
			  context, 1, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_rframe_config() - Gets the rframe_config.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @rframe_config: variable to store the value.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_rframe_configOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_RFRAME_CONFIG;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = RFRAME_SP1;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_RFRAME_CONFIG);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_rframe_config(
			  context, 1, &rframe_config),
		  0);
}

/**
 * cherry_uci_client_session_get_app_config_rframe_config() - Gets the rframe_config.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @rframe_config: variable to store the value.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_rframe_configTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_RFRAME_CONFIG);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_rframe_config(
			  context, 1, &rframe_config),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_device_mac_address() - Gets the device mac address.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @device_mac_address: variable to store the value.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, get_session_device_mac_addressNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_get_app_config_device_mac_address(
			  NULL, 1, &device_mac_address),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_app_config_device_mac_address(
			  context, 1, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_device_mac_address() - Gets the device mac address.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @device_mac_address: variable to store the value.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, get_session_device_mac_addressOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 10, 0);
	uint8_t data_length = 0x02;
	resp->total_len = 6;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_DEVICE_MAC_ADDRESS;
	resp->data[7] = data_length;
	//2byte mac address
	resp->data[8] = 0x21;
	resp->data[9] = 0x12;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DEVICE_MAC_ADDRESS);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_device_mac_address(
			  context, 1, &device_mac_address),
		  UCI_STATUS_OK);
	EXPECT_EQ(device_mac_address, 0x1221);
}

/**
 * cherry_uci_client_session_get_app_config_device_mac_address() - Gets the device mac address.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @device_mac_address: variable to store the value.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, get_session_device_mac_addressTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DEVICE_MAC_ADDRESS);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_device_mac_address(
			  context, 1, &device_mac_address),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_device_mac_address() - Sets the device mac address.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @dev_mac_address: Device mac address.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_mac_address_modeNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_mac_address_mode(
			NULL, TWO_BYTE_MAC_ADDRESS_MODE),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_device_mac_address() - Sets the device mac address.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @dev_mac_address: Device mac address.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_mac_address_modeOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = TWO_BYTE_MAC_ADDRESS_MODE;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1); //Verify session id
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_MAC_ADDRESS_MODE);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium V 2.0 section 8.3 App mac address mode config
		EXPECT_EQ(blk->data[11], TWO_BYTE_MAC_ADDRESS_MODE);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_mac_address_mode(
			cmd, TWO_BYTE_MAC_ADDRESS_MODE),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_device_mac_address() - Sets the device mac address.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @dev_mac_address: Device mac address.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_mac_address_modeTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1); //Verify session id
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_MAC_ADDRESS_MODE);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium V 2.0 section 8.3 App mac address mode config
		EXPECT_EQ(blk->data[11], TWO_BYTE_MAC_ADDRESS_MODE);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_mac_address_mode(
			cmd, TWO_BYTE_MAC_ADDRESS_MODE),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_dest_mac_address() - Gets destination mac address.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @dst_mac_address: variable to sore the value.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, get_session_dest_mac_addressNoContext)
{
	struct dst_mac_addresses value;
	EXPECT_EQ(cherry_uci_client_session_get_app_config_dest_mac_address(
			  NULL, 1, &value),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_app_config_dest_mac_address(
			  context, 1, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_dest_mac_address() - Gets dstination mac address.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @dst_mac_address: variable to sore the value.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, get_session_dest_mac_addressOk)
{
	struct dst_mac_addresses value;
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 10, 0);
	uint8_t data_length = 0x04;
	resp->total_len = 8;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_DST_MAC_ADDRESS;
	resp->data[7] = data_length;
	//2byte mac address
	resp->data[8] = 0x65;
	resp->data[9] = 0x56;
	resp->data[10] = 0x34;
	resp->data[11] = 0x12;
	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DST_MAC_ADDRESS);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_dest_mac_address(
			  context, 1, &value),
		  0);
	EXPECT_EQ(value.n_addresses, 2);
	EXPECT_EQ(value.addresses[0], 0x5665);
	EXPECT_EQ(value.addresses[1], 0x1234);
}

TEST_F(TestSessionClient, get_session_dest_mac_addressMsgTooShort)
{
	struct dst_mac_addresses value;
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 10, 0);
	uint8_t data_length = 0x04;
	resp->total_len = 4;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_DST_MAC_ADDRESS;
	resp->data[7] = data_length;
	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DST_MAC_ADDRESS);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_dest_mac_address(
			  context, 1, &value),
		  UCI_STATUS_INVALID_MESSAGE_SIZE);
}

TEST_F(TestSessionClient, get_session_dest_mac_addressiTooManyAddresses)
{
	struct dst_mac_addresses value;
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 10, 0);
	uint8_t data_length = FIRA_CONTROLEES_MAX * 2 + 2;
	resp->total_len = 8;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_DST_MAC_ADDRESS;
	resp->data[7] = data_length;
	//2byte mac address
	resp->data[8] = 0x65;
	resp->data[9] = 0x56;
	resp->data[10] = 0x34;
	resp->data[11] = 0x12;
	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DST_MAC_ADDRESS);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_dest_mac_address(
			  context, 1, &value),
		  UCI_STATUS_INVALID_MESSAGE_SIZE);
}

TEST_F(TestSessionClient, get_session_dest_mac_addressIncorrectMsg)
{
	struct dst_mac_addresses value;
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 10, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DST_MAC_ADDRESS);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_dest_mac_address(
			  context, 1, &value),
		  UCI_STATUS_INVALID_MESSAGE_SIZE);
}

TEST_F(TestSessionClient, get_session_dest_mac_addressIncorrectNApp)
{
	struct dst_mac_addresses value;
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 10, 0);
	uint8_t data_length = 0x04;
	resp->total_len = 8;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 2;
	resp->data[6] = UCI_APPLICATION_PARAMETER_DST_MAC_ADDRESS;
	resp->data[7] = data_length;
	//2byte mac address
	resp->data[8] = 0x65;
	resp->data[9] = 0x56;
	resp->data[10] = 0x34;
	resp->data[11] = 0x12;
	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DST_MAC_ADDRESS);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_dest_mac_address(
			  context, 1, &value),
		  UCI_STATUS_INVALID_MESSAGE_SIZE);
}

/**
 * cherry_uci_client_session_get_app_config_dest_mac_address() - Gets dstination mac address.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @dst_mac_address: variable to sore the value.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, get_session_dest_mac_addressTimeOut)
{
	struct dst_mac_addresses value;
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DST_MAC_ADDRESS);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_dest_mac_address(
			  context, 1, &value),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_start_session() - Start a Session session.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, start_sessionNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_start_session(NULL, 1),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_start_session() - Start a Session session.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, start_sessionOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONTROL,
					     UCI_OID_SESSION_START);
	struct uci_blk *resp = uci_blk_alloc(&uci, 5, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 8);
		EXPECT_EQ(blk->total_len, 4);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONTROL);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_START);
		EXPECT_EQ(*(int *)&blk->data[4], 1); //Verify session id
		EXPECT_EQ(blk->data[5], 0x00);
		EXPECT_EQ(blk->data[6], 0x00);
		EXPECT_EQ(blk->data[7], 0x00);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_start_session(context, 1), 0);
}

/**
 * cherry_uci_client_session_start_session() - Start a Session session.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, start_sessionTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 8);
		EXPECT_EQ(blk->total_len, 4);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONTROL);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_START);
		EXPECT_EQ(*(int *)&blk->data[4], 1); //Verify session id
		EXPECT_EQ(blk->data[5], 0x00);
		EXPECT_EQ(blk->data[6], 0x00);
		EXPECT_EQ(blk->data[7], 0x00);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_start_session(context, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_stop_session() - Stop a Session session.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, stop_sessionNocontext)
{
	EXPECT_EQ(cherry_uci_client_session_stop_session(NULL, 1),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_stop_session() - Stop a Session session.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, stop_sessionOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONTROL,
					     UCI_OID_SESSION_STOP);
	struct uci_blk *resp = uci_blk_alloc(&uci, 5, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 8);
		EXPECT_EQ(blk->total_len, 4);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONTROL);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_STOP);
		EXPECT_EQ(*(int *)&blk->data[4], 1); //Verify session id
		EXPECT_EQ(blk->data[5], 0x00);
		EXPECT_EQ(blk->data[6], 0x00);
		EXPECT_EQ(blk->data[7], 0x00);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_stop_session(context, 1), 0);
}

/**
 * cherry_uci_client_session_stop_session() - Stop a Session session.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, stop_sessionTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 8);
		EXPECT_EQ(blk->total_len, 4);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONTROL);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_STOP);
		EXPECT_EQ(*(int *)&blk->data[4], 1); //Verify session id
		EXPECT_EQ(blk->data[5], 0x00);
		EXPECT_EQ(blk->data[6], 0x00);
		EXPECT_EQ(blk->data[7], 0x00);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_stop_session(context, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_device_mac_address() - Sets the device mac address.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @dev_mac_address: Device mac address.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_device_mac_address_NoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_device_mac_address(
			NULL, DEV_MAC_ADDRESS),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_device_mac_address() - Sets the device mac address.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @dev_mac_address: Device mac address.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_device_mac_address_controllerSessionOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 5, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 13);
		EXPECT_EQ(blk->total_len, 9);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		uint8_t data_length = 0x02;
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 9);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DEVICE_MAC_ADDRESS);
		EXPECT_EQ(blk->data[10], data_length);
#ifdef CONTROLLER_ENABLED
		EXPECT_EQ(blk->data[11], 0x65);
		EXPECT_EQ(blk->data[12], 0x56);
#else
		EXPECT_EQ(blk->data[11], 0x21);
		EXPECT_EQ(blk->data[12], 0x12);
#endif
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
#ifdef CONTROLLER_ENABLED
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_device_mac_address(
			cmd, DEV_MAC_ADDRESS),
		UCI_STATUS_OK); //For mac_address =0x5665
#else
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_device_mac_address(
			cmd, DEV_MAC_ADDRESS),
		UCI_STATUS_OK); //For mac_address = 0x1221
#endif
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_device_mac_address() - Sets the device mac address.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @dev_mac_address: Device mac address.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient,
       set_session_device_mac_address_controllerSessionTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 13);
		EXPECT_EQ(blk->total_len, 9);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		uint8_t data_length = 0x02;
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 9);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DEVICE_MAC_ADDRESS);
		EXPECT_EQ(blk->data[10], data_length);
#ifdef CONTROLLER_ENABLED
		EXPECT_EQ(blk->data[11], 0x65);
		EXPECT_EQ(blk->data[12], 0x56);
#else
		EXPECT_EQ(blk->data[11], 0x21);
		EXPECT_EQ(blk->data[12], 0x12);
#endif
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
#ifdef CONTROLLER_ENABLED
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_device_mac_address(
			cmd, DEV_MAC_ADDRESS),
		UCI_STATUS_OK); //For mac_address =0x5665
#else
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_device_mac_address(
			cmd, DEV_MAC_ADDRESS),
		UCI_STATUS_OK); //For mac_address = 0x1221
#endif
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_dest_mac_address() - Sets the destination mac address.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @dst_mac_address: Destination mac address.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_dest_mac_addressNoContext)
{
	struct dst_mac_addresses value = {};
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dest_mac_address(
			NULL, &value),
		UCI_STATUS_INVALID_PARAM);
}

TEST_F(TestSessionClient, set_session_dest_mac_addressTooManyAddresses)
{
	struct dst_mac_addresses value;
	value.n_addresses = FIRA_CONTROLEES_MAX + 1;
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dest_mac_address(
			NULL, &value),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_dest_mac_address() - Sets the destination mac address.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @dst_mac_address: Destination mac address.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_dest_mac_addressSessionOk)
{
	struct dst_mac_addresses value;
	value.n_addresses = 1;
	value.addresses[0] = DEST_MAC_ADDRESS;
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 5, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 16);
		EXPECT_EQ(blk->total_len, 12);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		uint8_t data_length = 0x02;
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 12);
		EXPECT_EQ(*(int *)&blk->data[4], 1); //Verify session id
		EXPECT_EQ(blk->data[8], 2);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_NUMBER_OF_CONTROLEES);
		EXPECT_EQ(blk->data[10], 1);
		EXPECT_EQ(blk->data[11], 1);
		EXPECT_EQ(blk->data[12],
			  UCI_APPLICATION_PARAMETER_DST_MAC_ADDRESS);
		EXPECT_EQ(blk->data[13], data_length);
#ifdef CONTROLLER_ENABLED
		//2 byte MAC address
		EXPECT_EQ(blk->data[14], 0x21);
		EXPECT_EQ(blk->data[15], 0x12);
#else
		EXPECT_EQ(blk->data[14], 0x65);
		EXPECT_EQ(blk->data[15], 0x56);
#endif
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
#ifdef CONTROLLER_ENABLED
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dest_mac_address(
			cmd, &value),
		0);
#else
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dest_mac_address(
			cmd, &value),
		0);
#endif
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_dest_mac_address() - Sets the destination mac address.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @dst_mac_address: Destination mac address.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_dest_mac_addressSessionTimeOut)
{
	struct dst_mac_addresses value;
	value.n_addresses = 1;
	value.addresses[0] = DEST_MAC_ADDRESS;
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 16);
		EXPECT_EQ(blk->total_len, 12);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		uint8_t data_length = 0x02;
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 12);
		EXPECT_EQ(*(int *)&blk->data[4], 1); //Verify session id
		EXPECT_EQ(blk->data[8], 2);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_NUMBER_OF_CONTROLEES);
		EXPECT_EQ(blk->data[10], 1);
		EXPECT_EQ(blk->data[11], 1);
		EXPECT_EQ(blk->data[12],
			  UCI_APPLICATION_PARAMETER_DST_MAC_ADDRESS);
		EXPECT_EQ(blk->data[13], data_length);
#ifdef CONTROLLER_ENABLED
		//2 byte MAC address
		EXPECT_EQ(blk->data[14], 0x21);
		EXPECT_EQ(blk->data[15], 0x12);
#else
		EXPECT_EQ(blk->data[14], 0x65);
		EXPECT_EQ(blk->data[15], 0x56);
#endif
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dest_mac_address(
			cmd, &value),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_psdu_data_rate() - Sets psdu data rate.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @psdu_data_rate:
 *      0: 6.81Mbps (default)
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_psdu_data_rateNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_psdu_data_rate(
			NULL, DEFAULT_PSDU_DATA_RATE),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_psdu_data_rate() - Sets psdu data rate.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @psdu_data_rate:
 * 	0: 6.81Mbps (default)
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_psdu_data_rateSessionOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 5, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1); //Verify session id
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_PSDU_DATA_RATE);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App psdu data rate config
		EXPECT_EQ(blk->data[11], DEFAULT_PSDU_DATA_RATE);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_psdu_data_rate(
			cmd, DEFAULT_PSDU_DATA_RATE),
		0);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_psdu_data_rate() - Sets psdu data rate.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @psdu_data_rate:
 * 	0: 6.81Mbps (default)
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_psdu_data_rateSessionTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1); //Verify session id
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_PSDU_DATA_RATE);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App psdu data rate config
		EXPECT_EQ(blk->data[11], DEFAULT_PSDU_DATA_RATE);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_psdu_data_rate(
			cmd, DEFAULT_PSDU_DATA_RATE),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_no_of_controlees() - gets the number of controlees.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @no_of_controlees: variable to store the value.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, get_session_no_of_controleesNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_get_app_config_no_of_controlees(
			  NULL, 1, &no_of_controlees),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_app_config_no_of_controlees(
			  context, 1, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_no_of_controlees() - gets the number of controlees.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @no_of_controlees: variable to store the value.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, get_session_no_of_controleesSessionOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_NUMBER_OF_CONTROLEES;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = NO_OF_CONTROLEES;
	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1); //Verify session id
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_NUMBER_OF_CONTROLEES);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_no_of_controlees(
			  context, 1, &no_of_controlees),
		  0);
}

/**
 * cherry_uci_client_session_get_app_config_no_of_controlees() - gets the number of controlees.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @no_of_controlees: variable to store the value;
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, get_session_no_of_controleesSessionTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1); //Verify session id
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_NUMBER_OF_CONTROLEES);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_no_of_controlees(
			  context, 1, &no_of_controlees),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_data_repetition_countNoContext
 *
 * Test null context results in UCI_STATUS_INVALID_PARAM error.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_data_repetition_countNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_data_repetition_count(
			NULL, DATA_REPETITION_COUNT),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_data_repetition_countOk
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_data_repetition_countOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DATA_REPETITION_COUNT);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App data repetition count config
		EXPECT_EQ(blk->data[11], DATA_REPETITION_COUNT);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_data_repetition_count(
			cmd, DATA_REPETITION_COUNT),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_data_repetition_countTimeOut
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_data_repetition_countTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DATA_REPETITION_COUNT);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App data repetition count config
		EXPECT_EQ(blk->data[11], DATA_REPETITION_COUNT);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_data_repetition_count(
			cmd, DATA_REPETITION_COUNT),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_data_repetition_countNoContext
 * Test null context or null data_repetition_count results in UCI_STATUS_INVALID_PARAM error.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_data_repetition_countNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_data_repetition_count(
			NULL, 1, &data_repetition_count),
		UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_data_repetition_count(
			context, 1, NULL),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_data_repetition_countOk
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_data_repetition_countOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_DATA_REPETITION_COUNT;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = DATA_REPETITION_COUNT;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DATA_REPETITION_COUNT);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_data_repetition_count(
			context, 1, &data_repetition_count),
		0);
}

/**
 * cherry_uci_client_session_get_app_config_data_repetition_countTimeOut
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_data_repetition_countTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DATA_REPETITION_COUNT);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_data_repetition_count(
			context, 1, &data_repetition_count),
		UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_ranging_methodNoContext
 * Test null context results in UCI_STATUS_INVALID_PARAM error.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_ranging_methodNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_ranging_method(
			NULL, DL_TDOA_RANGING_METHOD),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_ranging_methodOk
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_ranging_methodOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_RANGING_METHOD);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App data DL-TDoA ranging round based on a SS-TWR or a DS-TWR config
		EXPECT_EQ(blk->data[11], DL_TDOA_RANGING_METHOD);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_ranging_method(
			cmd, DL_TDOA_RANGING_METHOD),
		0);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_ranging_methodTimeOut
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_ranging_methodTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_RANGING_METHOD);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App data DL-TDoA ranging round based on a SS-TWR or a DS-TWR config
		EXPECT_EQ(blk->data[11], DL_TDOA_RANGING_METHOD);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_ranging_method(
			cmd, DL_TDOA_RANGING_METHOD),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_ranging_methodNoContext
 * Test null context or null dl_tdoa_ranging_method results in UCI_STATUS_INVALID_PARAM error.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_ranging_methodNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_ranging_method(
			NULL, 1, &dl_tdoa_ranging_method),
		UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_ranging_method(
			context, 1, NULL),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_ranging_methodOk
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_ranging_methodOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_DL_TDOA_RANGING_METHOD;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = DL_TDOA_RANGING_METHOD;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_RANGING_METHOD);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_ranging_method(
			context, 1, &dl_tdoa_ranging_method),
		0);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_ranging_methodTimeOut
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_ranging_methodTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_RANGING_METHOD);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_ranging_method(
			context, 1, &dl_tdoa_ranging_method),
		UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_tx_timestamp_confNoContext
 * Test null context results in UCI_STATUS_INVALID_PARAM error.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_tx_timestamp_confNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_tx_timestamp_conf(
			NULL, DL_TDOA_TX_TIMESTAMP_CONF),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_tx_timestamp_confOk
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_tx_timestamp_confOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_TX_TIMESTAMP_CONF);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App data type and length of TX timestamps config
		EXPECT_EQ(blk->data[11], DL_TDOA_TX_TIMESTAMP_CONF);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_tx_timestamp_conf(
			cmd, DL_TDOA_TX_TIMESTAMP_CONF),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_tx_timestamp_confTimeOut
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_tx_timestamp_confTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_TX_TIMESTAMP_CONF);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App data type and length of TX timestamps config
		EXPECT_EQ(blk->data[11], DL_TDOA_TX_TIMESTAMP_CONF);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_tx_timestamp_conf(
			cmd, DL_TDOA_TX_TIMESTAMP_CONF),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_tx_timestamp_confNoContext
 * Test null context or null dl_tdoa_tx_timestamp_conf results in UCI_STATUS_INVALID_PARAM error.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_tx_timestamp_confNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_tx_timestamp_conf(
			NULL, 1, &dl_tdoa_tx_timestamp_conf),
		UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_tx_timestamp_conf(
			context, 1, NULL),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_tx_timestamp_confOk
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_tx_timestamp_confOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_DL_TDOA_TX_TIMESTAMP_CONF;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = DL_TDOA_TX_TIMESTAMP_CONF;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_TX_TIMESTAMP_CONF);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_tx_timestamp_conf(
			context, 1, &dl_tdoa_tx_timestamp_conf),
		0);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_tx_timestamp_confTimeout
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_tx_timestamp_confTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_TX_TIMESTAMP_CONF);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_tx_timestamp_conf(
			context, 1, &dl_tdoa_tx_timestamp_conf),
		UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_hop_countNoContext
 * Test null context results in UCI_STATUS_INVALID_PARAM error.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_hop_countNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_hop_count(
			NULL, DL_TDOA_HOP_COUNT),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_hop_countOk
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_hop_countOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_HOP_COUNT);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App data Hop Count field presence config
		EXPECT_EQ(blk->data[11], DL_TDOA_HOP_COUNT);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_hop_count(
			cmd, DL_TDOA_HOP_COUNT),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_hop_countTimeOut
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_hop_countTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_HOP_COUNT);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App data Hop Count field presence config
		EXPECT_EQ(blk->data[11], DL_TDOA_HOP_COUNT);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_hop_count(
			cmd, DL_TDOA_HOP_COUNT),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_hop_countNoContext
 * Test null context or null dl_tdoa_hop_count results in UCI_STATUS_INVALID_PARAM error.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_hop_countNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_get_app_config_dl_tdoa_hop_count(
			  NULL, 1, &dl_tdoa_hop_count),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_app_config_dl_tdoa_hop_count(
			  context, 1, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_hop_countOk
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_hop_countOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_DL_TDOA_HOP_COUNT;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = DL_TDOA_HOP_COUNT;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_HOP_COUNT);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_dl_tdoa_hop_count(
			  context, 1, &dl_tdoa_hop_count),
		  0);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_hop_countTimeOut
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_hop_countTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_HOP_COUNT);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_dl_tdoa_hop_count(
			  context, 1, &dl_tdoa_hop_count),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_cfoNoContext
 * Test null context results in UCI_STATUS_INVALID_PARAM error.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_cfoNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_cfo(
			NULL, DL_TDOA_ANCHOR_CFO),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_cfoOk
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_cfoOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_ANCHOR_CFO);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App data Anchor CFO presence config
		EXPECT_EQ(blk->data[11], DL_TDOA_ANCHOR_CFO);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_cfo(
			cmd, DL_TDOA_ANCHOR_CFO),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_cfoTimeOut
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_cfoTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_ANCHOR_CFO);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App data Anchor CFO presence config
		EXPECT_EQ(blk->data[11], DL_TDOA_ANCHOR_CFO);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_cfo(
			cmd, DL_TDOA_ANCHOR_CFO),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_anchor_cfoNoContext
 * Test null context or null dl_tdoa_anchor_cfo results in UCI_STATUS_INVALID_PARAM error.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_anchor_cfoNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_get_app_config_dl_tdoa_anchor_cfo(
			  NULL, 1, &dl_tdoa_anchor_cfo),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_app_config_dl_tdoa_anchor_cfo(
			  context, 1, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_anchor_cfoOk
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_anchor_cfoOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_DL_TDOA_ANCHOR_CFO;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = DL_TDOA_ANCHOR_CFO;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_ANCHOR_CFO);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_dl_tdoa_anchor_cfo(
			  context, 1, &dl_tdoa_anchor_cfo),
		  0);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_anchor_cfoTimeOut
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_anchor_cfoTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_ANCHOR_CFO);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_app_config_dl_tdoa_anchor_cfo(
			  context, 1, &dl_tdoa_anchor_cfo),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tx_active_ranging_roundsNoContext
 * Test null context results in UCI_STATUS_INVALID_PARAM error.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tx_active_ranging_roundsNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tx_active_ranging_rounds(
			NULL, DL_TDOA_TX_ACTIVE_RANGING_ROUNDS),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tx_active_ranging_roundsOk
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tx_active_ranging_roundsOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(
			blk->data[9],
			UCI_APPLICATION_PARAMETER_DL_TDOA_TX_ACTIVE_RANGING_ROUNDS);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App data active ranging round information presence config
		EXPECT_EQ(blk->data[11], DL_TDOA_TX_ACTIVE_RANGING_ROUNDS);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tx_active_ranging_rounds(
			cmd, DL_TDOA_TX_ACTIVE_RANGING_ROUNDS),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tx_active_ranging_roundsTimeOut
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tx_active_ranging_roundsTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(
			blk->data[9],
			UCI_APPLICATION_PARAMETER_DL_TDOA_TX_ACTIVE_RANGING_ROUNDS);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App data active ranging round information presence config
		EXPECT_EQ(blk->data[11], DL_TDOA_TX_ACTIVE_RANGING_ROUNDS);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tx_active_ranging_rounds(
			cmd, DL_TDOA_TX_ACTIVE_RANGING_ROUNDS),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tx_active_ranging_roundsNoContext
 * Test null context or null dl_tdoa_tx_active_ranging_rounds results in UCI_STATUS_INVALID_PARAM error.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tx_active_ranging_roundsNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tx_active_ranging_rounds(
			NULL, 1, &dl_tdoa_tx_active_ranging_rounds),
		UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tx_active_ranging_rounds(
			context, 1, NULL),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tx_active_ranging_roundsOk
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tx_active_ranging_roundsOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] =
		UCI_APPLICATION_PARAMETER_DL_TDOA_TX_ACTIVE_RANGING_ROUNDS;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = DL_TDOA_TX_ACTIVE_RANGING_ROUNDS;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(
			blk->data[9],
			UCI_APPLICATION_PARAMETER_DL_TDOA_TX_ACTIVE_RANGING_ROUNDS);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tx_active_ranging_rounds(
			context, 1, &dl_tdoa_tx_active_ranging_rounds),
		0);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tx_active_ranging_roundsTimeOut
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tx_active_ranging_roundsTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(
			blk->data[9],
			UCI_APPLICATION_PARAMETER_DL_TDOA_TX_ACTIVE_RANGING_ROUNDS);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tx_active_ranging_rounds(
			context, 1, &dl_tdoa_tx_active_ranging_rounds),
		UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_block_skippingNoContext
 * Test null context results in UCI_STATUS_INVALID_PARAM error.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_block_skippingNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_block_skipping(
			NULL, DL_TDOA_BLOCK_SKIPPING),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_block_skippingOk
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_block_skippingOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_BLOCK_SKIPPING);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App data block skipping performed config
		EXPECT_EQ(blk->data[11], DL_TDOA_BLOCK_SKIPPING);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_block_skipping(
			cmd, DL_TDOA_BLOCK_SKIPPING),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_block_skippingTimeOut
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_block_skippingTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_BLOCK_SKIPPING);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App data block skipping performed config
		EXPECT_EQ(blk->data[11], DL_TDOA_BLOCK_SKIPPING);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_block_skipping(
			cmd, DL_TDOA_BLOCK_SKIPPING),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_block_skippingNoContext
 * Test null context or null dl_tdoa_block_skipping results in UCI_STATUS_INVALID_PARAM error.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_block_skippingNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_block_skipping(
			NULL, 1, &dl_tdoa_block_skipping),
		UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_block_skipping(
			context, 1, NULL),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_block_skippingOk
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_block_skippingOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_DL_TDOA_BLOCK_SKIPPING;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = DL_TDOA_BLOCK_SKIPPING;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_BLOCK_SKIPPING);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_block_skipping(
			context, 1, &dl_tdoa_block_skipping),
		0);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_block_skippingTimeOut
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_block_skippingTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_BLOCK_SKIPPING);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_block_skipping(
			context, 1, &dl_tdoa_block_skipping),
		UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_time_reference_anchorNoContext
 * Test null context results in UCI_STATUS_INVALID_PARAM error.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_time_reference_anchorNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_time_reference_anchor(
			NULL, DL_TDOA_TIME_REFERENCE_ANCHOR),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_time_reference_anchorOk
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_time_reference_anchorOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(
			blk->data[9],
			UCI_APPLICATION_PARAMETER_DL_TDOA_TIME_REFERENCE_ANCHOR);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App data DT-Anchor time reference choice
		EXPECT_EQ(blk->data[11], DL_TDOA_TIME_REFERENCE_ANCHOR);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_time_reference_anchor(
			cmd, DL_TDOA_TIME_REFERENCE_ANCHOR),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_time_reference_anchorTimeOut
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_time_reference_anchorTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(
			blk->data[9],
			UCI_APPLICATION_PARAMETER_DL_TDOA_TIME_REFERENCE_ANCHOR);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App data DT-Anchor time reference choice
		EXPECT_EQ(blk->data[11], DL_TDOA_TIME_REFERENCE_ANCHOR);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_time_reference_anchor(
			cmd, DL_TDOA_TIME_REFERENCE_ANCHOR),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_time_reference_anchorNoContext
 * Test null context or null dl_tdoa_time_reference_anchor results in UCI_STATUS_INVALID_PARAM error.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_time_reference_anchorNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_time_reference_anchor(
			NULL, 1, &dl_tdoa_time_reference_anchor),
		UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_time_reference_anchor(
			context, 1, NULL),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_time_reference_anchorOk
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_time_reference_anchorOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_DL_TDOA_TIME_REFERENCE_ANCHOR;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = DL_TDOA_TIME_REFERENCE_ANCHOR;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(
			blk->data[9],
			UCI_APPLICATION_PARAMETER_DL_TDOA_TIME_REFERENCE_ANCHOR);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_time_reference_anchor(
			context, 1, &dl_tdoa_time_reference_anchor),
		0);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_time_reference_anchorTimeOut
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_time_reference_anchorTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(
			blk->data[9],
			UCI_APPLICATION_PARAMETER_DL_TDOA_TIME_REFERENCE_ANCHOR);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_time_reference_anchor(
			context, 1, &dl_tdoa_time_reference_anchor),
		UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_responder_tofNoContext
 * Test null context results in UCI_STATUS_INVALID_PARAM error.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_responder_tofNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_responder_tof(
			NULL, DL_TDOA_RESPONDER_TOF),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_responder_tofOk
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_responder_tofOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_RESPONDER_TOF);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App data Responder ToF Result in DTMs
		EXPECT_EQ(blk->data[11], DL_TDOA_RESPONDER_TOF);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_responder_tof(
			cmd, DL_TDOA_RESPONDER_TOF),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_responder_tofTimeOut
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_responder_tofTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_RESPONDER_TOF);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App data Responder ToF Result in DTMs
		EXPECT_EQ(blk->data[11], DL_TDOA_RESPONDER_TOF);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_responder_tof(
			cmd, DL_TDOA_RESPONDER_TOF),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_responder_tofNoContext
 * Test null context or null dl_tdoa_responder_tof results in UCI_STATUS_INVALID_PARAM error.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_responder_tofNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_responder_tof(
			NULL, 1, &dl_tdoa_responder_tof),
		UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_responder_tof(
			context, 1, NULL),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_responder_tofOk
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_responder_tofOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_DL_TDOA_RESPONDER_TOF;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = DL_TDOA_RESPONDER_TOF;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_RESPONDER_TOF);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_responder_tof(
			context, 1, &dl_tdoa_responder_tof),
		0);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_responder_tofTimeOut
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_responder_tofTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_RESPONDER_TOF);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_responder_tof(
			context, 1, &dl_tdoa_responder_tof),
		UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_key() - Sets this key for the session.
 *
 * @context: The client context.
 * @session_id: id of the session to modify.
 * @session_key: key of the session to modify.
 * @size: length of the session key, can be 128 or 256 bits.
 *
 * Testcase_Type :
 *      Negative case
 */
TEST_F(TestSessionClient, set_session_keyNoContext)
{
	srand(time(NULL));
	generate_session_key(session_key, KEY_SIZE);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_session_key(
			  NULL, session_key, sizeof(session_key)),
		  UCI_STATUS_INVALID_PARAM);
}

TEST_F(TestSessionClient, set_session_keyNoSessionKey)
{
	char *no_session_key = NULL;
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_session_key(
			  NULL, no_session_key, sizeof(no_session_key)),
		  UCI_STATUS_INVALID_PARAM);
}

TEST_F(TestSessionClient, set_session_keyNoContextNoSessionKey)
{
	char *no_session_key = NULL;
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_session_key(
			  NULL, no_session_key, sizeof(no_session_key)),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_key() - Sets this key for the session.
 *
 * @context: The client context.
 * @session_id: id of the session to modify.
 * @session_key: key of the session to modify.
 * @size: length of the session key, can be 128 or 256 bits.
 *
 * Testcase_Type :
 *      Timeout case
 */
TEST_F(TestSessionClient, set_session_keyTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		uint8_t no_of_app = 0x01;
		EXPECT_EQ(blk->len, 11 + sizeof(session_key));
		EXPECT_EQ(blk->total_len, 7 + sizeof(session_key));
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 7 + sizeof(session_key));
		EXPECT_EQ(*(int *)&blk->data[4], 1); //Verify session id
		EXPECT_EQ(blk->data[8], no_of_app);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_SESSION_KEY);
		EXPECT_EQ(blk->data[10], sizeof(session_key));
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_session_key(
			  cmd, session_key, sizeof(session_key)),
		  UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_key() - Sets this key for the session.
 *
 * @context: The client context.
 * @session_id: id of the session to modify.
 * @session_key: key of the session to modify.
 * @size: length of the session key, can be 128 or 256 bits.
 *
 * Testcase_Type :
 *      Positive case
 */
TEST_F(TestSessionClient, set_session_keyOk)
{
	srand(time(NULL));
	generate_session_key(session_key, KEY_SIZE);

	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 5, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		uint8_t no_of_app = 0x01;
		EXPECT_EQ(blk->len, 11 + sizeof(session_key));
		EXPECT_EQ(blk->total_len, 7 + sizeof(session_key));
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 7 + sizeof(session_key));
		EXPECT_EQ(*(int *)&blk->data[4], 1); //Verify session id
		EXPECT_EQ(blk->data[8], no_of_app);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_SESSION_KEY);
		EXPECT_EQ(blk->data[10], sizeof(session_key));
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_session_key(
			  cmd, session_key, sizeof(session_key)),
		  UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_sub_session_key() - Sets this key for the sub session.
 *
 * @context: The client context.
 * @sub_session_id: id of the sub session to modify.
 * @session_key: key of the sub session to modify.
 * @size: length of the sub session key, can be 128 or 256 bits.
 *
 * Testcase_Type :
 *      Negative case
 */
TEST_F(TestSessionClient, set_sub_session_keyNoContext)
{
	srand(time(NULL));
	generate_session_key(sub_session_key, KEY_SIZE);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_sub_session_key(
			NULL, sub_session_key, sizeof(sub_session_key)),
		UCI_STATUS_INVALID_PARAM);
}

TEST_F(TestSessionClient, set_sub_session_keyNoSubSessionKey)
{
	char *no_sub_session_key = NULL;
	struct cherry_uci_client_session_set_app_config_cmd *cmd =
		(struct cherry_uci_client_session_set_app_config_cmd
			 *)0xDEADBEEF;
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_sub_session_key(
			cmd, no_sub_session_key, sizeof(no_sub_session_key)),
		UCI_STATUS_INVALID_PARAM);
}

TEST_F(TestSessionClient, set_sub_session_keyNoContextNoSubSessionKey)
{
	char *no_sub_session_key = NULL;
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_sub_session_key(
			NULL, no_sub_session_key, sizeof(no_sub_session_key)),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_sub_session_key() - Sets this key for the sub session.
 *
 * @context: The client context.
 * @sub_session_id: id of the sub session to modify.
 * @session_key: key of the sub session to modify.
 * @size: length of the sub session key, can be 128 or 256 bits.
 *
 * Testcase_Type :
 *      Timeout case
 */
TEST_F(TestSessionClient, set_sub_session_keyTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		uint8_t no_of_app = 0x01;
		EXPECT_EQ(blk->len, 11 + sizeof(sub_session_key));
		EXPECT_EQ(blk->total_len, 7 + sizeof(sub_session_key));
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 7 + sizeof(sub_session_key));
		EXPECT_EQ(*(int *)&blk->data[4], 1); //Verify session id
		EXPECT_EQ(blk->data[8], no_of_app);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_SUB_SESSION_KEY);
		EXPECT_EQ(blk->data[10], sizeof(sub_session_key));
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_sub_session_key(
			cmd, sub_session_key, sizeof(sub_session_key)),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_sub_session_key() - Sets this key for the sub session.
 *
 * @context: The client context.
 * @sub_session_id: id of the sub session to modify.
 * @session_key: key of the sub session to modify.
 * @size: length of the sub session key, can be 128 or 256 bits.
 *
 * Testcase_Type :
 *      Positive case
 */
TEST_F(TestSessionClient, set_sub_session_keyOk)
{
	srand(time(NULL));
	generate_session_key(sub_session_key, KEY_SIZE);

	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 5, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	transport.SetReply(resp);

	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		uint8_t no_of_app = 0x01;
		EXPECT_EQ(blk->len, 11 + sizeof(sub_session_key));
		EXPECT_EQ(blk->total_len, 7 + sizeof(sub_session_key));
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 7 + sizeof(sub_session_key));
		EXPECT_EQ(*(int *)&blk->data[4], 1); //Verify session id
		EXPECT_EQ(blk->data[8], no_of_app);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_SUB_SESSION_KEY);
		EXPECT_EQ(blk->data[10], sizeof(sub_session_key));
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_sub_session_key(
			cmd, sub_session_key, sizeof(sub_session_key)),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_locationNoContext
 * Test null context results in UCI_STATUS_INVALID_PARAM error.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_locationNoContext)
{
	struct cherry_fira_anchor_location anchor_location;

	anchor_location.type = CHERRY_FIRA_ANCHOR_LOCATION_TYPE_NONE;

	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_location(
			NULL, &anchor_location),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_locationNoVariable
 * Test null on variable to set Anchor Location in UCI_STATUS_INVALID_PARAM error.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_locationNoVariable)
{
	struct cherry_uci_client_session_set_app_config_cmd *cmd =
		(struct cherry_uci_client_session_set_app_config_cmd
			 *)0xDEADBEEF;
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_location(
			cmd, NULL),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_locationOk
 * Verify command output in case anchor location not present
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_locationOk)
{
	struct cherry_fira_anchor_location anchor_location;

	anchor_location.type = CHERRY_FIRA_ANCHOR_LOCATION_TYPE_NONE;

	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_ANCHOR_LOCATION);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App data DL_TDOA_ANCHOR_LOCATION

		EXPECT_EQ(blk->data[11], DL_TDOA_ANCHOR_LOCATION_NOT_PRESENT);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_location(
			cmd, &anchor_location),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_locationTimeOut
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_locationTimeOut)
{
	struct cherry_fira_anchor_location anchor_location;

	anchor_location.type = CHERRY_FIRA_ANCHOR_LOCATION_TYPE_NONE;

	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_ANCHOR_LOCATION);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		//Session consortium v2.0 section 8.3 App data Responder ToF Result in DTMs
		EXPECT_EQ(blk->data[11], DL_TDOA_ANCHOR_LOCATION_NOT_PRESENT);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_location(
			cmd, &anchor_location),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_locationWGS84Ok
 * Verify command output in case anchor location WGS-84
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_locationWGS84Ok)
{
	struct cherry_fira_anchor_location anchor_location;

	anchor_location.type = CHERRY_FIRA_ANCHOR_LOCATION_TYPE_WGS84;
	anchor_location.data.wgs84.latitude = 1;
	anchor_location.data.wgs84.longitude =
		DL_TDOA_ANCHOR_LOCATION_LONGITUDE;
	anchor_location.data.wgs84.altitude = DL_TDOA_ANCHOR_LOCATION_ALTITUDE;

	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 24);
		EXPECT_EQ(blk->total_len, 20);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 20);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_ANCHOR_LOCATION);
		EXPECT_EQ(blk->data[10], 13); //length
		//Session consortium v2.0 section 8.3 App data DL_TDOA_ANCHOR_LOCATION

		EXPECT_EQ(blk->data[11],
			  FIRA_DT_LOCATION_COORD_WGS84 << 1 |
				  DL_TDOA_ANCHOR_LOCATION_PRESENT);
		EXPECT_EQ(blk->data[12], 0x01);
		EXPECT_EQ(blk->data[13], 0x00);
		EXPECT_EQ(blk->data[14], 0x00);
		EXPECT_EQ(blk->data[15], 0x00);
		EXPECT_EQ(blk->data[16], 0x2D); // du to << 1
		EXPECT_EQ(blk->data[17], 0x2D);
		EXPECT_EQ(blk->data[18], 0x2D);
		EXPECT_EQ(blk->data[19], 0x2D);
		EXPECT_EQ(blk->data[20], 0x29); // du to << 2
		EXPECT_EQ(blk->data[21], 0x69);
		EXPECT_EQ(blk->data[22], 0x69);
		EXPECT_EQ(blk->data[23], 0x65);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_location(
			cmd, &anchor_location),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_locationRelativeOk
 * Verify command output in case anchor location relative
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_locationRelativeOk)
{
	struct cherry_fira_anchor_location anchor_location;

	anchor_location.type = CHERRY_FIRA_ANCHOR_LOCATION_TYPE_REL;
	anchor_location.data.relative.x = 1;
	anchor_location.data.relative.y = DL_TDOA_ANCHOR_LOCATION_LONGITUDE;
	anchor_location.data.relative.z = DL_TDOA_ANCHOR_LOCATION_ALTITUDE;

	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 22);
		EXPECT_EQ(blk->total_len, 18);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 18);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_ANCHOR_LOCATION);
		EXPECT_EQ(blk->data[10], 11); //length
		//Session consortium v2.0 section 8.3 App data DL_TDOA_ANCHOR_LOCATION

		EXPECT_EQ(blk->data[11],
			  FIRA_DT_LOCATION_COORD_RELATIVE << 1 |
				  DL_TDOA_ANCHOR_LOCATION_PRESENT);
		EXPECT_EQ(blk->data[12], 0x01);
		EXPECT_EQ(blk->data[13], 0x00);
		EXPECT_EQ(blk->data[14], 0x00);
		EXPECT_EQ(blk->data[15], 0x05);
		EXPECT_EQ(blk->data[16], 0xA5);
		EXPECT_EQ(blk->data[17], 0xA5);
		EXPECT_EQ(blk->data[18], 0xAA);
		EXPECT_EQ(blk->data[19], 0xA5);
		EXPECT_EQ(blk->data[20], 0xA5);
		EXPECT_EQ(blk->data[21], 0xA5);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_location(
			cmd, &anchor_location),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_anchor_locationNoContext
 * Test null context or null dl_tdoa_anchor_location results in UCI_STATUS_INVALID_PARAM error.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_anchor_locationNoContext)
{
	struct dl_tdoa_anchor_location anchor_location;

	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_anchor_location(
			NULL, 1, &anchor_location),
		UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_anchor_location(
			context, 1, NULL),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_anchor_location_defaultOk
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_anchor_location_defaultOk)
{
	struct dl_tdoa_anchor_location anchor_location;

	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 5;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_DL_TDOA_ANCHOR_LOCATION;
	resp->data[7] = DATA_LENGTH;
	resp->data[8] = DL_TDOA_ANCHOR_LOCATION_NOT_PRESENT;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_ANCHOR_LOCATION);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_anchor_location(
			context, 1, &anchor_location),
		UCI_STATUS_OK);
	EXPECT_EQ(anchor_location.location_presence,
		  DL_TDOA_ANCHOR_LOCATION_NOT_PRESENT);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_anchor_location_WSG84Ok
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_anchor_location_WSG84Ok)
{
	struct dl_tdoa_anchor_location anchor_location;

	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 21, 0);
	uint8_t data_length = 13;
	resp->total_len = 17;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_DL_TDOA_ANCHOR_LOCATION;
	resp->data[7] = data_length;
	resp->data[8] = FIRA_DT_LOCATION_COORD_WGS84 << 1 |
			DL_TDOA_ANCHOR_LOCATION_PRESENT;
	resp->data[9] = 0x01;
	resp->data[10] = 0x02;
	resp->data[11] = 0x03;
	resp->data[12] = 0x04;
	resp->data[13] = 0x05;
	resp->data[14] = 0x06;
	resp->data[15] = 0x07;
	resp->data[16] = 0x08;
	resp->data[17] = 0x09;
	resp->data[18] = 0x0A;
	resp->data[19] = 0x0B;
	resp->data[20] = 0x0C;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_ANCHOR_LOCATION);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_anchor_location(
			context, 1, &anchor_location),
		UCI_STATUS_OK);
	EXPECT_EQ(anchor_location.location_presence,
		  DL_TDOA_ANCHOR_LOCATION_PRESENT);
	EXPECT_EQ(anchor_location.coordinates_system,
		  FIRA_DT_LOCATION_COORD_WGS84);
	EXPECT_EQ(anchor_location.location_x, 0x4030201);
	EXPECT_EQ(anchor_location.location_y, 0x100E0C0A);
	EXPECT_EQ(anchor_location.location_z, 0xC2C2824);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_anchor_location_relativeOk
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_anchor_location_relativeOk)
{
	struct dl_tdoa_anchor_location anchor_location;

	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 21, 0);
	uint8_t data_length = 13;
	resp->total_len = 17;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_DL_TDOA_ANCHOR_LOCATION;
	resp->data[7] = data_length;
	resp->data[8] = FIRA_DT_LOCATION_COORD_RELATIVE << 1 |
			DL_TDOA_ANCHOR_LOCATION_PRESENT;
	resp->data[9] = 0x01;
	resp->data[10] = 0x02;
	resp->data[11] = 0x03;
	resp->data[12] = 0x04;
	resp->data[13] = 0x05;
	resp->data[14] = 0x06;
	resp->data[15] = 0x07;
	resp->data[16] = 0x08;
	resp->data[17] = 0x09;
	resp->data[18] = 0x0A;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_ANCHOR_LOCATION);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_anchor_location(
			context, 1, &anchor_location),
		UCI_STATUS_OK);
	EXPECT_EQ(anchor_location.location_presence,
		  DL_TDOA_ANCHOR_LOCATION_PRESENT);
	EXPECT_EQ(anchor_location.coordinates_system,
		  FIRA_DT_LOCATION_COORD_RELATIVE);
	EXPECT_EQ(anchor_location.location_x, 0x30201);
	EXPECT_EQ(anchor_location.location_y, 0x7605040);
	EXPECT_EQ(anchor_location.location_z, 0xA0908);
}
/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_anchor_locationTimeout
 * Verify command output.
 **/
TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_dl_tdoa_anchor_locationTimeOut)
{
	struct dl_tdoa_anchor_location anchor_location;

	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DL_TDOA_ANCHOR_LOCATION);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(
		cherry_uci_client_session_get_app_config_dl_tdoa_anchor_location(
			context, 1, &anchor_location),
		UCI_STATUS_UCI_MESSAGE_RETRY);
}

TEST_F(TestSessionClient, cherry_uci_client_session_get_app_config_vendor_idOK)
{
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	uint8_t vendor_id[2];
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 6;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 1;
	resp->data[6] = UCI_APPLICATION_PARAMETER_VENDOR_ID;
	resp->data[7] = 2;
	resp->data[8] = 0x21;
	resp->data[9] = 0x12;
	transport.SetReply(resp);

	EXPECT_CALL(transport, Out).WillOnce([&](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], 1);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_VENDOR_ID);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_vendor_id(context, 1,
								     vendor_id),
		  0);
	EXPECT_EQ(vendor_id[0], 0x21);
	EXPECT_EQ(vendor_id[1], 0x12);
}

TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_vendor_idNoContext)
{
	uint8_t vendor_id[2];
	EXPECT_EQ(cherry_uci_client_session_get_app_config_vendor_id(NULL, 1,
								     vendor_id),
		  UCI_STATUS_INVALID_PARAM);
}

TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_vendor_idNotAllocated)
{
	EXPECT_EQ(cherry_uci_client_session_get_app_config_vendor_id(context, 1,
								     NULL),
		  UCI_STATUS_INVALID_PARAM);
}

TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_static_sts_IVOK)
{
	uint8_t static_sts_IV[6];
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 14, 0);
	uint8_t data_length = 0x06;
	resp->total_len = 10;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = NO_OF_APP;
	resp->data[6] = UCI_APPLICATION_PARAMETER_STATIC_STS_IV;
	resp->data[7] = data_length;
	resp->data[8] = 0xDE;
	resp->data[9] = 0xAD;
	resp->data[10] = 0xBA;
	resp->data[11] = 0xB1;
	resp->data[12] = 0xBE;
	resp->data[13] = 0xEF;

	transport.SetReply(resp);
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_STATIC_STS_IV);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_static_sts_IV(
			  context, 1, static_sts_IV),
		  UCI_STATUS_OK);
	EXPECT_EQ(static_sts_IV[0], 0xDE);
	EXPECT_EQ(static_sts_IV[1], 0xAD);
	EXPECT_EQ(static_sts_IV[2], 0xBA);
	EXPECT_EQ(static_sts_IV[3], 0xB1);
	EXPECT_EQ(static_sts_IV[4], 0xBE);
	EXPECT_EQ(static_sts_IV[5], 0xEF);
}

TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_static_sts_IVNoContext)
{
	uint8_t static_sts_IV[6];
	EXPECT_EQ(cherry_uci_client_session_get_app_config_static_sts_IV(
			  NULL, 1, static_sts_IV),
		  UCI_STATUS_INVALID_PARAM);
}

TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_static_sts_IVNoData)
{
	EXPECT_EQ(cherry_uci_client_session_get_app_config_static_sts_IV(
			  context, 1, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_session_vendor_id)
{
	uint8_t vendor_id[2] = { 0xDE, 0xAD };
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 5, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 13);
		EXPECT_EQ(blk->total_len, 9);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 9);
		EXPECT_EQ(*(int *)&blk->data[4], 1); //Verify session id
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_VENDOR_ID);
		EXPECT_EQ(blk->data[10], 0x02);
		EXPECT_EQ(blk->data[11], 0xDE);
		EXPECT_EQ(blk->data[12], 0xAD);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_vendor_id(
			  cmd, vendor_id),
		  UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_session_vendor_idNoContext)
{
	uint8_t vendor_id[2] = { 0xDE, 0xAD };
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_vendor_id(
			  NULL, vendor_id),
		  UCI_STATUS_INVALID_PARAM);
}

TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_session_vendor_idNoiData)
{
	struct cherry_uci_client_session_set_app_config_cmd *cmd =
		(struct cherry_uci_client_session_set_app_config_cmd
			 *)0xDEADBEEF;
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_vendor_id(
			  cmd, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_session_static_sts_iv)
{
	uint8_t static_sts_IV[6] = { 0xDE, 0xAD, 0xBA, 0xB1, 0xBE, 0xEF };
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 5, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 17);
		EXPECT_EQ(blk->total_len, 13);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 13);
		EXPECT_EQ(*(int *)&blk->data[4], 1); //Verify session id
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_STATIC_STS_IV);
		EXPECT_EQ(blk->data[10], 0x06);
		EXPECT_EQ(blk->data[11], 0xDE);
		EXPECT_EQ(blk->data[12], 0xAD);
		EXPECT_EQ(blk->data[13], 0xBA);
		EXPECT_EQ(blk->data[14], 0xB1);
		EXPECT_EQ(blk->data[15], 0xBE);
		EXPECT_EQ(blk->data[16], 0xEF);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_static_sts_iv(
			cmd, static_sts_IV),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_session_static_sts_ivNoData)
{
	struct cherry_uci_client_session_set_app_config_cmd *cmd =
		(struct cherry_uci_client_session_set_app_config_cmd
			 *)0xDEADBEEF;
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_static_sts_iv(
			cmd, NULL),
		UCI_STATUS_INVALID_PARAM);
}

TEST_F(TestSessionClient,
       cherry_uci_client_session_set_app_config_cmd_put_session_static_sts_ivNoContext)
{
	uint8_t static_sts_IV[6] = { 0xDE, 0xAD, 0xBA, 0xB1, 0xBE, 0xEF };
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_static_sts_iv(
			NULL, static_sts_IV),
		UCI_STATUS_INVALID_PARAM);
}

TEST_F(TestSessionClient,
       cherry_uci_client_session_get_app_config_vendor_idInvalid_response_size)
{
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG);
	uint8_t vendor_id[2];
	struct uci_blk *resp = uci_blk_alloc(&uci, 9, 0);
	resp->total_len = 9;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 1;
	resp->data[6] = UCI_APPLICATION_PARAMETER_VENDOR_ID;
	//wrong size and garbage data
	resp->data[7] = 5;
	resp->data[8] = 0x21;
	resp->data[9] = 0x12;
	resp->data[10] = 0x34;
	resp->data[11] = 0x56;
	resp->data[12] = 0x78;

	transport.SetReply(resp);

	EXPECT_CALL(transport, Out).WillOnce([&](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 10);
		EXPECT_EQ(blk->total_len, 6);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], 1);
		EXPECT_EQ(blk->data[9], UCI_APPLICATION_PARAMETER_VENDOR_ID);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	EXPECT_EQ(cherry_uci_client_session_get_app_config_vendor_id(context, 1,
								     vendor_id),
		  UCI_STATUS_INVALID_MESSAGE_SIZE);
}

TEST_F(TestSessionClient, set_session_enable_diagsNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_enable_diags(
			  NULL, 1),
		  UCI_STATUS_INVALID_PARAM);
}

TEST_F(TestSessionClient, set_session_enable_diagsOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 5, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_ENABLE_DIAGNOSTICS);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		EXPECT_EQ(blk->data[11], 1);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_enable_diags(
			  cmd, 1),
		  0);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

TEST_F(TestSessionClient, set_session_enable_diagsTimeout)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_ENABLE_DIAGNOSTICS);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		EXPECT_EQ(blk->data[11], 1);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_put_enable_diags(
			  cmd, 1),
		  UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

TEST_F(TestSessionClient, set_session_diags_frame_report_fieldsNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_diags_frame_report_fields(
			NULL, 1),
		UCI_STATUS_INVALID_PARAM);
}

TEST_F(TestSessionClient, set_session_key_diags_frame_report_fieldsOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 5, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DIAGS_FRAME_REPORTS_FIELDS);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		EXPECT_EQ(blk->data[11], 1);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_diags_frame_report_fields(
			cmd, 1),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

TEST_F(TestSessionClient, set_session_diags_frame_report_fieldsTimeout)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 8);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_DIAGS_FRAME_REPORTS_FIELDS);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		EXPECT_EQ(blk->data[11], 1);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_diags_frame_report_fields(
			cmd, 1),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 *
 * cherry_uci_client_session_set_app_config_cmd_put_session_RX_antenna_selection() - Sets antenna set for RX.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @ant_set: antenna set.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_RX_antenna_selectionNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_rx_antenna_selection(
			NULL, BPRF_MODE),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_RX_antenna_selection() - Sets antenna set for RX.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @ant_set: antenna set.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_RX_antenna_selectionOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_RX_ANTENNA_SELECTION);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		EXPECT_EQ(blk->data[11], 1);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_rx_antenna_selection(
			cmd, 1),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_RX_antenna_selection() - Sets antenna set for RX.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @ant_set: antenna set.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_RX_antenna_selectionTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_RX_ANTENNA_SELECTION);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		EXPECT_EQ(blk->data[11], 42);
		EXPECT_FALSE(blk->next);
		return 0;
	});
	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_rx_antenna_selection(
			cmd, 42),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 *
 * cherry_uci_client_session_set_app_config_cmd_put_session_TX_antenna_selection() - Sets antenna set for TX.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @ant_set: antenna set.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_TX_antenna_selectionNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_tx_antenna_selection(
			NULL, BPRF_MODE),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_TX_antenna_selection() - Sets antenna set for TX.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @ant_set: antenna set.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_TX_antenna_selectionOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_TX_ANTENNA_SELECTION);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		EXPECT_EQ(blk->data[11], 1);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_tx_antenna_selection(
			cmd, 1),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_TX_antenna_selection() - Sets antenna set for TX.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @ant_set: antenna set.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_TX_antenna_selectionTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 12);
		EXPECT_EQ(blk->total_len, 8);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_TX_ANTENNA_SELECTION);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		EXPECT_EQ(blk->data[11], 42);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_tx_antenna_selection(
			cmd, 42),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}
/**
 *
 * cherry_uci_client_session_set_app_config_cmd_put_session_antenna_selection() - Sets antenna set.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @ant_set: antenna set.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, set_session_antenna_selectionNoContext)
{
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_antenna_selection(
			NULL, BPRF_MODE),
		UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_antenna_selection() - Sets antenna set.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @ant_set: antenna set.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, set_session_antenna_selectionOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 15);
		EXPECT_EQ(blk->total_len, 11);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], 2);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_RX_ANTENNA_SELECTION);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		EXPECT_EQ(blk->data[11], 42);
		EXPECT_EQ(blk->data[12],
			  UCI_APPLICATION_PARAMETER_TX_ANTENNA_SELECTION);
		EXPECT_EQ(blk->data[13], DATA_LENGTH);
		EXPECT_EQ(blk->data[14], 42);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_antenna_selection(
			cmd, 42),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_antenna_selection() - Sets antenna set.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @ant_set: antenna set.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, set_session_antenna_selectionTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 15);
		EXPECT_EQ(blk->total_len, 11);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], 2);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_RX_ANTENNA_SELECTION);
		EXPECT_EQ(blk->data[10], DATA_LENGTH);
		EXPECT_EQ(blk->data[11], 42);
		EXPECT_EQ(blk->data[12],
			  UCI_APPLICATION_PARAMETER_TX_ANTENNA_SELECTION);
		EXPECT_EQ(blk->data[13], DATA_LENGTH);
		EXPECT_EQ(blk->data[14], 42);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_antenna_selection(
			cmd, 42),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_count() - Get sessions count.
 *
 * @context: FiRa context.
 * @count: Session count.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, session_get_countNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_get_count(NULL, &count),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_count(context, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_count() - Get sessions count.
 *
 * @context: FiRa context.
 * @count: Session count.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, session_get_countnOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_COUNT);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x01;
	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 4);
		EXPECT_EQ(blk->total_len, 0);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_COUNT);
		EXPECT_EQ(blk->data[3], 0);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_count(context, &count), 0);
}

/**
 * cherry_uci_client_session_get_count() - Get sessions count.
 *
 * @context: FiRa context.
 * @count: Session count.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, session_get_countTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 4);
		EXPECT_EQ(blk->total_len, 0);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_COUNT);
		EXPECT_EQ(blk->data[3], 0);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_count(context, &count),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

/**
 * cherry_uci_client_session_get_state() - Get session state.
 *
 * @context: FiRa context.
 * @session_id: Session identifier.
 * @state: Session state.
 *
 * Testcase_Type :
 *      Negative case
 **/
TEST_F(TestSessionClient, cherry_uci_client_session_get_stateNoContext)
{
	EXPECT_EQ(cherry_uci_client_session_get_state(NULL, 1, &state),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_session_get_state(context, 1, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

/**
 * cherry_uci_client_session_get_state() - Get session state.
 *
 * @context: FiRa context.
 * @session_id: Session identifier.
 * @state: Session state.
 *
 * Testcase_Type :
 *      Positive case
 **/
TEST_F(TestSessionClient, cherry_uci_client_session_get_stateOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_STATE);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 8);
		EXPECT_EQ(blk->total_len, 4);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_STATE);
		EXPECT_EQ(blk->data[3], 4);
		EXPECT_EQ(*(int *)&blk->data[4], 1); //Verify session id
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_state(context, 1, &state), 0);
}

/**
 * cherry_uci_client_session_get_state() - Get session state.
 *
 * @context: FiRa context.
 * @session_id: Session identifier.
 * @state: Session state.
 *
 * Testcase_Type :
 *      Timeout case
 **/
TEST_F(TestSessionClient, cherry_uci_client_session_get_stateTimeOut)
{
	//No response back to the cherry_uci_client_session: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 8);
		EXPECT_EQ(blk->total_len, 4);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_GET_STATE);
		EXPECT_EQ(blk->data[3], 4);
		EXPECT_EQ(*(int *)&blk->data[4], 1); //Verify session id
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_session_get_state(context, 1, &state),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

TEST_F(TestSessionClient, set_session_session_time_baseOk)
{
	//Set the response back to the cherry_uci_client_session
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);

	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;

	transport.SetReply(resp);

	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 20);
		EXPECT_EQ(blk->total_len, 16);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_SESSION_SET_APP_CONFIG);
		EXPECT_EQ(blk->data[3], 16);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], NO_OF_APP);
		EXPECT_EQ(blk->data[9],
			  UCI_APPLICATION_PARAMETER_SESSION_TIME_BASE);
		EXPECT_EQ(blk->data[10], 0x09);
		//Session consortium v2.0 section 8.3 App max num of measurements config
		EXPECT_EQ(blk->data[11], 0x07);
		EXPECT_EQ(blk->data[12], 0x01);
		EXPECT_EQ(blk->data[13], 0x00);
		EXPECT_EQ(blk->data[14], 0x00);
		EXPECT_EQ(blk->data[15], 0x00);
		EXPECT_EQ(blk->data[16], 0x02);
		EXPECT_EQ(blk->data[17], 0x00);
		EXPECT_EQ(blk->data[18], 0x00);
		EXPECT_EQ(blk->data[19], 0x00);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	uint32_t session_reference = 1;
	uint32_t offset_us = 2;
	cmd = cherry_uci_client_session_set_app_config_cmd_create(context);
	EXPECT_NE(cmd, nullptr);
	EXPECT_EQ(
		cherry_uci_client_session_set_app_config_cmd_put_session_time_base(
			cmd, true, true, true, session_reference, offset_us),
		UCI_STATUS_OK);
	EXPECT_EQ(cherry_uci_client_session_set_app_config_cmd_send(cmd, 1),
		  UCI_STATUS_OK);
}
