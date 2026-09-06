/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */
/*
 *-----------------------------------------------------------------------------
 * Filename       :   test_cherry_uci_client_calib.cc
 * Description    :   Validate calibration functions through Gtest
 * Pre-requisites :
 *-----------------------------------------------------------------------------
*/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

extern "C" {
#include "cherry_calib_client.h"
#include "cherry_core_client.h"

#include <qmalloc.h>
#include <qtils.h>
#include <uci/uci.h>
#include <uci/uci_message.h>
#include <uci/uci_spec_fira.h>
#include <uci/uci_spec_mcps.h>
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

static void device_status_cb(const enum uci_device_state new_state,
			     void *user_data)
{
}

static void boot_cb(const enum uci_qorvo_boot_reason reason, void *user_data)
{
}

static struct uci_allocator simple_allocator = { .ops = &simple_allocator_ops };

//Test suite
class TestCalibClient : public ::testing::Test {
    protected:
	void SetUp() override
	{
		mock_alloc.implicit_call();
		mock_sema.implicit_call();
		ASSERT_EQ(uci_init(&uci, &simple_allocator, true), 0);
		ASSERT_EQ(uci_transport_attach(&uci, &transport), 0);
		ASSERT_EQ(cherry_uci_client_calib_open(&context, &uci), 0);
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
		cherry_uci_client_calib_close(context);
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
	StrictMock<MockUciTransport> transport;
	struct cherry_calib_context *context = NULL;
	struct cherry_core_context *core_ctx = NULL;
	MockQmalloc mock_alloc;
	MockQsemaphore mock_sema;
	bool expected_timeout = false;

    public:
	static void *calib_alloc_cb(void *user_data, uint8_t size)
	{
		return qmalloc(size);
	}
	static bool calib_key_notif_cb(void *user_data, const char *key_name,
				       const void *data, size_t data_size)
	{
		if (get_calib_expected_value) {
			EXPECT_EQ(0, memcmp(get_calib_expected_value, data,
					    data_size));
		}

		if (key_name)
			qfree((char *)key_name);
		if (data)
			qfree((void *)data);
		return true;
	}
	static const char *get_calib_expected_value;
};

const char *TestCalibClient::get_calib_expected_value = NULL;

TEST_F(TestCalibClient, get_calib_no_context)
{
	const char *key = "kernel";
	const char *keylist[1] = { key };
	struct cherry_calib_cb calib_cb;

	EXPECT_EQ(UCI_STATUS_INVALID_PARAM,
		  cherry_uci_client_calib_get_key(NULL, keylist, 1, &calib_cb));
}

TEST_F(TestCalibClient, get_calib_malformed_response)
{
	const char *key = "kernel";
	const char *keylist[1] = { key };
	struct uci_blk *resp = uci_blk_alloc(&uci, 4, 0);
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_QORVO_MAC,
			       UCI_OID_QORVO_MAC_GET_CALIBRATIONS);
	struct cherry_calib_cb calib_cb;

	calib_cb.user_data = NULL;
	calib_cb.alloc = calib_alloc_cb;
	calib_cb.key_notif = calib_key_notif_cb;

	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 13);
		EXPECT_EQ(blk->total_len, 9);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));

		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_QORVO_MAC);
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_QORVO_MAC_GET_CALIBRATIONS);

		EXPECT_EQ(blk->data[3], 9);
		EXPECT_EQ(blk->data[4], 1); //NO_OF_CALIB
		EXPECT_EQ(blk->data[5], 0); //NO_OF_CALIB on 16 bits
		EXPECT_EQ(blk->data[6], 6); //LENGTH_KEY
		EXPECT_EQ(blk->data[7], 'k');
		EXPECT_EQ(blk->data[12], 'l');

		EXPECT_FALSE(blk->next);
		return 0;
	});

	resp->total_len = 0;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);

	transport.SetReply(resp);

	ASSERT_EQ(UCI_STATUS_SYNTAX_ERROR,
		  cherry_uci_client_calib_get_key(context, keylist, 1,
						  &calib_cb));
};

TEST_F(TestCalibClient, get_calib_bad_response_status)
{
	const char *key = "kernel";
	const char *keylist[1] = { key };
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_QORVO_MAC,
			       UCI_OID_QORVO_MAC_GET_CALIBRATIONS);
	struct cherry_calib_cb calib_cb;

	calib_cb.user_data = NULL;
	calib_cb.alloc = calib_alloc_cb;
	calib_cb.key_notif = calib_key_notif_cb;

	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 13);
		EXPECT_EQ(blk->total_len, 9);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));

		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_QORVO_MAC);
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_QORVO_MAC_GET_CALIBRATIONS);

		EXPECT_EQ(blk->data[3], 9);
		EXPECT_EQ(blk->data[4], 1); //NO_OF_CALIB
		EXPECT_EQ(blk->data[5], 0); //NO_OF_CALIB on 16 bits
		EXPECT_EQ(blk->data[6], 6); //LENGTH_KEY
		EXPECT_EQ(blk->data[7], 'k');
		EXPECT_EQ(blk->data[12], 'l');

		EXPECT_FALSE(blk->next);
		return 0;
	});

	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_FAILED;

	transport.SetReply(resp);

	ASSERT_EQ(UCI_STATUS_FAILED, cherry_uci_client_calib_get_key(
					     context, keylist, 1, &calib_cb));
};

TEST_F(TestCalibClient, get_calib_too_many_keys)
{
	const char *key = "kernel";
	const char *keylist[1] = { key };
	struct uci_blk *resp = uci_blk_alloc(&uci, 22, 0);
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_QORVO_MAC,
			       UCI_OID_QORVO_MAC_GET_CALIBRATIONS);
	struct cherry_calib_cb calib_cb;

	calib_cb.user_data = NULL;
	calib_cb.alloc = calib_alloc_cb;
	calib_cb.key_notif = calib_key_notif_cb;

	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 13);
		EXPECT_EQ(blk->total_len, 9);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));

		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_QORVO_MAC);
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_QORVO_MAC_GET_CALIBRATIONS);

		EXPECT_EQ(blk->data[3], 9);
		EXPECT_EQ(blk->data[4], 1); //NO_OF_CALIB
		EXPECT_EQ(blk->data[5], 0); //NO_OF_CALIB on 16 bits
		EXPECT_EQ(blk->data[6], 6); //LENGTH_KEY
		EXPECT_EQ(blk->data[7], 'k');
		EXPECT_EQ(blk->data[12], 'l');

		EXPECT_FALSE(blk->next);
		return 0;
	});

	resp->total_len = 18;
	resp->len = 4 + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x02;
	resp->data[6] = 0;
	resp->data[7] = 6;
	resp->data[8] = 'k';
	resp->data[9] = 'e';
	resp->data[10] = 'r';
	resp->data[11] = 'n';
	resp->data[12] = 'e';
	resp->data[13] = 'l';
	resp->data[14] = UCI_STATUS_OK;
	resp->data[15] = 6;
	resp->data[16] = 0x01;
	resp->data[17] = 0x02;
	resp->data[18] = 0x03;
	resp->data[19] = 0x04;
	resp->data[20] = 0x05;
	resp->data[21] = 0x06;

	transport.SetReply(resp);

	ASSERT_EQ(UCI_STATUS_SYNTAX_ERROR,
		  cherry_uci_client_calib_get_key(context, keylist, 1,
						  &calib_cb));
};

/*
** To rework to free allocated data in test
TEST_F(TestCalibClient, get_calib_bad_key_status)
{
	const char *key = "kernel";
	const char *keylist[1] = { key };
	struct uci_blk *resp = uci_blk_alloc(&uci, 15, 0);
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_QORVO_MAC,
			       UCI_OID_QORVO_MAC_GET_CALIBRATIONS);
	struct cherry_calib_cb calib_cb;

	calib_cb.user_data = NULL;
	calib_cb.alloc = calib_alloc_cb;
	calib_cb.key_notif = calib_key_notif_cb;

	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 13);
		EXPECT_EQ(blk->total_len, 9);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));

		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_QORVO_MAC);
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_QORVO_MAC_GET_CALIBRATIONS);

		EXPECT_EQ(blk->data[3], 9);
		EXPECT_EQ(blk->data[4], 1); //NO_OF_CALIB
		EXPECT_EQ(blk->data[5], 0); //NO_OF_CALIB on 16 bits
		EXPECT_EQ(blk->data[6], 6); //LENGTH_KEY
		EXPECT_EQ(blk->data[7], 'k');
		EXPECT_EQ(blk->data[12], 'l');

		EXPECT_FALSE(blk->next);
		return 0;
	});

	resp->total_len = 11;
	resp->len = 4 + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x01;
	resp->data[6] = 0;
	resp->data[7] = 6;
	resp->data[8] = 'k';
	resp->data[9] = 'e';
	resp->data[10] = 'r';
	resp->data[11] = 'n';
	resp->data[12] = 'e';
	resp->data[13] = 'l';
	resp->data[14] = UCI_STATUS_INVALID_PARAM;

	transport.SetReply(resp);

	ASSERT_EQ(UCI_STATUS_INVALID_PARAM,
		  cherry_uci_client_calib_get_key(context, keylist, 1, &calib_cb));
};
*/

TEST_F(TestCalibClient, get_calib_timeout)
{
	const char *key = "kernel";
	const char *keylist[1] = { key };
	struct cherry_calib_cb calib_cb;

	calib_cb.user_data = NULL;
	calib_cb.alloc = calib_alloc_cb;
	calib_cb.key_notif = calib_key_notif_cb;

	ExpectTimeout();
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 13);
		EXPECT_EQ(blk->total_len, 9);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));

		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_QORVO_MAC);
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_QORVO_MAC_GET_CALIBRATIONS);

		EXPECT_EQ(blk->data[3], 9);
		EXPECT_EQ(blk->data[4], 1); //NO_OF_CALIB
		EXPECT_EQ(blk->data[5], 0); //NO_OF_CALIB on 16 bits
		EXPECT_EQ(blk->data[6], 6); //LENGTH_KEY
		EXPECT_EQ(blk->data[7], 'k');
		EXPECT_EQ(blk->data[12], 'l');

		EXPECT_FALSE(blk->next);
		return 0;
	});

	ASSERT_EQ(UCI_STATUS_UCI_MESSAGE_RETRY,
		  cherry_uci_client_calib_get_key(context, keylist, 1,
						  &calib_cb));
};

TEST_F(TestCalibClient, get_calib_ok)
{
	const char *key = "kernel";
	const char *keylist[1] = { key };
	const char expected_value[6] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
	struct uci_blk *resp = uci_blk_alloc(&uci, 22, 0);
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_QORVO_MAC,
			       UCI_OID_QORVO_MAC_GET_CALIBRATIONS);
	struct cherry_calib_cb calib_cb;

	calib_cb.user_data = NULL;
	calib_cb.alloc = calib_alloc_cb;
	calib_cb.key_notif = calib_key_notif_cb;

	get_calib_expected_value = expected_value;

	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 13);
		EXPECT_EQ(blk->total_len, 9);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));

		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_QORVO_MAC);
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_QORVO_MAC_GET_CALIBRATIONS);

		EXPECT_EQ(blk->data[3], 9);
		EXPECT_EQ(blk->data[4], 1); //NO_OF_CALIB
		EXPECT_EQ(blk->data[5], 0); //NO_OF_CALIB on 16 bits
		EXPECT_EQ(blk->data[6], 6); //LENGTH_KEY
		EXPECT_EQ(blk->data[7], 'k');
		EXPECT_EQ(blk->data[12], 'l');

		EXPECT_FALSE(blk->next);
		return 0;
	});

	resp->total_len = 18;
	resp->len = 4 + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;
	resp->data[5] = 0x01;
	resp->data[6] = 0;
	resp->data[7] = 6;
	resp->data[8] = 'k';
	resp->data[9] = 'e';
	resp->data[10] = 'r';
	resp->data[11] = 'n';
	resp->data[12] = 'e';
	resp->data[13] = 'l';
	resp->data[14] = UCI_STATUS_OK;
	resp->data[15] = 6;
	resp->data[16] = 0x01;
	resp->data[17] = 0x02;
	resp->data[18] = 0x03;
	resp->data[19] = 0x04;
	resp->data[20] = 0x05;
	resp->data[21] = 0x06;

	transport.SetReply(resp);

	ASSERT_EQ(UCI_STATUS_OK, cherry_uci_client_calib_get_key(
					 context, keylist, 1, &calib_cb));
};

TEST_F(TestCalibClient, set_calib_no_context)
{
	const char *key = "kernel";
	const char value[6] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
	EXPECT_EQ(UCI_STATUS_INVALID_PARAM,
		  cherry_uci_client_calib_set_key(NULL, key, value,
						  sizeof(value)));
}

TEST_F(TestCalibClient, set_calib_malformed_response_no_status)
{
	const char *key = "kernel";
	const char value[6] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_QORVO_MAC,
			       UCI_OID_QORVO_MAC_SET_CALIBRATIONS);
	struct uci_blk *resp = uci_blk_alloc(&uci, 4, 0);
	resp->total_len = 0;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);

	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 20);
		EXPECT_EQ(blk->total_len, 16);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));

		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_QORVO_MAC);
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_QORVO_MAC_SET_CALIBRATIONS);

		EXPECT_EQ(blk->data[3], 16);
		EXPECT_EQ(blk->data[4], 1); //NO_OF_CALIB
		EXPECT_EQ(blk->data[5], 0); //NO_OF_CALIB on 16 bits
		EXPECT_EQ(blk->data[6], 6); //LENGTH_KEY
		EXPECT_EQ(blk->data[7], 'k');
		EXPECT_EQ(blk->data[12], 'l');
		EXPECT_EQ(blk->data[13], 6); //LEN OF VALUE
		EXPECT_EQ(blk->data[14], 0x01);
		EXPECT_EQ(blk->data[15], 0x02);
		EXPECT_EQ(blk->data[16], 0x03);
		EXPECT_EQ(blk->data[17], 0x04);
		EXPECT_EQ(blk->data[18], 0x05);
		EXPECT_EQ(blk->data[19], 0x06);

		EXPECT_FALSE(blk->next);
		return 0;
	});

	transport.SetReply(resp);

	ASSERT_EQ(UCI_STATUS_SYNTAX_ERROR,
		  cherry_uci_client_calib_set_key(context, key, value,
						  sizeof(value)));
};

TEST_F(TestCalibClient, set_calib_timeout)
{
	const char *key = "kernel";
	const char value[6] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };

	ExpectTimeout();
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 20);
		EXPECT_EQ(blk->total_len, 16);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));

		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_QORVO_MAC);
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_QORVO_MAC_SET_CALIBRATIONS);

		EXPECT_EQ(blk->data[3], 16);
		EXPECT_EQ(blk->data[4], 1); //NO_OF_CALIB
		EXPECT_EQ(blk->data[5], 0); //NO_OF_CALIB on 16 bits
		EXPECT_EQ(blk->data[6], 6); //LENGTH_KEY
		EXPECT_EQ(blk->data[7], 'k');
		EXPECT_EQ(blk->data[12], 'l');
		EXPECT_EQ(blk->data[13], 6); //LEN OF VALUE
		EXPECT_EQ(blk->data[14], 0x01);
		EXPECT_EQ(blk->data[15], 0x02);
		EXPECT_EQ(blk->data[16], 0x03);
		EXPECT_EQ(blk->data[17], 0x04);
		EXPECT_EQ(blk->data[18], 0x05);
		EXPECT_EQ(blk->data[19], 0x06);

		EXPECT_FALSE(blk->next);
		return 0;
	});

	ASSERT_EQ(UCI_STATUS_UCI_MESSAGE_RETRY,
		  cherry_uci_client_calib_set_key(context, key, value,
						  sizeof(value)));
};

TEST_F(TestCalibClient, set_calib_ok)
{
	const char *key = "kernel";
	const char value[6] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_QORVO_MAC,
			       UCI_OID_QORVO_MAC_SET_CALIBRATIONS);
	struct uci_blk *resp = uci_blk_alloc(&uci, 6, 0);
	resp->total_len = 2;
	resp->len = UCI_PACKET_HEADER_SIZE + resp->total_len;
	uci_blk_put_control_header(resp, mt_gid_oid, resp->total_len);
	resp->data[4] = UCI_STATUS_OK;

	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) -> int {
		EXPECT_EQ(blk->len, 20);
		EXPECT_EQ(blk->total_len, 16);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));

		uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_QORVO_MAC);
		EXPECT_EQ(UCI_OID(mt_gid_oid),
			  UCI_OID_QORVO_MAC_SET_CALIBRATIONS);

		EXPECT_EQ(blk->data[3], 16);
		EXPECT_EQ(blk->data[4], 1); //NO_OF_CALIB
		EXPECT_EQ(blk->data[5], 0); //NO_OF_CALIB on 16 bits
		EXPECT_EQ(blk->data[6], 6); //LENGTH_KEY
		EXPECT_EQ(blk->data[7], 'k');
		EXPECT_EQ(blk->data[12], 'l');
		EXPECT_EQ(blk->data[13], 6); //LEN OF VALUE
		EXPECT_EQ(blk->data[14], 0x01);
		EXPECT_EQ(blk->data[15], 0x02);
		EXPECT_EQ(blk->data[16], 0x03);
		EXPECT_EQ(blk->data[17], 0x04);
		EXPECT_EQ(blk->data[18], 0x05);
		EXPECT_EQ(blk->data[19], 0x06);

		EXPECT_FALSE(blk->next);
		return 0;
	});

	transport.SetReply(resp);

	ASSERT_EQ(UCI_STATUS_OK, cherry_uci_client_calib_set_key(
					 context, key, value, sizeof(value)));
};
