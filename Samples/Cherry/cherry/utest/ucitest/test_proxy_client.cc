/*
 * Implementation for fira client functions
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

extern "C" {
#include "cherry_proxy_client.h"

#include <qmalloc.h>
#include <uci/uci.h>
#include <uci/uci_message.h>
#include <uci/uci_spec_fira.h>
#include <uci_internal.h>
}

#include "mock_qmalloc.hh"
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

static struct uci_allocator simple_allocator = { .ops = &simple_allocator_ops };

static void boot_cb(const enum uci_qorvo_boot_reason reason, void *user_data)
{
}

static void data_cb(uint8_t *data_notif, uint16_t data_notif_sz,
		    void *user_data)
{
}

//Test suite
class TestProxyClient : public ::testing::Test {
    protected:
	void SetUp() override
	{
		mock_alloc.implicit_call();
		ASSERT_EQ(uci_init(&uci, &simple_allocator, true), 0);
		ASSERT_EQ(uci_transport_attach(&uci, &transport), 0);
		ASSERT_EQ(cherry_uci_client_proxy_open(&proxy_ctx, &transport,
						       NULL, false, boot_cb,
						       data_cb),
			  0);
	}
	void TearDown() override
	{
		ASSERT_TRUE(Mock::VerifyAndClearExpectations(&transport));
		uci_transport_detach(&uci);
		cherry_uci_client_proxy_close(proxy_ctx);
		uci_uninit(&uci);
	}
	struct uci uci;
	struct cherry_proxy_context *proxy_ctx = NULL;
	StrictMock<MockUciTransport> transport;
	MockQmalloc mock_alloc;

    public:
	static void boot_cb_with_expect(const enum uci_qorvo_boot_reason reason,
					void *user_data)
	{
		EXPECT_EQ(reason, expected_reason);
		boot_cb_cnt++;
	}
	static enum uci_qorvo_boot_reason expected_reason;
	static uint8_t boot_cb_cnt;
};

class TestProxyClientNtf : public ::TestProxyClient {
    protected:
	void SetUp() override
	{
		mock_alloc.implicit_call();
		ASSERT_EQ(uci_init(&uci, &simple_allocator, true), 0);
		ASSERT_EQ(uci_transport_attach(&uci, &transport), 0);
		ASSERT_EQ(cherry_uci_client_proxy_open(
				  &proxy_ctx, &transport, NULL, false,
				  boot_cb_with_expect, data_cb),
			  0);
	}
};

enum uci_qorvo_boot_reason TestProxyClient::expected_reason =
	UCI_QORVO_BOOT_REASON_FATAL_ERROR_RESET;
uint8_t TestProxyClient::boot_cb_cnt = 0;

/**
 * uci_boot_handler() - get UWBS boot notification.
 *
 * Testcase_Type :
 *      Nominal case
 **/
TEST_F(TestProxyClientNtf, test_nominal_ntf_boot)
{
	//Set the notification
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_NOTIFICATION,
					     UCI_GID_QORVO_EXT2,
					     UCI_OID_QORVO_CORE_DEVICE_BOOT);
	struct uci_blk *notif =
		uci_blk_alloc(&uci, UCI_PACKET_HEADER_SIZE + 1, 0);
	notif->len = UCI_PACKET_HEADER_SIZE + 1;
	notif->total_len = UCI_DEVICE_STATE_READY;
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
TEST_F(TestProxyClientNtf, test_nominal_invalid_ntf_boot)
{
	//Set the notification
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_NOTIFICATION,
					     UCI_GID_QORVO_EXT2,
					     UCI_OID_QORVO_CORE_DEVICE_BOOT);
	struct uci_blk *notif =
		uci_blk_alloc(&uci, UCI_PACKET_HEADER_SIZE + 1, 0);
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
TEST_F(TestProxyClient, test_nominal_no_ntf_boot_cb)
{
	//Set the notification
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_NOTIFICATION,
					     UCI_GID_QORVO_EXT2,
					     UCI_OID_QORVO_CORE_DEVICE_BOOT);
	struct uci_blk *notif =
		uci_blk_alloc(&uci, UCI_PACKET_HEADER_SIZE + 1, 0);
	notif->len = UCI_PACKET_HEADER_SIZE;
	notif->total_len = 0;
	uci_blk_put_control_header(notif, mt_gid_oid, notif->total_len);

	boot_cb_cnt = 0;

	transport.SendNotif(notif);

	EXPECT_EQ(boot_cb_cnt, 0);
}
