/*
 * Implementation for fira client functions
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

extern "C" {
#include "cherry_fira_client.h"

#include <qerr.h>
#include <qmalloc.h>
#include <uci/uci.h>
#include <uci/uci_message.h>
#include <uci/uci_spec_fira.h>
#include <uci/uci_unit_converter.h>
#include <uci_internal.h>
}

#include "mock_qmalloc.hh"
#include "mock_qsemaphore.hh"
#include "mock_uci_transport.h"

#include <cstring>

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

static struct uci_allocator simple_allocator = { .ops = &simple_allocator_ops };

static void fira_handler_client_diag_ntf_cb(struct diagnostic_info *result,
					    void *user_data)
{
}

//Test suite
class TestFiraClient : public ::testing::Test {
    protected:
	void SetUp() override
	{
		mock_alloc.implicit_call();
		mock_sema.implicit_call();
		ASSERT_EQ(uci_init(&uci, &simple_allocator, true), 0);
		ASSERT_EQ(uci_transport_attach(&uci, &transport), 0);
		ASSERT_EQ(cherry_uci_client_fira_open(
				  &context, &uci, NULL,
				  fira_handler_client_diag_ntf_cb),
			  0);
	}
	void TearDown() override
	{
		if (expected_timeout)
			mock_sema.implicit_call();
		ASSERT_TRUE(Mock::VerifyAndClearExpectations(&transport));
		uci_transport_detach(&uci);
		cherry_uci_client_fira_close(context);
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
	uint8_t device_type, device_role, range_round_usage, scheduled_mode,
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
	struct cherry_fira_context *context = NULL;
	StrictMock<MockUciTransport> transport;
	uint32_t session_handle = 0;
	MockQmalloc mock_alloc;
	MockQsemaphore mock_sema;
	bool expected_timeout = false;
};

TEST_F(TestFiraClient, float_to_q8dot8One)
{
	EXPECT_EQ(float_to_q(1, 8), 256);
}

TEST_F(TestFiraClient, float_to_q16dot16MinusOne)
{
	EXPECT_EQ(float_to_q(-1, 16), -65536);
}

TEST_F(TestFiraClient, float_to_q24dot8Float)
{
	EXPECT_EQ(float_to_q(2.65, 8), 678);
}

TEST_F(TestFiraClient, float_to_q24dot8NegativeFloat1)
{
	EXPECT_EQ(float_to_q(-52.6583, 8), -13481);
}

TEST_F(TestFiraClient, float_to_q24dot8NegativeFloat2)
{
	EXPECT_EQ(float_to_q(-52.66210, 8), -13481);
}

TEST_F(TestFiraClient, float_to_q24dot8LongNegativeFloat1)
{
	EXPECT_EQ(float_to_q(-104547.756, 8), -26764226);
}

TEST_F(TestFiraClient, float_to_q24dot8LongNegativeFloat2)
{
	EXPECT_EQ(float_to_q(-33892.2734375, 8), -8676422);
}

TEST_F(TestFiraClient, float_to_qOverflowLow)
{
	EXPECT_EQ(float_to_q(-0x7FFFFFFFFFFFFFFF, 8), -1);
}

TEST_F(TestFiraClient, float_to_qOverflowHigh)
{
	EXPECT_EQ(float_to_q(0x7FFFFFFFFFFFFFFF, 8), -1);
}

// cherry_uci_client_fira_update_dt_tag_ranging_rounds

TEST_F(TestFiraClient,
       cherry_uci_client_fira_update_dt_tag_ranging_roundsErrorCases)
{
	uint8_t ranging_round_indexes[1] = { 0x22 };
	uint8_t dl_tdoa_update_ranging_round_array
		[DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE];
	uint8_t dl_tdoa_update_ranging_round_array_size[1];

	EXPECT_EQ(cherry_uci_client_fira_update_dt_tag_ranging_rounds(
			  NULL, 1, 1, ranging_round_indexes,
			  dl_tdoa_update_ranging_round_array,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_fira_update_dt_tag_ranging_rounds(
			  context, 1, 0, ranging_round_indexes,
			  dl_tdoa_update_ranging_round_array,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_fira_update_dt_tag_ranging_rounds(
			  context, 1, FIRA_DT_TAG_MAX_ACTIVE_RR,
			  ranging_round_indexes,
			  dl_tdoa_update_ranging_round_array,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_fira_update_dt_tag_ranging_rounds(
			  context, 1, 1, NULL,
			  dl_tdoa_update_ranging_round_array,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_fira_update_dt_tag_ranging_rounds(
			  context, 1, 1, ranging_round_indexes, NULL,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_fira_update_dt_tag_ranging_rounds(
			  context, 1, 1, ranging_round_indexes,
			  dl_tdoa_update_ranging_round_array, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

TEST_F(TestFiraClient,
       cherry_uci_client_fira_update_dt_tag_ranging_roundsnOkSize0)
{
	uint8_t ranging_round_indexes[1] = { 0x22 };
	uint8_t dl_tdoa_update_ranging_round_array
		[DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE];
	uint8_t dl_tdoa_update_ranging_round_array_size[1];

	//Set the response back to the cherry_uci_client_fira
	uint16_t mt_gid_oid = UCI_MT_GID_OID(
		UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_SESSION_CONFIG,
		UCI_OID_SESSION_UPDATE_DT_TAG_RANGING_ROUNDS);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;
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
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_SESSION_UPDATE_DT_TAG_RANGING_ROUNDS);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], 1);
		EXPECT_EQ(blk->data[9], 0x22);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_fira_update_dt_tag_ranging_rounds(
			  context, 1, 1, ranging_round_indexes,
			  dl_tdoa_update_ranging_round_array,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_OK);
	EXPECT_EQ(dl_tdoa_update_ranging_round_array_size[0], 0);
	EXPECT_EQ(dl_tdoa_update_ranging_round_array[0], NULL);
}

TEST_F(TestFiraClient,
       cherry_uci_client_fira_update_dt_tag_ranging_roundsTimeOut)
{
	uint8_t ranging_round_indexes[1] = { 0x22 };
	uint8_t dl_tdoa_update_ranging_round_array
		[DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE];
	uint8_t dl_tdoa_update_ranging_round_array_size[1];

	//No response back to the cherry_uci_client_fira: timeout and retry status
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
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_SESSION_UPDATE_DT_TAG_RANGING_ROUNDS);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], 1);
		EXPECT_EQ(blk->data[9], 0x22);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_fira_update_dt_tag_ranging_rounds(
			  context, 1, 1, ranging_round_indexes,
			  dl_tdoa_update_ranging_round_array,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

TEST_F(TestFiraClient,
       cherry_uci_client_fira_update_dt_tag_ranging_roundsOkSize1)
{
	uint8_t ranging_round_indexes[2] = { 0x22 };
	uint8_t dl_tdoa_update_ranging_round_array
		[DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE];
	uint8_t dl_tdoa_update_ranging_round_array_size[1];

	//Set the response back to the cherry_uci_client_fira
	uint16_t mt_gid_oid = UCI_MT_GID_OID(
		UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_SESSION_CONFIG,
		UCI_OID_SESSION_UPDATE_DT_TAG_RANGING_ROUNDS);
	struct uci_blk *resp = uci_blk_alloc(&uci, 7, 0);
	resp->total_len = 3;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_FAILED;
	resp->data[5] = 0x01;
	resp->data[6] = 0x22;
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
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_SESSION_UPDATE_DT_TAG_RANGING_ROUNDS);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], 1);
		EXPECT_EQ(blk->data[9], 0x22);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_fira_update_dt_tag_ranging_rounds(
			  context, 1, 1, ranging_round_indexes,
			  dl_tdoa_update_ranging_round_array,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_FAILED);
	EXPECT_EQ(dl_tdoa_update_ranging_round_array_size[0], 0x01);
	EXPECT_EQ(dl_tdoa_update_ranging_round_array[0], 0x22);
}

TEST_F(TestFiraClient,
       cherry_uci_client_fira_update_dt_tag_ranging_roundsIncorrectSize)
{
	uint8_t ranging_round_indexes[2] = { 0x22 };
	uint8_t dl_tdoa_update_ranging_round_array
		[DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE];
	uint8_t dl_tdoa_update_ranging_round_array_size[1];

	//Set the response back to the cherry_uci_client_fira
	uint16_t mt_gid_oid = UCI_MT_GID_OID(
		UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_SESSION_CONFIG,
		UCI_OID_SESSION_UPDATE_DT_TAG_RANGING_ROUNDS);
	struct uci_blk *resp = uci_blk_alloc(&uci, 5, 0);
	resp->total_len = 1;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_FAILED;
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
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_SESSION_UPDATE_DT_TAG_RANGING_ROUNDS);
		EXPECT_EQ(blk->data[3], 6);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], 1);
		EXPECT_EQ(blk->data[9], 0x22);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_fira_update_dt_tag_ranging_rounds(
			  context, 1, 1, ranging_round_indexes,
			  dl_tdoa_update_ranging_round_array,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_INVALID_MESSAGE_SIZE);
}

// cherry_uci_client_fira_update_dt_anchor_ranging_rounds

TEST_F(TestFiraClient,
       cherry_uci_client_fira_update_dt_anchor_ranging_roundsErrorCases)
{
	uint8_t ranging_round_indexes[1] = { 0x23 };
	uint8_t ranging_role[1] = { UCI_DT_ANCHOR_INITIATOR };
	uint8_t nb_of_responder_too_low[1] = { 0 };
	uint8_t nb_of_responder_too_high[1] = { 9 };
	uint8_t nb_of_responder[1] = { 2 };
	uint16_t responder_address_list[1][8] = { { 0x33, 0x44 } };
	uint8_t responder_slot_scheduling[1] = { RESPONDER_SLOTS_PRESENT };
	uint8_t responder_slots[1][8] = { { 2, 4 } };

	uint8_t dl_tdoa_update_ranging_round_array
		[DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE];
	uint8_t dl_tdoa_update_ranging_round_array_size[1];

	EXPECT_EQ(cherry_uci_client_fira_update_dt_anchor_ranging_rounds(
			  NULL, 1, 1, ranging_round_indexes, ranging_role, NULL,
			  NULL, NULL, NULL, dl_tdoa_update_ranging_round_array,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_fira_update_dt_anchor_ranging_rounds(
			  context, 1, 0, ranging_round_indexes, ranging_role,
			  NULL, NULL, NULL, NULL,
			  dl_tdoa_update_ranging_round_array,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_fira_update_dt_anchor_ranging_rounds(
			  context, 1, FIRA_DT_TAG_MAX_ACTIVE_RR,
			  ranging_round_indexes, ranging_role, NULL, NULL, NULL,
			  NULL, dl_tdoa_update_ranging_round_array,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_fira_update_dt_anchor_ranging_rounds(
			  context, 1, 1, NULL, ranging_role, NULL, NULL, NULL,
			  NULL, dl_tdoa_update_ranging_round_array,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_fira_update_dt_anchor_ranging_rounds(
			  context, 1, 1, ranging_round_indexes, NULL, NULL,
			  NULL, NULL, NULL, dl_tdoa_update_ranging_round_array,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_fira_update_dt_anchor_ranging_rounds(
			  context, 1, 1, ranging_round_indexes, ranging_role,
			  NULL, NULL, NULL, NULL,
			  dl_tdoa_update_ranging_round_array,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_fira_update_dt_anchor_ranging_rounds(
			  context, 1, 1, ranging_round_indexes, ranging_role,
			  nb_of_responder_too_low, NULL, NULL, NULL,
			  dl_tdoa_update_ranging_round_array,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_fira_update_dt_anchor_ranging_rounds(
			  context, 1, 1, ranging_round_indexes, ranging_role,
			  nb_of_responder_too_high, NULL, NULL, NULL,
			  dl_tdoa_update_ranging_round_array,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_fira_update_dt_anchor_ranging_rounds(
			  context, 1, 1, ranging_round_indexes, ranging_role,
			  nb_of_responder, NULL, NULL, NULL,
			  dl_tdoa_update_ranging_round_array,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_fira_update_dt_anchor_ranging_rounds(
			  context, 1, 1, ranging_round_indexes, ranging_role,
			  nb_of_responder, responder_address_list, NULL, NULL,
			  dl_tdoa_update_ranging_round_array,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_fira_update_dt_anchor_ranging_rounds(
			  context, 1, 1, ranging_round_indexes, ranging_role,
			  nb_of_responder, responder_address_list,
			  responder_slot_scheduling, NULL,
			  dl_tdoa_update_ranging_round_array,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_fira_update_dt_anchor_ranging_rounds(
			  context, 1, 1, ranging_round_indexes, ranging_role,
			  nb_of_responder, responder_address_list,
			  responder_slot_scheduling, responder_slots, NULL,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_INVALID_PARAM);
	EXPECT_EQ(cherry_uci_client_fira_update_dt_anchor_ranging_rounds(
			  context, 1, 1, ranging_round_indexes, ranging_role,
			  nb_of_responder, responder_address_list,
			  responder_slot_scheduling, responder_slots,
			  dl_tdoa_update_ranging_round_array, NULL),
		  UCI_STATUS_INVALID_PARAM);
}

TEST_F(TestFiraClient,
       cherry_uci_client_fira_update_dt_anchor_ranging_roundsTimeOut)
{
	uint8_t ranging_round_indexes[1] = { 0x23 };
	uint8_t ranging_role[1] = { UCI_DT_ANCHOR_INITIATOR };
	uint8_t nb_of_responder[1] = { 2 };
	uint16_t responder_address_list[1][8] = { { 0x33, 0x44 } };
	uint8_t responder_slot_scheduling[1] = { RESPONDER_SLOTS_PRESENT };
	uint8_t responder_slots[1][8] = { { 2, 4 } };

	uint8_t dl_tdoa_update_ranging_round_array
		[DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE];
	uint8_t dl_tdoa_update_ranging_round_array_size[1];

	//No response back to the cherry_uci_client_fira: timeout and retry status
	ExpectTimeout();
	//Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 19);
		EXPECT_EQ(blk->total_len, 15);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_SESSION_UPDATE_DT_ANCHOR_RANGING_ROUNDS);
		EXPECT_EQ(blk->data[3], 15);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], 1); // Number of Active Ranging Rounds
		EXPECT_EQ(blk->data[9], 0x23); // Round Index
		EXPECT_EQ(blk->data[10], 0x01); // Ranging Role
		EXPECT_EQ(blk->data[11], 0x02); // Number of Responders
		EXPECT_EQ(blk->data[12],
			  0x33); // Responder MAC Address 1 (bits 7-0)
		EXPECT_EQ(blk->data[13],
			  0x00); // Responder MAC Address 1 (bits 31-16)
		EXPECT_EQ(blk->data[14],
			  0x44); // Responder MAC Address 2 (bits 7-0)
		EXPECT_EQ(blk->data[15],
			  0x00); // Responder MAC Address 2 (bits 31-16)
		EXPECT_EQ(blk->data[16], 0x01); // Responder Slot Scheduling
		EXPECT_EQ(blk->data[17], 0x02); // Responder Slots 1
		EXPECT_EQ(blk->data[18], 0x04); // Responder Slots 2
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_fira_update_dt_anchor_ranging_rounds(
			  context, 1, 1, ranging_round_indexes, ranging_role,
			  nb_of_responder, responder_address_list,
			  responder_slot_scheduling, responder_slots,
			  dl_tdoa_update_ranging_round_array,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_UCI_MESSAGE_RETRY);
}

TEST_F(TestFiraClient,
       cherry_uci_client_fira_update_dt_anchor_ranging_rounds_Ok_Initiator_RspSlotPresent_RspSize0)
{
	uint8_t ranging_round_indexes[1] = { 0x23 };
	uint8_t ranging_role[1] = { UCI_DT_ANCHOR_INITIATOR };
	uint8_t nb_of_responder[1] = { 2 };
	uint16_t responder_address_list[1][8] = { { 0x33, 0x44 } };
	uint8_t responder_slot_scheduling[1] = { RESPONDER_SLOTS_PRESENT };
	uint8_t responder_slots[1][8] = { { 2, 4 } };

	uint8_t dl_tdoa_update_ranging_round_array
		[DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE];
	uint8_t dl_tdoa_update_ranging_round_array_size[1];

	// Set the response back to the cherry_uci_client_fira
	uint16_t mt_gid_oid = UCI_MT_GID_OID(
		UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_SESSION_CONFIG,
		UCI_OID_SESSION_UPDATE_DT_ANCHOR_RANGING_ROUNDS);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;
	transport.SetReply(resp);

	// Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 19);
		EXPECT_EQ(blk->total_len, 15);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_SESSION_UPDATE_DT_ANCHOR_RANGING_ROUNDS);
		EXPECT_EQ(blk->data[3], 15);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], 1); // Number of Active Ranging Rounds
		EXPECT_EQ(blk->data[9], 0x23); // Round Index
		EXPECT_EQ(blk->data[10], 0x01); // Ranging Role
		EXPECT_EQ(blk->data[11], 0x02); // Number of Responders
		EXPECT_EQ(blk->data[12],
			  0x33); // Responder MAC Address 1 (bits 7-0)
		EXPECT_EQ(blk->data[13],
			  0x00); // Responder MAC Address 1 (bits 31-16)
		EXPECT_EQ(blk->data[14],
			  0x44); // Responder MAC Address 2 (bits 7-0)
		EXPECT_EQ(blk->data[15],
			  0x00); // Responder MAC Address 2 (bits 31-16)
		EXPECT_EQ(blk->data[16], 0x01); // Responder Slot Scheduling
		EXPECT_EQ(blk->data[17], 0x02); // Responder Slots 1
		EXPECT_EQ(blk->data[18], 0x04); // Responder Slots 2
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_fira_update_dt_anchor_ranging_rounds(
			  context, 1, 1, ranging_round_indexes, ranging_role,
			  nb_of_responder, responder_address_list,
			  responder_slot_scheduling, responder_slots,
			  dl_tdoa_update_ranging_round_array,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_OK);
}

TEST_F(TestFiraClient,
       cherry_uci_client_fira_update_dt_anchor_ranging_rounds_Ok_Initiator_2Rounds_RspSlotImplicit_RspSize1)
{
	uint8_t ranging_round_indexes[2] = { 0x23, 0x24 };
	uint8_t ranging_role[2] = { UCI_DT_ANCHOR_INITIATOR,
				    UCI_DT_ANCHOR_INITIATOR };
	uint8_t nb_of_responder[2] = { 2, 2 };
	uint16_t responder_address_list[2][8] = { { 0x33, 0x44 },
						  { 0x33, 0x44 } };
	uint8_t responder_slot_scheduling[2] = { IMPLICIT_SCHEDULING,
						 IMPLICIT_SCHEDULING };
	uint8_t responder_slots[2][8] = { { 2, 4 }, { 2, 4 } };

	uint8_t dl_tdoa_update_ranging_round_array
		[DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE];
	uint8_t dl_tdoa_update_ranging_round_array_size[1];

	// Set the response back to the cherry_uci_client_fira
	uint16_t mt_gid_oid = UCI_MT_GID_OID(
		UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_SESSION_CONFIG,
		UCI_OID_SESSION_UPDATE_DT_ANCHOR_RANGING_ROUNDS);
	struct uci_blk *resp = uci_blk_alloc(&uci, 7, 0);
	resp->total_len = 3;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_FAILED;
	resp->data[5] = 0x01;
	resp->data[6] = 0x23;
	transport.SetReply(resp);

	// Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 25);
		EXPECT_EQ(blk->total_len, 21);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_SESSION_UPDATE_DT_ANCHOR_RANGING_ROUNDS);
		EXPECT_EQ(blk->data[3], 21);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], 2); // Number of Active Ranging Rounds
		EXPECT_EQ(blk->data[9], 0x23); // Round Index
		EXPECT_EQ(blk->data[10], 0x01); // Ranging Role
		EXPECT_EQ(blk->data[11], 0x02); // Number of Responders
		EXPECT_EQ(blk->data[12],
			  0x33); // Responder MAC Address 1 (bits 7-0)
		EXPECT_EQ(blk->data[13],
			  0x00); // Responder MAC Address 1 (bits 31-16)
		EXPECT_EQ(blk->data[14],
			  0x44); // Responder MAC Address 2 (bits 7-0)
		EXPECT_EQ(blk->data[15],
			  0x00); // Responder MAC Address 2 (bits 31-16)
		EXPECT_EQ(blk->data[16], 0x00); // Responder Slot Scheduling
		EXPECT_EQ(blk->data[17], 0x24); // Round Index
		EXPECT_EQ(blk->data[18], 0x01); // Ranging Role
		EXPECT_EQ(blk->data[19], 0x02); // Number of Responders
		EXPECT_EQ(blk->data[20],
			  0x33); // Responder MAC Address 1 (bits 7-0)
		EXPECT_EQ(blk->data[21],
			  0x00); // Responder MAC Address 1 (bits 31-16)
		EXPECT_EQ(blk->data[22],
			  0x44); // Responder MAC Address 2 (bits 7-0)
		EXPECT_EQ(blk->data[23],
			  0x00); // Responder MAC Address 2 (bits 31-16)
		EXPECT_EQ(blk->data[24], 0x00); // Responder Slot Scheduling
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_fira_update_dt_anchor_ranging_rounds(
			  context, 1, 2, ranging_round_indexes, ranging_role,
			  nb_of_responder, responder_address_list,
			  responder_slot_scheduling, responder_slots,
			  dl_tdoa_update_ranging_round_array,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_FAILED);
}

TEST_F(TestFiraClient,
       cherry_uci_client_fira_update_dt_anchor_ranging_rounds_Ok_Responder_RspSize0)
{
	uint8_t ranging_round_indexes[1] = { 0x32 };
	uint8_t ranging_role[1] = { UCI_DT_ANCHOR_RESPONDER };
	uint8_t nb_of_responder[1] = { 2 };
	uint16_t responder_address_list[1][8] = { { 0x33, 0x44 } };
	uint8_t responder_slot_scheduling[1] = { IMPLICIT_SCHEDULING };
	uint8_t responder_slots[1][8] = { { 2, 4 } };

	uint8_t dl_tdoa_update_ranging_round_array
		[DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE];
	uint8_t dl_tdoa_update_ranging_round_array_size[1];

	// Set the response back to the cherry_uci_client_fira
	uint16_t mt_gid_oid = UCI_MT_GID_OID(
		UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_SESSION_CONFIG,
		UCI_OID_SESSION_UPDATE_DT_ANCHOR_RANGING_ROUNDS);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x00;
	transport.SetReply(resp);

	// Verify the Command Out
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 11);
		EXPECT_EQ(blk->total_len, 7);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_SESSION_CONFIG);
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_SESSION_UPDATE_DT_ANCHOR_RANGING_ROUNDS);
		EXPECT_EQ(blk->data[3], 7);
		EXPECT_EQ(*(int *)&blk->data[4], 1);
		EXPECT_EQ(blk->data[8], 1); // Number of Active Ranging Rounds
		EXPECT_EQ(blk->data[9], 0x32); // Round Index
		EXPECT_EQ(blk->data[10], 0x00); // Ranging Role
		EXPECT_FALSE(blk->next);
		return 0;
	});

	EXPECT_EQ(cherry_uci_client_fira_update_dt_anchor_ranging_rounds(
			  context, 1, 1, ranging_round_indexes, ranging_role,
			  nb_of_responder, responder_address_list,
			  responder_slot_scheduling, responder_slots,
			  dl_tdoa_update_ranging_round_array,
			  dl_tdoa_update_ranging_round_array_size),
		  UCI_STATUS_OK);
	EXPECT_EQ(dl_tdoa_update_ranging_round_array_size[0], 0x00);
	EXPECT_EQ(dl_tdoa_update_ranging_round_array[0], NULL);
}
