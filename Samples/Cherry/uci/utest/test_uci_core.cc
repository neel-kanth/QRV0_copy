/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "mock_uci_allocator.h"
#include "mock_uci_transport.h"
#include "uci_gmock_matcher.h"

#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

extern "C" {
#include "uci/uci.h"
#include "uci/uci_message.h"
#include "uci/uci_spec_fira.h"
#include "uci/uci_spec_qorvo.h"
#include "uci_internal.h"
}

using testing::InSequence;
using testing::Mock;
using testing::Return;
using testing::StrictMock;

struct uci_message_builder;

#define CREATE_BLOCK(payload)                                               \
	{                                                                   \
		.next = nullptr, .data = payload, .len = 0, .total_len = 0, \
		.size = sizeof(payload), .flags = 0                         \
	}

/**
 * expect_alloc() - Expect both the allocation and later cleanup of the block.
 *
 * @allocator: The Mock allocator used by UCI core.
 * @size_hint: The requested size hint we expect or 0 if we don't know the size
 * to expect.
 */
static void expect_alloc(MockUciAllocator *allocator, size_t size_hint)
{
	uci_blk *blk = new uci_blk();
	ASSERT_TRUE(blk);
	blk->data = new uint8_t[size_hint];
	ASSERT_TRUE(blk->data);
	blk->size = size_hint;
	blk->len = 0;
	blk->next = nullptr;
	blk->flags = 0;
	blk->total_len = 0;

	EXPECT_CALL(*allocator, alloc(size_hint, 0))
		.WillOnce(Return(blk))
		.RetiresOnSaturation();
	EXPECT_CALL(*allocator, free(blk)).WillOnce([](struct uci_blk *to_free) {
		delete[] to_free->data;
		delete to_free;
	});
}

class TestUciInit : public ::testing::Test {};

TEST_F(TestUciInit, NoAllocator)
{
	struct uci uci;

	EXPECT_EQ(uci_init(&uci, nullptr, false), QERR_ENOENT);
}

TEST_F(TestUciInit, WithAllocator)
{
	MockUciAllocator allocator;
	struct uci uci;

	/* Uci will pre-allocate a uci_blk for an empty response. */
	expect_alloc(&allocator, UCI_STATUS_PACKET_SIZE);
	EXPECT_EQ(uci_init(&uci, &allocator, false), 0);

	uci_uninit(&uci);
}

TEST(UciCore, ServerIgnoreRsp)
{
	MockUciAllocator allocator;
	StrictMock<MockUciTransport> transport;
	struct uci uci;

	/* Uci will pre-allocate a uci_blk for an empty response. */
	expect_alloc(&allocator, UCI_STATUS_PACKET_SIZE);

	/* Given this is a server and message received is a respond or notif */
	ASSERT_EQ(uci_init(&uci, &allocator, false), 0);
	ASSERT_EQ(uci_transport_attach(&uci, &transport), 0);

	uint8_t payload[5];
	struct uci_blk packet = CREATE_BLOCK(payload);
	packet.len = 5;
	packet.total_len = 1;
	EXPECT_CALL(allocator, free(&packet));

	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_INIT);
	uci_blk_put_control_header(&packet, mt_gid_oid, packet.total_len);
	payload[4] = UCI_STATUS_OK;

	uci_packet_recv(&uci, &packet);

	uci_transport_detach(&uci);
	uci_uninit(&uci);
}

TEST(UciCore, ServerIgnoreNotif)
{
	MockUciAllocator allocator;
	StrictMock<MockUciTransport> transport;
	struct uci uci;

	/* Uci will pre-allocate a uci_blk for an empty response. */
	expect_alloc(&allocator, UCI_STATUS_PACKET_SIZE);

	/* Given this is a server and message received is a respond or notif */
	ASSERT_EQ(uci_init(&uci, &allocator, false), 0);
	ASSERT_EQ(uci_transport_attach(&uci, &transport), 0);

	uint8_t payload[5];
	struct uci_blk packet = CREATE_BLOCK(payload);
	packet.len = 5;
	packet.total_len = 1;
	EXPECT_CALL(allocator, free(&packet));

	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_NOTIFICATION,
					     UCI_GID_CORE,
					     UCI_OID_CORE_DEVICE_STATUS);
	uci_blk_put_control_header(&packet, mt_gid_oid, packet.total_len);
	payload[4] = UCI_DEVICE_STATE_READY;

	uci_packet_recv(&uci, &packet);

	uci_transport_detach(&uci);
	uci_uninit(&uci);
}

TEST(UciCore, ClientIgnoreCmd)
{
	MockUciAllocator allocator;
	StrictMock<MockUciTransport> transport;
	struct uci uci;

	/* Uci will pre-allocate a uci_blk for an empty response. */
	expect_alloc(&allocator, UCI_STATUS_PACKET_SIZE);

	/* Given this is a client and message received is a command */
	ASSERT_EQ(uci_init(&uci, &allocator, true), 0);
	ASSERT_EQ(uci_transport_attach(&uci, &transport), 0);

	uint8_t payload[4];
	struct uci_blk packet = CREATE_BLOCK(payload);
	packet.len = sizeof(payload);
	packet.total_len = 0;
	EXPECT_CALL(allocator, free(&packet));

	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND,
					     UCI_GID_CORE,
					     UCI_OID_CORE_DEVICE_RESET);
	uci_blk_put_control_header(&packet, mt_gid_oid, packet.total_len);

	uci_packet_recv(&uci, &packet);

	uci_transport_detach(&uci);
	uci_uninit(&uci);
}

TEST(UciCore, ClientIgnoreUnknown)
{
	MockUciAllocator allocator;
	StrictMock<MockUciTransport> transport;
	struct uci uci;

	/* Uci will pre-allocate a uci_blk for an empty response. */
	expect_alloc(&allocator, UCI_STATUS_PACKET_SIZE);

	/* Given this is a client and message received is a command */
	ASSERT_EQ(uci_init(&uci, &allocator, true), 0);
	ASSERT_EQ(uci_transport_attach(&uci, &transport), 0);

	uint8_t payload[4];
	struct uci_blk packet = CREATE_BLOCK(payload);
	packet.len = sizeof(payload);
	packet.total_len = 0;
	EXPECT_CALL(allocator, free(&packet));

	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_NOTIFICATION,
					     UCI_GID_CORE,
					     UCI_OID_CORE_DEVICE_STATUS);
	uci_blk_put_control_header(&packet, mt_gid_oid, packet.total_len);

	uci_packet_recv(&uci, &packet);

	uci_transport_detach(&uci);
	uci_uninit(&uci);
}

TEST(UciCore, TestGetPayloadSize)
{
	uint8_t hdr[4];
	hdr[1] = 0;
	hdr[2] = 1;
	hdr[3] = 2;

	hdr[0] = UCI_MESSAGE_TYPE_COMMAND << 5;
	ASSERT_EQ(uci_packet_hdr_get_payload_size(&hdr[0]), 2)
		<< "CMD has 1 byte length";
	hdr[0] = UCI_MESSAGE_TYPE_RESPONSE << 5;
	ASSERT_EQ(uci_packet_hdr_get_payload_size(&hdr[0]), 2)
		<< "RSP has 1 byte length";
	hdr[0] = UCI_MESSAGE_TYPE_NOTIFICATION << 5;
	ASSERT_EQ(uci_packet_hdr_get_payload_size(&hdr[0]), 2)
		<< "NTF has 1 byte length";

	hdr[0] = UCI_MESSAGE_TYPE_SE_TESTING_COMMAND << 5;
	ASSERT_EQ(uci_packet_hdr_get_payload_size(&hdr[0]), 513)
		<< "SE CMD has 2 byte length in LE";
	hdr[0] = UCI_MESSAGE_TYPE_SE_TESTING_RESPONSE << 5;
	ASSERT_EQ(uci_packet_hdr_get_payload_size(&hdr[0]), 513)
		<< "SE RSP has 2 byte length in LE";
}

TEST_F(TestUciInit, MemoryLimitOnInit)
{
	StrictMock<MockUciAllocator> allocator;
	struct uci uci;

	/* Uci will pre-allocate a uci_blk for an empty response. */
	EXPECT_CALL(allocator, alloc(UCI_STATUS_PACKET_SIZE, 0))
		.WillOnce(Return(nullptr));
	EXPECT_EQ(uci_init(&uci, &allocator, false), 0);
	uci_uninit(&uci);
}

TEST_F(TestUciInit, FailedPreAllocationRecovery)
{
	StrictMock<MockUciTransport> transport;
	StrictMock<MockUciAllocator> allocator;
	struct uci uci;
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_DEINIT);
	uint8_t payload[UCI_STATUS_PACKET_SIZE];
	struct uci_blk block = { .next = nullptr,
				 .data = static_cast<uint8_t *>(payload),
				 .len = 0,
				 .total_len = 0,
				 .size = UCI_STATUS_PACKET_SIZE,
				 .flags = 0 };

	/* Uci will pre-allocate a uci_blk for an empty response. */
	EXPECT_CALL(allocator, alloc(UCI_STATUS_PACKET_SIZE, 0))
		.WillOnce(Return(nullptr));
	EXPECT_EQ(uci_init(&uci, &allocator, false), 0);
	ASSERT_EQ(uci_transport_attach(&uci, &transport), 0);
	ASSERT_TRUE(Mock::VerifyAndClearExpectations(&allocator));

	EXPECT_CALL(allocator, alloc(UCI_STATUS_PACKET_SIZE, 0))
		/* allocation for the RSP. */
		.WillOnce(Return(&block))
		/* Pre-allocation for next msg, we can say it fails. */
		.WillOnce(Return(nullptr));
	EXPECT_CALL(allocator, free(&block));
	EXPECT_CALL(transport, Out(IsResponseStatus(UCI_GID_SESSION_CONFIG,
						    UCI_OID_SESSION_DEINIT,
						    UCI_STATUS_FAILED)))
		.WillOnce(Return(0));
	uci_send_status(&uci, mt_gid_oid, UCI_STATUS_FAILED);

	ASSERT_TRUE(Mock::VerifyAndClearExpectations(&allocator));
	ASSERT_TRUE(Mock::VerifyAndClearExpectations(&transport));

	uci_uninit(&uci);
}

TEST_F(TestUciInit, SendStatusPreAllocationTooShort)
{
	StrictMock<MockUciTransport> transport;
	StrictMock<MockUciAllocator> allocator;
	struct uci uci;
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_DEINIT);
	// We will return a payload smaller than UCI_STATUS_PACKET_SIZE.
	uint8_t payload[UCI_PACKET_HEADER_SIZE];
	struct uci_blk block = { .next = nullptr,
				 .data = static_cast<uint8_t *>(payload),
				 .len = 0,
				 .total_len = 0,
				 .size = UCI_PACKET_HEADER_SIZE,
				 .flags = 0 };

	/* Uci will pre-allocate a uci_blk for an empty response. */
	EXPECT_CALL(allocator, alloc(UCI_STATUS_PACKET_SIZE, 0))
		.WillOnce(Return(nullptr));
	EXPECT_EQ(uci_init(&uci, &allocator, false), 0);
	ASSERT_EQ(uci_transport_attach(&uci, &transport), 0);
	ASSERT_TRUE(Mock::VerifyAndClearExpectations(&allocator));

	EXPECT_CALL(allocator, alloc(UCI_STATUS_PACKET_SIZE, 0))
		.WillOnce(Return(&block));
	EXPECT_CALL(allocator, free(&block));
	uci_send_status(&uci, mt_gid_oid, UCI_STATUS_FAILED);

	ASSERT_TRUE(Mock::VerifyAndClearExpectations(&allocator));
	ASSERT_TRUE(Mock::VerifyAndClearExpectations(&transport));

	uci_uninit(&uci);
}

TEST_F(TestUciInit, SendMessagePreAllocationTooShort)
{
	StrictMock<MockUciTransport> transport;
	StrictMock<MockUciAllocator> allocator;
	struct uci uci;
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_DEINIT);
	// We will return a payload smaller than UCI_STATUS_PACKET_SIZE.
	uint8_t payload[UCI_PACKET_HEADER_SIZE];
	struct uci_blk block = { .next = nullptr,
				 .data = static_cast<uint8_t *>(payload),
				 .len = 0,
				 .total_len = 0,
				 .size = UCI_PACKET_HEADER_SIZE,
				 .flags = 0 };

	/* Uci will pre-allocate a uci_blk for an empty response. */
	EXPECT_CALL(allocator, alloc(UCI_STATUS_PACKET_SIZE, 0))
		.WillOnce(Return(nullptr));
	EXPECT_EQ(uci_init(&uci, &allocator, false), 0);
	ASSERT_EQ(uci_transport_attach(&uci, &transport), 0);
	ASSERT_TRUE(Mock::VerifyAndClearExpectations(&allocator));

	EXPECT_CALL(allocator, alloc(UCI_STATUS_PACKET_SIZE, 0))
		.WillOnce(Return(&block));
	EXPECT_CALL(allocator, free(&block));
	uci_send_message(&uci, mt_gid_oid, NULL);

	ASSERT_TRUE(Mock::VerifyAndClearExpectations(&allocator));
	ASSERT_TRUE(Mock::VerifyAndClearExpectations(&transport));

	uci_uninit(&uci);
}

TEST_F(TestUciInit, MemoryLimitOnStatus)
{
	StrictMock<MockUciTransport> transport;
	StrictMock<MockUciAllocator> allocator;
	struct uci uci;
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_NOTIFICATION,
					     UCI_GID_CORE,
					     UCI_OID_CORE_DEVICE_STATUS);

	/* Uci will pre-allocate a uci_blk for an empty response. */
	EXPECT_CALL(allocator, alloc(UCI_STATUS_PACKET_SIZE, 0))
		.WillOnce(Return(nullptr))
		.WillOnce(Return(nullptr));
	EXPECT_EQ(uci_init(&uci, &allocator, false), 0);
	ASSERT_EQ(uci_transport_attach(&uci, &transport), 0);

	uci_send_status(&uci, mt_gid_oid, 3);
	uci_uninit(&uci);
}

// Test suite with an initialized uci_core
class TestUciCore : public ::testing::Test {
    public:
	void SetUp() override
	{
		/* Uci will pre-allocate a uci_blk for an empty response. */
		expect_alloc(&allocator, UCI_STATUS_PACKET_SIZE);
		ASSERT_EQ(uci_init(&uci, &allocator, false), 0);
		ASSERT_EQ(uci_transport_attach(&uci, &transport), 0);
	}
	void TearDown() override
	{
		ASSERT_TRUE(Mock::VerifyAndClearExpectations(&transport));
		uci_transport_detach(&uci);
		uci_uninit(&uci);
	}

    public:
	struct uci uci;
	StrictMock<MockUciAllocator> allocator;
	StrictMock<MockUciTransport> transport;
};

TEST_F(TestUciCore, PacketRecvAlloc)
{
	struct uci_blk block;

	EXPECT_CALL(allocator, alloc(10, 0)).WillOnce(Return(&block));

	ASSERT_EQ(uci_packet_recv_alloc(&uci, 10), &block);
	EXPECT_EQ(block.next, nullptr);
	EXPECT_EQ(block.len, 0u);
	EXPECT_EQ(block.total_len, 0u);
	EXPECT_EQ(block.flags, 0u);
}

TEST_F(TestUciCore, PacketRecvFreeAll)
{
	struct uci_blk block;
	block.flags = 0;
	block.next = nullptr;
	EXPECT_CALL(allocator, free(&block));
	uci_packet_recv_free_all(&uci, &block);
}

TEST_F(TestUciCore, Alloc)
{
	struct uci_blk block;

	EXPECT_CALL(allocator, alloc).WillOnce(Return(&block));

	ASSERT_EQ(uci_blk_alloc(&uci, 0, 0), &block);
	EXPECT_EQ(block.next, nullptr);
	EXPECT_EQ(block.len, 0u);
	EXPECT_EQ(block.total_len, 0u);
	EXPECT_EQ(block.flags, 0u);
}

TEST_F(TestUciCore, AllocFailure)
{
	EXPECT_CALL(allocator, alloc).WillOnce(Return(nullptr));
	ASSERT_EQ(uci_blk_alloc(&uci, 0, 0), nullptr);
}

TEST_F(TestUciCore, StackAllocNoFree)
{
	struct uci_blk block = { .next = nullptr,
				 .data = nullptr,
				 .len = 0,
				 .total_len = 0,
				 .size = 0,
				 .flags = UCI_BLK_FLAGS_STATIC };

	EXPECT_CALL(allocator, free(&block)).Times(0);
	uci_blk_free_all(&uci, &block);
}

static void custom_free(void *arg, struct uci_blk_destructible *blk)
{
	(void)blk;
	bool *called = static_cast<bool *>(arg);
	*called = true;
}

TEST_F(TestUciCore, CustomFree)
{
	bool custom_free_called = false;
	struct uci_blk_destructible blk_destructible = {
		.blk = { .next = nullptr,
			 .data = nullptr,
			 .len = 0,
			 .total_len = 0,
			 .size = 0,
			 .flags = UCI_BLK_FLAGS_DESTRUCTIBLE },
		.destructor = custom_free,
		.destructor_arg = &custom_free_called,
	};

	uci_blk_free_all(&uci,
			 reinterpret_cast<struct uci_blk *>(&blk_destructible));
	EXPECT_TRUE(custom_free_called);
}

TEST_F(TestUciCore, FreeOneBlock)
{
	struct uci_blk block;
	block.flags = 0;
	block.next = nullptr;
	EXPECT_CALL(allocator, free(&block));
	uci_blk_free_all(&uci, &block);
}

TEST_F(TestUciCore, FreeSeveralBlocks)
{
	InSequence s;
	struct uci_blk b1, b2, b3;
	b1.flags = 0;
	b2.flags = 0;
	b3.flags = 0;

	b1.next = &b2;
	b2.next = &b3;
	b3.next = nullptr;

	EXPECT_CALL(allocator, free(&b1));
	EXPECT_CALL(allocator, free(&b2));
	EXPECT_CALL(allocator, free(&b3));

	uci_blk_free_all(&uci, &b1);
}

TEST_F(TestUciCore, FreeDoNotFreeNullptr)
{
	uci_blk_free_all(&uci, nullptr);
}

TEST_F(TestUciCore, TestSimpleMessagePut)
{
	uint8_t payload[16];
	struct uci_blk block = CREATE_BLOCK(payload);
	EXPECT_CALL(allocator, alloc).WillOnce(Return(&block));
	uci_message_builder builder = UCI_MESSAGE_BUILDER_INITIALIZER(&uci);

	uint8_t gid = UCI_STATUS_UNKNOWN_GID;
	ASSERT_EQ(uci_message_put(&builder, &gid, sizeof(gid)), 0);
	ASSERT_EQ(uci_message_failed(&builder), 0);
}

TEST_F(TestUciCore, TestMessagePutFailAllocateFirstBlock)
{
	struct uci_message_builder builder =
		UCI_MESSAGE_BUILDER_INITIALIZER(&uci);
	uint8_t data[] = { 0xde, 0xad, 0xbe, 0xef, '\0' };

	EXPECT_CALL(allocator, alloc).WillOnce(Return(nullptr));
	ASSERT_EQ(uci_message_put(&builder, data, sizeof(data)),
		  UCI_STATUS_FAILED);
	ASSERT_EQ(uci_message_failed(&builder), UCI_STATUS_FAILED);
}

TEST_F(TestUciCore, TestMessagePutFailAllocateSecondBlock)
{
	uint8_t payload[8];
	struct uci_blk block = CREATE_BLOCK(payload);

	struct uci_message_builder builder =
		UCI_MESSAGE_BUILDER_INITIALIZER(&uci);
	uint8_t data[] = { 0xde, 0xad, 0xbe, 0xef, '\0' };

	EXPECT_CALL(allocator, alloc)
		.WillOnce(Return(&block))
		.WillOnce(Return(nullptr));

	ASSERT_EQ(uci_message_put(&builder, data, sizeof(data)),
		  UCI_STATUS_FAILED);
	ASSERT_EQ(uci_message_failed(&builder), UCI_STATUS_FAILED);
}

TEST_F(TestUciCore, TestMessagePutFailErrorSecondBlock256)
{
	uint8_t payload[UCI_MAX_CONTROL_PACKET_SIZE];
	struct uci_blk block = CREATE_BLOCK(payload);

	struct uci_message_builder builder =
		UCI_MESSAGE_BUILDER_INITIALIZER(&uci);
	uint8_t data[] = { 0xde, 0xad, 0xbe, 0xef, '\0' };

	EXPECT_CALL(allocator, alloc)
		.WillOnce(Return(&block))
		.WillOnce(Return(nullptr));

	// fill the first packet entirely 51 * 5 == 255
	for (int i = 0; i < UCI_MAX_CONTROL_PACKET_SIZE / 5; i++) {
		ASSERT_EQ(uci_message_put(&builder, data, sizeof(data)), 0);
	}

	ASSERT_EQ(uci_message_put(&builder, data, 1), UCI_STATUS_FAILED);
	ASSERT_EQ(uci_message_failed(&builder), UCI_STATUS_FAILED);
}

TEST_F(TestUciCore, TestMessagePutFailBlockTooSmall)
{
	InSequence s;
	uint8_t payload[3];
	struct uci_blk block = CREATE_BLOCK(payload);
	struct uci_message_builder builder =
		UCI_MESSAGE_BUILDER_INITIALIZER(&uci);
	uint8_t data[] = { 0xde, 0xad, 0xbe, 0xef, '\0' };

	EXPECT_CALL(allocator, alloc).WillOnce(Return(&block));
	EXPECT_CALL(allocator, free(&block));

	EXPECT_EQ(uci_message_put(&builder, data, sizeof(data)),
		  UCI_STATUS_FAILED);
	ASSERT_EQ(uci_message_failed(&builder), UCI_STATUS_FAILED);
}

TEST_F(TestUciCore, TestMessagePutBlock24_NoFragmentation)
{
	// Test multiple put on a single block of 24 bytes.
	uint8_t payload[30];
	struct uci_blk block = CREATE_BLOCK(payload);
	struct uci_message_builder builder =
		UCI_MESSAGE_BUILDER_INITIALIZER(&uci);
	uint8_t data[] = { 0xde, 0xad, 0xbe, 0xef, '\0' };

	EXPECT_CALL(allocator, alloc).WillOnce(Return(&block));

	ASSERT_EQ(uci_message_put(&builder, data, sizeof(data)), 0);
	ASSERT_EQ(uci_message_put_32bit(&builder, 42), 0);
	ASSERT_EQ(uci_message_put_16bit(&builder, 43), 0);
	ASSERT_EQ(uci_message_put_8bit(&builder, 44), 0);
	ASSERT_EQ(uci_message_put_64bit(&builder, 0x0123456789abcdef), 0);
	ASSERT_EQ(uci_message_put_48bit(&builder, 0x123456789abc), 0);
	ASSERT_EQ(uci_message_failed(&builder), 0);

	EXPECT_EQ(block.next, nullptr);
	EXPECT_EQ(block.len, UCI_PACKET_HEADER_SIZE + 5 + 4 + 2 + 1 + 8 + 6);
	EXPECT_EQ(block.total_len, 5u + 4 + 2 + 1 + 8 + 6);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 0], 0xde);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 1], 0xad);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 2], 0xbe);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 3], 0xef);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 4], '\0');
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 5], 42);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 6], 0);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 7], 0);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 8], 0);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 9], 43);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 10], 0);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 11], 44);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 12], 0xef);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 13], 0xcd);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 14], 0xab);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 15], 0x89);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 16], 0x67);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 17], 0x45);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 18], 0x23);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 19], 0x01);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 20], 0xbc);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 21], 0x9a);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 22], 0x78);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 23], 0x56);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 24], 0x34);
	EXPECT_EQ(payload[UCI_PACKET_HEADER_SIZE + 25], 0x12);
}

class TestUciMessagePut : public TestUciCore {};

TEST_F(TestUciMessagePut, WithFragmentation)
{
	// Test multiple put on two blocks of 8 bytes.
	uint8_t payload1[8];
	struct uci_blk b1 = CREATE_BLOCK(payload1);
	uint8_t payload2[8];
	struct uci_blk b2 = CREATE_BLOCK(payload2);

	struct uci_message_builder builder =
		UCI_MESSAGE_BUILDER_INITIALIZER(&uci);
	uint8_t data[] = { 0xde, 0xad, 0xbe, 0xef, '\0' };

	EXPECT_CALL(allocator, alloc)
		.WillOnce(Return(&b1))
		.WillOnce(Return(&b2));

	ASSERT_EQ(uci_message_put(&builder, data, sizeof(data)), 0);
	ASSERT_EQ(uci_message_put_32bit(&builder, 42), 0);
	ASSERT_EQ(uci_message_put_16bit(&builder, 43), 0);
	ASSERT_EQ(uci_message_put_8bit(&builder, 44), 0);
	ASSERT_EQ(uci_message_failed(&builder), 0);

	EXPECT_EQ(b1.next, &b2);
	EXPECT_EQ(b2.next, nullptr);

	EXPECT_EQ(b1.total_len, 12u);
	EXPECT_EQ(b1.len, 8u);
	EXPECT_EQ(b2.total_len, 0u);
	EXPECT_EQ(b2.len, 12u - 8 + 4);
	EXPECT_EQ(payload1[UCI_PACKET_HEADER_SIZE + 0], 0xde);
	EXPECT_EQ(payload1[UCI_PACKET_HEADER_SIZE + 1], 0xad);
	EXPECT_EQ(payload1[UCI_PACKET_HEADER_SIZE + 2], 0xbe);
	EXPECT_EQ(payload1[UCI_PACKET_HEADER_SIZE + 3], 0xef);
	EXPECT_EQ(payload2[0], '\0');
	EXPECT_EQ(payload2[1], 42);
	EXPECT_EQ(payload2[2], 0);
	EXPECT_EQ(payload2[3], 0);
	EXPECT_EQ(payload2[4], 0);
	EXPECT_EQ(payload2[5], 43);
	EXPECT_EQ(payload2[6], 0);
	EXPECT_EQ(payload2[7], 44);
}

TEST_F(TestUciMessagePut, WithFragmentationAndNesting)
{
	// Test multiple put on two blocks of 8 bytes.
	uint8_t payload1[8];
	struct uci_blk b1 = CREATE_BLOCK(payload1);
	uint8_t payload2[8];
	struct uci_blk b2 = CREATE_BLOCK(payload2);

	struct uci_message_builder builder =
		UCI_MESSAGE_BUILDER_INITIALIZER(&uci);
	uint8_t data[] = { 0xde, 0xad, 0xbe, 0xef, '\0' };

	EXPECT_CALL(allocator, alloc)
		.WillOnce(Return(&b1))
		.WillOnce(Return(&b2));

	// Start first nest before first put
	struct uci_message_builder nest_builder_1;
	uci_message_start_nest(&builder, &nest_builder_1);

	ASSERT_EQ(uci_message_put(&nest_builder_1, data, sizeof(data)), 0);

	// Start second nest after first put
	struct uci_message_builder nest_builder_2;
	uci_message_start_nest(&nest_builder_1, &nest_builder_2);

	ASSERT_EQ(uci_message_put_8bit(&nest_builder_2, 42), 0);

	uci_message_end_nest(&nest_builder_1, &nest_builder_2);
	ASSERT_EQ(uci_message_nest_get_elems_nr(&nest_builder_2), 1);

	ASSERT_EQ(uci_message_put_16bit(&nest_builder_1, 43), 0);

	uci_message_end_nest(&builder, &nest_builder_1);
	ASSERT_EQ(uci_message_nest_get_elems_nr(&nest_builder_1), 3);
	// Force an updated number of elements
	uci_message_nest_set_elems_nr(&nest_builder_1, 5);

	ASSERT_EQ(uci_message_put_16bit(&builder, 44), 0);
	ASSERT_EQ(uci_message_failed(&builder), 0);
	// This must not crash
	uci_message_nest_set_elems_nr(&builder, 5);

	EXPECT_EQ(b1.next, &b2);
	EXPECT_EQ(b2.next, nullptr);

	EXPECT_EQ(b1.total_len, 12u);
	EXPECT_EQ(b1.len, 8u);
	EXPECT_EQ(b2.total_len, 0u);
	EXPECT_EQ(b2.len, 12u - 8 + 4);
	EXPECT_EQ(payload1[UCI_PACKET_HEADER_SIZE + 0], 5);
	EXPECT_EQ(payload1[UCI_PACKET_HEADER_SIZE + 1], 0xde);
	EXPECT_EQ(payload1[UCI_PACKET_HEADER_SIZE + 2], 0xad);
	EXPECT_EQ(payload1[UCI_PACKET_HEADER_SIZE + 3], 0xbe);
	EXPECT_EQ(payload2[0], 0xef);
	EXPECT_EQ(payload2[1], '\0');
	EXPECT_EQ(payload2[2], 1);
	EXPECT_EQ(payload2[3], 42);
	EXPECT_EQ(payload2[4], 43);
	EXPECT_EQ(payload2[5], 0);
	EXPECT_EQ(payload2[6], 44);
	EXPECT_EQ(payload2[7], 0);
}

class TestUciParser : public ::testing::Test {};

TEST_F(TestUciParser, TestParserWithNoFragmentation)
{
	// Parsing a message from a single packet
	uint8_t payload[] = { 0,    0,	  0,	0, /* header */
			      0xde, 0xad, 0xbe, 0xef, '\0', 42,	  0,
			      0,    0,	  43,	0,    44,   0xef, 0xcd,
			      0xab, 0x89, 0x67, 0x45, 0x23, 0x01, 0x55,
			      0x44, 0x33, 0x22, 0x11, 0x00 };
	uint8_t tmp[5];
	struct uci_blk block = {
		.next = NULL,
		.data = payload,
		.len = sizeof(payload),
		.total_len = sizeof(payload) - 4,
		.size = sizeof(payload),
		.flags = UCI_BLK_FLAGS_HEADER_RESERVED,
	};
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(&block);

	EXPECT_EQ(uci_message_remaining(&parser), 26u);
	EXPECT_EQ(uci_message_get(&parser, tmp, sizeof(tmp)), sizeof(tmp));
	EXPECT_EQ(tmp[0], 0xde);
	EXPECT_EQ(tmp[1], 0xad);
	EXPECT_EQ(tmp[2], 0xbe);
	EXPECT_EQ(tmp[3], 0xef);
	EXPECT_EQ(tmp[4], '\0');
	EXPECT_EQ(uci_message_remaining(&parser), 21u);
	EXPECT_EQ(uci_message_get_32bit(&parser), 42u);
	EXPECT_EQ(uci_message_remaining(&parser), 17u);
	EXPECT_EQ(uci_message_get_16bit(&parser), 43u);
	EXPECT_EQ(uci_message_remaining(&parser), 15u);
	EXPECT_EQ(uci_message_get_8bit(&parser), 44u);
	EXPECT_EQ(uci_message_remaining(&parser), 14u);
	EXPECT_EQ(uci_message_get_64bit(&parser), 0x0123456789abcdefu);
	EXPECT_EQ(uci_message_remaining(&parser), 6u);
	EXPECT_EQ(uci_message_get_48bit(&parser), 0x1122334455u);
	EXPECT_EQ(uci_message_remaining(&parser), 0u);
}

class UciCoreParserSkip : public testing::TestWithParam<size_t> {};

INSTANTIATE_TEST_SUITE_P(SkipRange, UciCoreParserSkip,
			 testing::Range(1ul, 10ul));

TEST_P(UciCoreParserSkip, SkipSuccess)
{
	uint8_t payload2[] = { 6, 7, 8, 9, 10 };
	struct uci_blk block2 = {
		.next = NULL,
		.data = payload2,
		.len = sizeof(payload2),
		.total_len = 0,
		.size = sizeof(payload2),
		.flags = 0,
	};
	uint8_t payload[] = {
		0, 0, 0, 0, /* header */
		0, 1, 2, 3, 4, 5 /* payload */
	};
	struct uci_blk block = {
		.next = &block2,
		.data = payload,
		.len = sizeof(payload),
		.total_len = sizeof(payload) + sizeof(payload2) - 4,
		.size = sizeof(payload),
		.flags = UCI_BLK_FLAGS_HEADER_RESERVED,
	};
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(&block);
	size_t p = GetParam();
	ASSERT_EQ(p, uci_message_skip(&parser, p));
	ASSERT_EQ(uci_message_remaining(&parser), 11 - p);
	ASSERT_EQ(uci_message_get_8bit(&parser), static_cast<uint8_t>(p));
}

TEST(UciCoreParserSkip, SkipFailure)
{
	uint8_t payload[] = { 0,    0,	  0,   0, /* header */
			      0x00, 0x01, 0x02 };
	struct uci_blk block = {
		.next = NULL,
		.data = payload,
		.len = sizeof(payload),
		.total_len = sizeof(payload) - 4,
		.size = sizeof(payload),
		.flags = UCI_BLK_FLAGS_HEADER_RESERVED,
	};
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(&block);
	ASSERT_EQ(3, uci_message_skip(&parser, 5));
}

TEST_F(TestUciParser, TestParserWithRoomAndNoFragmentation)
{
	// Parsing a message from a single packet with room.
	uint8_t payload[] = { 0,    0,	  0,	0,    0xde, 0xad, 0xbe,
			      0xef, '\0', 42,	0,    0,    0,	  43,
			      0,    44,	  0xff, 0xff, 0xff, 0xff, 0xff,
			      0xff, 0xff, 0xff, 0xff, 0xff };
	uint8_t tmp[5];
	struct uci_blk block = {
		.next = NULL,
		.data = payload,
		.len = 16,
		.total_len = sizeof(payload) - 4,
		.size = sizeof(payload),
		.flags = UCI_BLK_FLAGS_HEADER_RESERVED,
	};
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(&block);

	EXPECT_GE(uci_message_remaining(&parser), 12u);
	EXPECT_EQ(uci_message_get(&parser, tmp, sizeof(tmp)), sizeof(tmp));
	EXPECT_EQ(tmp[0], 0xde);
	EXPECT_EQ(tmp[1], 0xad);
	EXPECT_EQ(tmp[2], 0xbe);
	EXPECT_EQ(tmp[3], 0xef);
	EXPECT_EQ(tmp[4], '\0');
	EXPECT_GE(uci_message_remaining(&parser), 7u);
	EXPECT_EQ(uci_message_get_32bit(&parser), 42u);
	EXPECT_GE(uci_message_remaining(&parser), 3u);
	EXPECT_EQ(uci_message_get_16bit(&parser), 43u);
	EXPECT_GE(uci_message_remaining(&parser), 1u);
	EXPECT_EQ(uci_message_get_8bit(&parser), 44u);
	EXPECT_GE(uci_message_remaining(&parser), 0u);
}

TEST_F(TestUciParser, BlockWithFragmentation)
{
	// Parsing a message from 3 packets.
	uint8_t payload[] = {
		0, 0, 0,  0, 0xde, 0xad, 0xbe, 0xef, '\0', 42,	 0,
		0, 0, 43, 0, 44,   0xff, 0xff, 0xff, 0xff, 0xff,
	};
	uint8_t tmp[5];
	struct uci_blk b3 = {
    .next = nullptr,
		.data = payload + 7 + 7,
		.len = 2,
		.total_len = 0,
		.size = 7,
		.flags = 0,
	}, b2 = {
    .next = &b3,
		.data = payload + 7,
		.len = 7,
		.total_len = 0,
		.size = 7,
		.flags = 0,
	}, b1 = {
    .next = &b2,
		.data = payload,
		.len = 7,
		.total_len = 7 - 4 + 7 + 2,
		.size = 7,
		.flags = UCI_BLK_FLAGS_HEADER_RESERVED,
  };

	struct uci_message_parser parser = UCI_MESSAGE_PARSER_INITIALIZER(&b1);

	EXPECT_EQ(uci_message_remaining(&parser), 12u);
	EXPECT_EQ(uci_message_get(&parser, tmp, sizeof(tmp)), sizeof(tmp));
	EXPECT_EQ(tmp[0], 0xde);
	EXPECT_EQ(tmp[1], 0xad);
	EXPECT_EQ(tmp[2], 0xbe);
	EXPECT_EQ(tmp[3], 0xef);
	EXPECT_EQ(tmp[4], '\0');
	EXPECT_EQ(uci_message_remaining(&parser), 7u);
	EXPECT_EQ(uci_message_get_32bit(&parser), 42u);
	EXPECT_EQ(uci_message_remaining(&parser), 3u);
	EXPECT_EQ(uci_message_get_16bit(&parser), 43u);
	EXPECT_EQ(uci_message_remaining(&parser), 1u);
	EXPECT_EQ(uci_message_get_8bit(&parser), 44u);
	EXPECT_EQ(uci_message_remaining(&parser), 0u);
}

TEST_F(TestUciParser, BlockWithFragmentation2)
{
	// Parsing a message from 3 packets.
	uint8_t payload[] = {
		0, 0, 0,  0, 0xde, 0xad, 0xbe, 0xef, '\0', 42,	 0,
		0, 0, 43, 0, 0,	   0,	 0,    0,    44,   0xff,
	};
	uint8_t tmp[5];
	struct uci_blk b3 = {
    .next = nullptr,
		.data = payload + 7 + 7,
		.len = 6,
		.total_len = 0,
		.size = 7,
		.flags = UCI_BLK_FLAGS_HEADER_RESERVED,
	}, b2 = {
    .next = &b3,
		.data = payload + 7,
		.len = 7,
		.total_len = 0,
		.size = 7,
		.flags = 0,
	}, b1 = {
    .next = &b2,
		.data = payload,
		.len = 7,
		.total_len = 7 - 4 + 7 + 6 - 4,
		.size = 7,
		.flags = UCI_BLK_FLAGS_HEADER_RESERVED,
	};

	struct uci_message_parser parser = UCI_MESSAGE_PARSER_INITIALIZER(&b1);

	EXPECT_EQ(uci_message_remaining(&parser), 12u);
	EXPECT_EQ(uci_message_get(&parser, tmp, sizeof(tmp)), sizeof(tmp));
	EXPECT_EQ(tmp[0], 0xde);
	EXPECT_EQ(tmp[1], 0xad);
	EXPECT_EQ(tmp[2], 0xbe);
	EXPECT_EQ(tmp[3], 0xef);
	EXPECT_EQ(tmp[4], '\0');
	EXPECT_EQ(uci_message_remaining(&parser), 7u);
	EXPECT_EQ(uci_message_get_32bit(&parser), 42u);
	EXPECT_EQ(uci_message_remaining(&parser), 3u);
	EXPECT_EQ(uci_message_get_16bit(&parser), 43u);
	EXPECT_EQ(uci_message_remaining(&parser), 1u);
	EXPECT_EQ(uci_message_get_8bit(&parser), 44u);
	EXPECT_EQ(uci_message_remaining(&parser), 0u);
}

template <size_t N> struct MockHandler : MockHandler<N - 1> {
	MOCK_METHOD(enum qerr, handler,
		    (struct uci * uci, uint16_t mt_gid_oid,
		     const struct uci_blk *payload));

	void setup(struct uci_message_handler *handlers)
	{
		handlers->mt_gid_oid = 0;
		handlers->handler = static_handler;

		MockHandler<N - 1>::setup(handlers - 1);
	}

	static enum qerr static_handler(struct uci *uci, uint16_t mt_gid_oid,
					const struct uci_blk *payload,
					void *user_data)
	{
		return static_cast<MockHandler<N> *>(user_data)->handler(
			uci, mt_gid_oid, payload);
	}
};

template <> struct MockHandler<0> {
	MOCK_METHOD(enum qerr, handler,
		    (struct uci * uci, uint16_t mt_gid_oid,
		     const struct uci_blk *payload));

	void setup(struct uci_message_handler *handlers)
	{
		handlers->mt_gid_oid = 0;
		handlers->handler = static_handler;
	}

	static enum qerr static_handler(struct uci *uci, uint16_t mt_gid_oid,
					const struct uci_blk *payload,
					void *user_data)
	{
		return static_cast<MockHandler<0> *>(user_data)->handler(
			uci, mt_gid_oid, payload);
	}
};

template <size_t N>
struct MockHandlers : MockHandler<N - 1>, uci_message_handlers {
	MockHandlers()
	{
		MockHandler<N - 1>::setup(handlers + N - 1);

		uci_message_handlers::handlers = handlers;
		uci_message_handlers::n_handlers = N;
		uci_message_handlers::user_data = this;
	}

	template <size_t I> void set(uint16_t mt_gid_oid)
	{
		static_assert(I < N, "");
		handlers[I].mt_gid_oid = mt_gid_oid;
	}

	struct uci_message_handler handlers[N];
};

TEST_F(TestUciCore, TestRegisterWithNullptr)
{
	uci_message_handlers_register(&uci, nullptr);
}

TEST_F(TestUciCore, TestRegister)
{
	MockHandlers<3> handlers;
	uint16_t mt_gid_oid = 0x4501;
	handlers.set<0>(mt_gid_oid);
	handlers.set<1>(mt_gid_oid + 1);
	handlers.set<2>(mt_gid_oid + 2);
	uci_message_handlers_register(&uci, &handlers);
	EXPECT_EQ(uci.known_gid, 1u << 5);
}

TEST_F(TestUciCore, TestUnregister)
{
	MockHandlers<3> handlers;
	uint16_t mt_gid_oid = 0x0501;
	handlers.set<0>(mt_gid_oid);
	handlers.set<1>(mt_gid_oid + 1);
	handlers.set<2>(mt_gid_oid + 2);

	uci_message_handlers_register(&uci, &handlers);
	EXPECT_EQ(uci.known_gid, 1u << 5);
	uci_message_handlers_unregister(&uci, &handlers);
	EXPECT_EQ(uci.known_gid, 0u);
}

TEST_F(TestUciCore, TestUnregisterNotRegistered)
{
	MockHandlers<1> handlers;
	MockHandlers<1> handlers2;
	uint16_t mt_gid_oid = 0x0501;
	handlers.set<0>(mt_gid_oid);
	handlers2.set<0>(mt_gid_oid + 0x100);

	uci_message_handlers_register(&uci, &handlers);
	unsigned known = uci.known_gid;
	EXPECT_EQ(known, 1u << 5);
	uci_message_handlers_unregister(&uci, &handlers2);
	known = uci.known_gid;
	EXPECT_EQ(known, 1u << 5);
}

TEST_F(TestUciCore, TestTwoHandlersForSameGID)
{
	MockHandlers<1> handlers;
	MockHandlers<1> handlers2;
	uint16_t mt_gid_oid = 0x4321;
	uint8_t payload[16];
	struct uci_blk packet = CREATE_BLOCK(payload);
	packet.len = 8;
	/* First handler called for each packet. */
	handlers.set<0>(mt_gid_oid);
	EXPECT_CALL(static_cast<MockHandler<0> &>(handlers),
		    handler(&uci, mt_gid_oid, &packet))
		.WillRepeatedly([&]() { return QERR_SUCCESS; });
	/* Second handler not called as handled by first one. */
	handlers2.set<0>(mt_gid_oid);

	EXPECT_CALL(allocator, free(&packet)).Times(1);

	uci_message_handlers_register(&uci, &handlers2);
	uci_message_handlers_register(&uci, &handlers);

	payload[0] = (mt_gid_oid >> 8) & 0xff;
	payload[1] = mt_gid_oid & 0xff;

	uci_packet_recv(&uci, &packet);
}

constexpr uint16_t kSeCmdMtGid =
	UCI_MT_GID_OID(UCI_MESSAGE_TYPE_SE_TESTING_COMMAND, 0, 0);

/*
 * Test reception and emission of SE testing message types.
 * Messages with these types have a length encoded differently, which changes
 * how UCI core behave.
 */
TEST_F(TestUciCore, TestSeCmd)
{
	MockHandlers<1> handlers;
	handlers.set<0>(kSeCmdMtGid);
	uci_message_handlers_register(&uci, &handlers);
	uint8_t block[6];

	for (int payload_size = 0; payload_size < 3; ++payload_size) {
		block[0] = (kSeCmdMtGid >> 8) & 0xff;
		block[1] = kSeCmdMtGid & 0xff;
		block[2] = payload_size;
		block[3] = 0;
		block[4] = 1;
		block[5] = 2;

		// Packet sent to the handler.
		struct uci_blk packet = CREATE_BLOCK(block);
		packet.flags = UCI_BLK_FLAGS_HEADER_RESERVED;
		packet.len = UCI_PACKET_HEADER_SIZE + payload_size;
		packet.total_len = payload_size;

		// Packet sent by the handler.
		struct uci_blk rsp_block = CREATE_BLOCK(block);
		rsp_block.flags = UCI_BLK_FLAGS_HEADER_RESERVED;
		rsp_block.len = UCI_PACKET_HEADER_SIZE + payload_size;
		rsp_block.total_len = payload_size;

		// Expected payload to be received.
		std::vector<uint8_t> ex_payload = std::vector<uint8_t>(
			block, block + UCI_PACKET_HEADER_SIZE + payload_size);

		// Expect UCI handler is called with our command
		EXPECT_CALL(static_cast<MockHandler<0> &>(handlers),
			    handler(&uci, kSeCmdMtGid, &packet))
			.WillOnce([&](struct uci *uci, uint16_t mt_gid_oid,
				      const struct uci_blk *blk) {
				struct uci_message_parser parser =
					UCI_MESSAGE_PARSER_INITIALIZER(blk);
				EXPECT_EQ(uci_message_remaining(&parser),
					  payload_size)
					<< "remaining length is incorrect";

				std::vector<uint8_t> value =
					std::vector<uint8_t>(blk->data,
							     blk->data +
								     blk->len);
				EXPECT_THAT(value, ::testing::ElementsAreArray(
							   ex_payload))
					<< "Sent block content differ from what we expected";

				// Next time we expect to receive the response
				// we are sending.
				ex_payload[0] =
					UCI_MESSAGE_TYPE_SE_TESTING_RESPONSE
					<< 5;

				// Send the same content, with proper message
				// type.
				uci_send_se_message(
					uci,
					UCI_MESSAGE_TYPE_SE_TESTING_RESPONSE,
					&rsp_block);
				return QERR_SUCCESS;
			});

		// Expect the response sent by our mock handler is properly
		// received.
		EXPECT_CALL(transport, Out(&rsp_block))
			.WillOnce([&](struct uci_blk *blk) {
				struct uci_message_parser parser =
					UCI_MESSAGE_PARSER_INITIALIZER(blk);
				EXPECT_EQ(uci_message_remaining(&parser),
					  payload_size)
					<< "remaining length is incorrect";

				std::vector<uint8_t> value =
					std::vector<uint8_t>(blk->data,
							     blk->data +
								     blk->len);
				EXPECT_THAT(value, ::testing::ElementsAreArray(
							   ex_payload))
					<< "Received block content differ from what we sent";
				return QERR_SUCCESS;
			});

		// Both the command and the response blocks should be free.
		EXPECT_CALL(allocator, free(&packet));
		EXPECT_CALL(allocator, free(&rsp_block));

		// Sends the command.
		uci_packet_recv(&uci, &packet);

		// Stop on first error.
		Mock::VerifyAndClearExpectations(&transport);
		Mock::VerifyAndClearExpectations(
			&static_cast<MockHandler<0> &>(handlers));
		ASSERT_FALSE(HasFailure()) << "Error on i=" << payload_size;
	}
}

TEST_F(TestUciCore, TestReceiveIncorrectPacket)
{
	// Test receive with a packet with a size smaller than the minimum
	// packet size possible.
	MockHandlers<5> handlers;
	uint8_t payload[3];
	struct uci_blk packet = CREATE_BLOCK(payload);
	uint16_t mt_gid_oid = 0x4321;

	handlers.set<3>(mt_gid_oid);
	EXPECT_CALL(static_cast<MockHandler<3> &>(handlers),
		    handler(&uci, mt_gid_oid, &packet))
		.Times(0);

	EXPECT_CALL(allocator, free(&packet));

	uci_message_handlers_register(&uci, &handlers);

	payload[0] = (mt_gid_oid >> 8) & 0xff;
	payload[1] = mt_gid_oid & 0xff;

	uci_packet_recv(&uci, &packet);
}

TEST_F(TestUciCore, TestReceiveIncorrectMessageTypeWithPayload)
{
	uint8_t payload[5] = { 224, 0, 0, 0, 0 };
	struct uci_blk packet = CREATE_BLOCK(payload);
	packet.len = 5;
	packet.total_len = 1;
	EXPECT_CALL(allocator, free(&packet));
	/* Packet should be dropped with a warning without any message sent on
	 * the transport.
	 */
	uci_packet_recv(&uci, &packet);
}

/*
 * Ensure a segmented packet is dropped when receiving a new packet with a
 * different mt/gid/oid.
 */
class TestMessageDropping
	: public TestUciCore,
	  public testing::WithParamInterface<std::tuple<uint16_t, bool> > {};

INSTANTIATE_TEST_SUITE_P(MessageTypes, TestMessageDropping,
			 testing::Values(
				 // CMD, GID 1, OID 1
				 std::make_tuple(0b0010000100000001, false),
				 // RSP, GID 1, OID 1
				 std::make_tuple(0b0100000100000001, true),
				 // NTF, GID 1, OID 1
				 std::make_tuple(0b0110000100000001, true),
				 // SE CMD
				 std::make_tuple(0b1000000000000000, false),
				 // SE RSP
				 std::make_tuple(0b1010000000000000, true)));

TEST_P(TestMessageDropping, DropMessage)
{
	MockHandlers<1> handlers_1;
	uint16_t dropped_mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND, 0, 0);
	handlers_1.set<0>(dropped_mt_gid_oid);
	uci_message_handlers_register(&uci, &handlers_1);

	MockHandlers<1> handlers_2;
	auto param = GetParam();
	uint16_t handled_mt_gid_oid = std::get<0>(param);
	handlers_2.set<0>(handled_mt_gid_oid);
	uci_message_handlers_register(&uci, &handlers_2);
	uci.is_client = std::get<1>(param);

	uint8_t payload_1[4] = { 0 };
	payload_1[0] = 0b00110000; // CMD + segmented
	payload_1[1] = dropped_mt_gid_oid & 0xff;
	struct uci_blk dropped_packet = CREATE_BLOCK(payload_1);
	dropped_packet.len = 4;
	dropped_packet.total_len = 0;
	EXPECT_CALL(static_cast<MockHandler<0> &>(handlers_1), handler).Times(0);

	uci_packet_recv(&uci, &dropped_packet);

	/* Ensure there are no interactions with the handler or the transport */
	ASSERT_TRUE(Mock::VerifyAndClearExpectations(&transport));
	ASSERT_TRUE(Mock::VerifyAndClearExpectations(
		static_cast<MockHandler<0> *>(&handlers_1)));

	uint8_t payload_2[4] = { 0 };
	payload_2[0] = (handled_mt_gid_oid >> 8) & 0xff;
	payload_2[1] = handled_mt_gid_oid & 0xff;
	struct uci_blk handled_packet = CREATE_BLOCK(payload_2);
	handled_packet.len = 4;
	handled_packet.total_len = 0;

	EXPECT_CALL(static_cast<MockHandler<0> &>(handlers_2), handler);
	EXPECT_CALL(allocator, free(&dropped_packet));
	EXPECT_CALL(allocator, free(&handled_packet));

	uci_packet_recv(&uci, &handled_packet);
}

TEST_F(TestUciCore, TestReceiveNotFragmented)
{
	MockHandlers<5> handlers;
	uint8_t payload[16];
	struct uci_blk packet = CREATE_BLOCK(payload);
	packet.len = 8;
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_NOTIFICATION,
					     UCI_GID_CORE,
					     UCI_OID_CORE_DEVICE_STATUS);

	handlers.set<3>(mt_gid_oid);
	EXPECT_CALL(static_cast<MockHandler<3> &>(handlers),
		    handler(&uci, mt_gid_oid, &packet));

	EXPECT_CALL(allocator, free(&packet));

	uci_message_handlers_register(&uci, &handlers);

	payload[0] = (mt_gid_oid >> 8) & 0xff;
	payload[1] = mt_gid_oid & 0xff;

	uci_packet_recv(&uci, &packet);
}

TEST_F(TestUciCore, TestReceiveTwice)
{
	InSequence s;
	MockHandlers<5> handlers;
	uint8_t payload[8];
	struct uci_blk packet1 = CREATE_BLOCK(payload);
	packet1.len = 8;
	struct uci_blk packet2 = CREATE_BLOCK(payload);
	packet2.len = 8;
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_NOTIFICATION,
					     UCI_GID_CORE,
					     UCI_OID_CORE_DEVICE_STATUS);

	handlers.set<3>(mt_gid_oid);
	EXPECT_CALL(static_cast<MockHandler<3> &>(handlers),
		    handler(&uci, mt_gid_oid, &packet1));

	EXPECT_CALL(allocator, free(&packet1));

	EXPECT_CALL(static_cast<MockHandler<3> &>(handlers),
		    handler(&uci, mt_gid_oid, &packet2));

	EXPECT_CALL(allocator, free(&packet2));

	uci_message_handlers_register(&uci, &handlers);

	payload[0] = (mt_gid_oid >> 8) & 0xff;
	payload[1] = mt_gid_oid & 0xff;

	uci_packet_recv(&uci, &packet1);
	uci_packet_recv(&uci, &packet2);
}

TEST_F(TestUciCore, TestReceiveUnkownOid)
{
	// Given a uci server with a one handler.
	MockHandlers<1> handlers;
	handlers.set<0>(UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND, 0, 0x0a));
	uci_message_handlers_register(&uci, &handlers);

	// When a command with unkown OID is received.
	uint8_t payload[16];
	struct uci_blk packet = CREATE_BLOCK(payload);
	packet.len = 8;
	packet.total_len = 12;
	EXPECT_CALL(allocator, free(&packet));
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND, 0, 4);
	uci_blk_put_control_header(&packet, mt_gid_oid, packet.total_len);

	// A uci_blk is allocated for the response.
	expect_alloc(&allocator, UCI_STATUS_PACKET_SIZE);

	// And a response with status UNKOWN OID is sent.
	EXPECT_CALL(transport,
		    Out(IsResponseStatus(0, 4, UCI_STATUS_UNKNOWN_OID)))
		.WillOnce(Return(0));

	uci_packet_recv(&uci, &packet);
}

TEST_F(TestUciCore, TestReceiveUnkownGid)
{
	// When a unknown GID is received.
	uint8_t payload[16];
	struct uci_blk packet = CREATE_BLOCK(payload);
	packet.len = 8;
	packet.total_len = 4;
	EXPECT_CALL(allocator, free(&packet));
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND, 0x0f, 0);
	uci_blk_put_control_header(&packet, mt_gid_oid, packet.total_len);

	// A uci_blk is allocated for the response.
	expect_alloc(&allocator, UCI_STATUS_PACKET_SIZE);

	// And a response with status UNKOWN GID is sent.
	EXPECT_CALL(transport,
		    Out(IsResponseStatus(0x0f, 0, UCI_STATUS_UNKNOWN_GID)))
		.WillOnce(Return(0));

	uci_packet_recv(&uci, &packet);
}

TEST_F(TestUciCore, TestReceiveUnhandled)
{
	MockHandlers<2> handlers;
	uint8_t payload[16];
	struct uci_blk packet = CREATE_BLOCK(payload);
	packet.len = 8;
	uint16_t mt_gid_oid = 0x4321;
	handlers.set<0>(mt_gid_oid);
	handlers.set<1>(mt_gid_oid + 1);
	EXPECT_CALL(static_cast<MockHandler<0> &>(handlers),
		    handler(&uci, mt_gid_oid, &packet))
		.WillOnce([&]() { return QERR_SUCCESS; });
	EXPECT_CALL(static_cast<MockHandler<1> &>(handlers),
		    handler(&uci, mt_gid_oid, &packet))
		.Times(0);

	EXPECT_CALL(allocator, free(&packet)).Times(2);

	uci_message_handlers_register(&uci, &handlers);

	payload[0] = (mt_gid_oid >> 8) & 0xff;
	payload[1] = mt_gid_oid & 0xff;
	uci_packet_recv(&uci, &packet);

	mt_gid_oid &= 0x0FFF;
	payload[0] = (mt_gid_oid >> 8) & 0xff;
	payload[1] = mt_gid_oid & 0xff;
	uci_packet_recv(&uci, &packet);
}

TEST_F(TestUciCore, TestReceiveDeviceStateErrorAny)
{
	// Given a device in error state.
	uci.device_state = UCI_DEVICE_STATE_ERROR;

	// When a new message is received.
	uint8_t payload[16];
	struct uci_blk packet = CREATE_BLOCK(payload);
	packet.len = 8;
	packet.total_len = 4;
	EXPECT_CALL(allocator, free(&packet));
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND,
					     UCI_GID_CORE,
					     UCI_OID_CORE_GET_CAPS_INFO);
	uci_blk_put_control_header(&packet, mt_gid_oid, packet.total_len);

	// A uci_blk is allocated for the response.
	expect_alloc(&allocator, UCI_STATUS_PACKET_SIZE);

	// And a response with status REJECTED is sent.
	EXPECT_CALL(transport, Out(IsResponseStatus(UCI_GID_CORE,
						    UCI_OID_CORE_GET_CAPS_INFO,
						    UCI_STATUS_REJECTED)))
		.WillOnce(Return(0));

	uci_packet_recv(&uci, &packet);
}

TEST_F(TestUciCore, TestReceiveDeviceStateErrorResetHandled)
{
	MockHandlers<1> handlers;
	uint8_t payload[16];
	struct uci_blk packet = CREATE_BLOCK(payload);
	packet.len = 8;
	packet.total_len = 4;
	EXPECT_CALL(allocator, free(&packet));

	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND,
					     UCI_GID_CORE,
					     UCI_OID_CORE_DEVICE_RESET);
	uci_blk_put_control_header(&packet, mt_gid_oid, packet.total_len);

	handlers.set<0>(mt_gid_oid);
	EXPECT_CALL(static_cast<MockHandler<0> &>(handlers),
		    handler(&uci, mt_gid_oid, &packet));

	uci_message_handlers_register(&uci, &handlers);

	uci.device_state = UCI_DEVICE_STATE_ERROR;
	uci_packet_recv(&uci, &packet);
}

TEST_F(TestUciCore, TestReceiveDeviceStateErrorResetNoHandler)
{
	// Given a device in error state.
	uci.device_state = UCI_DEVICE_STATE_ERROR;

	// When an unknown command is received.
	uint8_t payload[16];
	struct uci_blk packet = CREATE_BLOCK(payload);
	packet.len = 8;
	packet.total_len = 4;
	EXPECT_CALL(allocator, free(&packet));
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND,
					     UCI_GID_CORE,
					     UCI_OID_CORE_DEVICE_RESET);
	uci_blk_put_control_header(&packet, mt_gid_oid, packet.total_len);

	// A uci_blk is allocated for the response.
	expect_alloc(&allocator, UCI_STATUS_PACKET_SIZE);

	// And a response with status UNKOWN GID is sent.
	EXPECT_CALL(transport, Out(IsResponseStatus(UCI_GID_CORE,
						    UCI_OID_CORE_DEVICE_RESET,
						    UCI_STATUS_UNKNOWN_GID)))
		.WillOnce(Return(0));

	uci_packet_recv(&uci, &packet);
}

TEST_F(TestUciCore, TestReceiveMultipleRegister)
{
	MockHandlers<5> handlers1;
	MockHandlers<2> handlers2;
	uint8_t payload[16];
	struct uci_blk packet = CREATE_BLOCK(payload);
	packet.len = 8;
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND, 1, 2);

	handlers2.set<1>(mt_gid_oid);
	handlers2.set<0>(UCI_MT_GID_OID(0, 1, 1));
	EXPECT_CALL(static_cast<MockHandler<1> &>(handlers2),
		    handler(&uci, mt_gid_oid, &packet));

	EXPECT_CALL(allocator, free(&packet));

	uci_message_handlers_register(&uci, &handlers1);
	uci_message_handlers_register(&uci, &handlers2);

	payload[0] = (mt_gid_oid >> 8) & 0xff;
	payload[1] = mt_gid_oid & 0xff;
	payload[2] = 0;
	payload[3] = 0; // No payload.

	uci_packet_recv(&uci, &packet);
}

TEST_F(TestUciCore, RefuseMixGid)
{
	MockHandlers<2> handlers;
	handlers.set<1>(UCI_MT_GID_OID(0, 0, 0));
	handlers.set<0>(UCI_MT_GID_OID(0, 1, 1));

	uci_message_handlers_register(&uci, &handlers);
	/* register refused this table of handlers */
	ASSERT_EQ(uci.handlers_head, nullptr);
}

TEST_F(TestUciCore, TestReceiveMultipleRegister2)
{
	MockHandlers<5> handlers1;
	MockHandlers<2> handlers2;
	MockHandlers<7> handlers3;
	uint8_t payload[16];
	struct uci_blk packet = CREATE_BLOCK(payload);
	packet.len = 8;
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_NOTIFICATION,
					     UCI_GID_CORE,
					     UCI_OID_CORE_DEVICE_STATUS);

	handlers2.set<1>(mt_gid_oid);
	EXPECT_CALL(static_cast<MockHandler<1> &>(handlers2),
		    handler(&uci, mt_gid_oid, &packet));

	EXPECT_CALL(allocator, free(&packet));

	uci_message_handlers_register(&uci, &handlers1);
	uci_message_handlers_register(&uci, &handlers2);
	uci_message_handlers_register(&uci, &handlers3);

	payload[0] = (mt_gid_oid >> 8) & 0xff;
	payload[1] = mt_gid_oid & 0xff;

	uci_packet_recv(&uci, &packet);
}

/**
 * TODO: The original test that only one handler is called, but all handler are
 * called.
 * We Might need to discuss this.
 *
TEST_F(TestUciCore, TestReceiveMultipleRegisterSameGIDOID)
{
	MockHandlers<5> handlers1;
	MockHandlers<2> handlers2;
	MockHandlers<7> handlers3;
	uint8_t payload[16];
	struct uci_blk packet = CREATE_BLOCK(payload);
	packet.len = 8;
	uint16_t mt_gid_oid = 0x4321;

	handlers2.set<1>(mt_gid_oid);
	EXPECT_CALL(static_cast<MockHandler<1>&>(handlers2),
		    handler(&uci, mt_gid_oid, &packet));

	handlers1.set<3>(mt_gid_oid);
	EXPECT_CALL(static_cast<MockHandler<3>&>(handlers1),
		    handler(&uci, mt_gid_oid, &packet));

	EXPECT_CALL(allocator, free(&packet));

	uci_message_handlers_register(&uci, &handlers1);
	uci_message_handlers_register(&uci, &handlers2);
	uci_message_handlers_register(&uci, &handlers3);

	payload[0] = (mt_gid_oid >> 8) & 0xff;
	payload[1] = mt_gid_oid & 0xff;

	uci_packet_recv(&uci, &packet);
}
*/

TEST_F(TestUciCore, TestUciSendStatusNoTransport)
{
	uint16_t gid_oid =
		UCI_GID_OID(UCI_GID_SESSION_CONFIG, UCI_OID_SESSION_DEINIT);

	// Given no transport attached, uci_send_status fails
	ASSERT_EQ(uci_transport_detach(&uci), 0);
	EXPECT_NO_FATAL_FAILURE(
		uci_send_status(&uci, gid_oid, UCI_STATUS_FAILED));
}

TEST_F(TestUciCore, TestUciSendStatus)
{
	uint16_t gid_oid =
		UCI_GID_OID(UCI_GID_SESSION_CONFIG, UCI_OID_SESSION_DEINIT);

	expect_alloc(&allocator, UCI_STATUS_PACKET_SIZE);
	EXPECT_CALL(transport, Out(IsResponseStatus(UCI_GID_SESSION_CONFIG,
						    UCI_OID_SESSION_DEINIT,
						    UCI_STATUS_FAILED)))
		.WillOnce(Return(0));

	uci_send_status(&uci, gid_oid, UCI_STATUS_FAILED);
}

TEST_F(TestUciCore, TestUciSendStatusReplaceMt)
{
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND,
					     UCI_GID_CORE,
					     UCI_OID_CORE_DEVICE_RESET);
	expect_alloc(&allocator, UCI_STATUS_PACKET_SIZE);
	EXPECT_CALL(transport, Out(IsResponseStatus(UCI_GID_CORE,
						    UCI_OID_CORE_DEVICE_RESET,
						    UCI_STATUS_FAILED)))
		.WillOnce(Return(0));
	uci_send_status(&uci, mt_gid_oid, UCI_STATUS_FAILED);
}

class TestUciSendMessage : public TestUciCore {};

TEST_F(TestUciSendMessage, FullPacketWithCorrectSize)
{
	uint8_t payload[5];
	struct uci_blk block = CREATE_BLOCK(payload);

	EXPECT_CALL(allocator, alloc(5, 0)).WillOnce(Return(&block));
	EXPECT_CALL(allocator, free(&block));

	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) {
		EXPECT_EQ(blk->len, 5);
		EXPECT_EQ(blk->size, 5);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		int16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_NOTIFICATION);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_CORE_DEVICE_STATUS);
		EXPECT_EQ(blk->data[3], 1);
		EXPECT_EQ(blk->data[4], UCI_DEVICE_STATE_ACTIVE);
		return 0;
	});

	struct uci_message_builder builder =
		UCI_MESSAGE_BUILDER_INITIALIZER_WITH_SIZE(&uci, 5);
	ASSERT_EQ(uci_message_put_8bit(&builder, UCI_DEVICE_STATE_ACTIVE), 0);
	uci_send_message(&uci,
			 UCI_MT_GID_OID(UCI_MESSAGE_TYPE_NOTIFICATION,
					UCI_GID_CORE,
					UCI_OID_CORE_DEVICE_STATUS),
			 builder.message);

	// uci_packet_send_done has been called and now tx is empty
	ASSERT_FALSE(uci.tx);
}

TEST_F(TestUciSendMessage, FullPacketWithBiggerSize)
{
	uint8_t payload[UCI_MAX_CONTROL_PACKET_SIZE];
	struct uci_blk block = CREATE_BLOCK(payload);

	EXPECT_CALL(allocator, alloc(5, 0)).WillOnce(Return(&block));
	EXPECT_CALL(allocator, free(&block));

	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) {
		EXPECT_EQ(blk->len, 5);
		EXPECT_EQ(blk->size, UCI_MAX_CONTROL_PACKET_SIZE);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		int16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_NOTIFICATION);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_CORE_DEVICE_STATUS);
		EXPECT_EQ(blk->data[3], 1);
		EXPECT_EQ(blk->data[4], UCI_DEVICE_STATE_ACTIVE);
		return 0;
	});

	struct uci_message_builder builder =
		UCI_MESSAGE_BUILDER_INITIALIZER_WITH_SIZE(&uci, 5);
	ASSERT_EQ(uci_message_put_8bit(&builder, UCI_DEVICE_STATE_ACTIVE), 0);
	uci_send_message(&uci,
			 UCI_MT_GID_OID(UCI_MESSAGE_TYPE_NOTIFICATION,
					UCI_GID_CORE,
					UCI_OID_CORE_DEVICE_STATUS),
			 builder.message);

	// uci_packet_send_done has been called and now tx is empty
	ASSERT_FALSE(uci.tx);
}

TEST_F(TestUciSendMessage, FullPacketOnTwoBlocks)
{
	uint8_t payload1[5];
	struct uci_blk block1 = CREATE_BLOCK(payload1);
	uint8_t payload2[5];
	struct uci_blk block2 = CREATE_BLOCK(payload2);

	EXPECT_CALL(allocator, alloc(7, 0))
		.WillOnce(Return(&block1))
		.WillOnce(Return(&block2));
	EXPECT_CALL(allocator, free(&block1));
	EXPECT_CALL(allocator, free(&block2));

	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) {
		struct uci_blk *packet_head = blk;
		EXPECT_EQ(packet_head->size, 5);
		EXPECT_EQ(packet_head->len, 5);
		EXPECT_EQ(packet_head->total_len, 2);
		EXPECT_TRUE(uci_blk_has_header(packet_head));
		EXPECT_FALSE(uci_blk_is_segment(packet_head));
		int16_t mt_gid_oid = uci_blk_get_mt_gid_oid(packet_head);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_NOTIFICATION);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_CORE_DEVICE_STATUS);
		EXPECT_EQ(packet_head->data[3], 2);
		// First message get first part of the payload
		EXPECT_EQ(packet_head->data[4], 1);

		EXPECT_TRUE(blk->next);
		struct uci_blk *packet_tail = blk->next;
		EXPECT_FALSE(uci_blk_has_header(packet_tail));
		EXPECT_EQ(packet_tail->size, 5);
		EXPECT_EQ(packet_tail->len, 1);
		// Next message get second part of the payload
		EXPECT_EQ(packet_tail->data[0], 2);

		return 0;
	});

	struct uci_message_builder builder =
		UCI_MESSAGE_BUILDER_INITIALIZER_WITH_SIZE(&uci, 7);
	ASSERT_EQ(uci_message_put_8bit(&builder, 1), 0);
	ASSERT_EQ(uci_message_put_8bit(&builder, 2), 0);
	uci_send_message(&uci,
			 UCI_MT_GID_OID(UCI_MESSAGE_TYPE_NOTIFICATION,
					UCI_GID_CORE,
					UCI_OID_CORE_DEVICE_STATUS),
			 builder.message);

	// uci_packet_send_done has been called and now tx is empty
	ASSERT_FALSE(uci.tx);
}

TEST_F(TestUciSendMessage, CmdWithoutPayload)
{
	EXPECT_CALL(transport, Out).WillOnce([](struct uci_blk *blk) {
		EXPECT_EQ(blk->len, 4);
		EXPECT_EQ(blk->total_len, 0);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		int16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
		EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
		EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
		EXPECT_EQ(UCI_OID(mt_gid_oid), UCI_OID_CORE_DEVICE_RESET);
		EXPECT_EQ(blk->data[3], 0);
		EXPECT_FALSE(blk->next);
		return 0;
	});

	expect_alloc(&allocator, UCI_STATUS_PACKET_SIZE);
	uci_send_message(&uci,
			 UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND, UCI_GID_CORE,
					UCI_OID_CORE_DEVICE_RESET),
			 NULL);

	// uci_packet_send_done has been called and now tx is empty
	ASSERT_FALSE(uci.tx);
}

TEST_F(TestUciSendMessage, SegmentedMessages)
{
	uint8_t payload1[UCI_MAX_CONTROL_PACKET_SIZE];
	struct uci_blk block1 = CREATE_BLOCK(payload1);
	uint8_t payload2[10];
	struct uci_blk block2 = CREATE_BLOCK(payload2);

	EXPECT_CALL(allocator, alloc)
		.WillOnce(Return(&block1))
		.WillOnce(Return(&block2));
	EXPECT_CALL(allocator, free(&block1));
	EXPECT_CALL(allocator, free(&block2));

	// Expect to receive a message over two packets
	EXPECT_CALL(transport, Out)
		.WillOnce([](struct uci_blk *blk) {
			EXPECT_EQ(blk->len, UCI_MAX_CONTROL_PACKET_SIZE);
			EXPECT_EQ(blk->size, UCI_MAX_CONTROL_PACKET_SIZE);
			EXPECT_EQ(blk->total_len, 257);
			EXPECT_TRUE(uci_blk_has_header(blk));
			EXPECT_TRUE(uci_blk_is_segment(blk));
			int16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
			EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
			EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
			EXPECT_EQ(UCI_OID(mt_gid_oid),
				  UCI_OID_CORE_DEVICE_RESET);
			EXPECT_FALSE(blk->next);
			return 0;
		})
		.WillOnce([](struct uci_blk *blk) {
			EXPECT_EQ(blk->len, 6);
			EXPECT_EQ(blk->size, 10);
			EXPECT_EQ(blk->total_len, 0);
			EXPECT_TRUE(uci_blk_has_header(blk));
			EXPECT_FALSE(uci_blk_is_segment(blk));
			int16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
			EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
			EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
			EXPECT_EQ(UCI_OID(mt_gid_oid),
				  UCI_OID_CORE_DEVICE_RESET);
			EXPECT_FALSE(blk->next);
			return 0;
		});

	struct uci_message_builder builder =
		UCI_MESSAGE_BUILDER_INITIALIZER(&uci);
	for (size_t i = 0; i < UCI_MAX_CONTROL_PAYLOAD_SIZE + 2; i++) {
		ASSERT_EQ(uci_message_put_8bit(&builder, i), 0);
	}

	uci_send_message(&uci,
			 UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND, UCI_GID_CORE,
					UCI_OID_CORE_DEVICE_RESET),
			 builder.message);

	// uci_packet_send_done has been called and now tx is empty
	ASSERT_FALSE(uci.tx);
}

TEST_F(TestUciSendMessage, SegmentedMessagesOverTwoBlocks)
{
	uint8_t payload1[253];
	struct uci_blk block1 = CREATE_BLOCK(payload1);
	uint8_t payload2[252];
	struct uci_blk block2 = CREATE_BLOCK(payload2);
	uint8_t payload3[251];
	struct uci_blk block3 = CREATE_BLOCK(payload3);

	EXPECT_CALL(allocator, alloc)
		.WillOnce(Return(&block1))
		.WillOnce(Return(&block2))
		.WillOnce(Return(&block3));
	EXPECT_CALL(allocator, free(&block1));
	EXPECT_CALL(allocator, free(&block2));
	EXPECT_CALL(allocator, free(&block3));

	// Expect to receive a message over two packets
	EXPECT_CALL(transport, Out)
		.WillOnce([](struct uci_blk *blk) {
			// First packet in two blocks
			EXPECT_EQ(blk->len, 253);
			EXPECT_EQ(blk->size, 253);
			EXPECT_EQ(blk->total_len, 272);
			EXPECT_TRUE(uci_blk_has_header(blk));
			EXPECT_TRUE(uci_blk_is_segment(blk));
			int16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
			EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
			EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
			EXPECT_EQ(UCI_OID(mt_gid_oid),
				  UCI_OID_CORE_DEVICE_RESET);
			EXPECT_TRUE(blk->next);
			EXPECT_EQ(blk->next->size, 252);
			EXPECT_EQ(blk->next->len,
				  UCI_MAX_CONTROL_PACKET_SIZE - blk->len);
			EXPECT_FALSE(blk->next->flags &
				     UCI_BLK_FLAGS_HEADER_RESERVED);
			return 0;
		})
		.WillOnce([](struct uci_blk *blk) {
			EXPECT_EQ(blk->len, 21);
			EXPECT_EQ(blk->size, 251);
			EXPECT_EQ(blk->total_len, 0);
			EXPECT_TRUE(uci_blk_has_header(blk));
			EXPECT_FALSE(uci_blk_is_segment(blk));
			int16_t mt_gid_oid = uci_blk_get_mt_gid_oid(blk);
			EXPECT_EQ(UCI_MT(mt_gid_oid), UCI_MESSAGE_TYPE_COMMAND);
			EXPECT_EQ(UCI_GID(mt_gid_oid), UCI_GID_CORE);
			EXPECT_EQ(UCI_OID(mt_gid_oid),
				  UCI_OID_CORE_DEVICE_RESET);
			EXPECT_FALSE(blk->next);
			return 0;
		});

	struct uci_message_builder builder =
		UCI_MESSAGE_BUILDER_INITIALIZER(&uci);
	for (size_t i = 0; i < 34; i++) {
		ASSERT_EQ(uci_message_put_64bit(&builder, i), 0);
	}

	uci_send_message(&uci,
			 UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND, UCI_GID_CORE,
					UCI_OID_CORE_DEVICE_RESET),
			 builder.message);

	// uci_packet_send_done has been called and now tx is empty
	ASSERT_FALSE(uci.tx);
}

TEST_F(TestUciMessagePut, UciMessagePutNocopyByMonkey)
{
	ASSERT_NE(0, uci_message_put_nocopy(nullptr, 0, nullptr));

	struct uci_message_builder builder =
		UCI_MESSAGE_BUILDER_INITIALIZER(&uci);
	ASSERT_NE(0, uci_message_put_nocopy(&builder, 0, nullptr));

	builder.first_errno = 0;
	ASSERT_NE(0, uci_message_put_nocopy(&builder, 10, nullptr));

	builder.first_errno = 0;
	uint8_t *data;
	ASSERT_NE(0, uci_message_put_nocopy(&builder, 0, &data));

	builder.first_errno = UCI_STATUS_FAILED;
	ASSERT_EQ(-builder.first_errno,
		  uci_message_put_nocopy(&builder, 10, &data));
}

TEST_F(TestUciCore, UciMessageGetNocopyByMonkey)
{
	ASSERT_EQ(0, uci_message_get_nocopy(nullptr, 0, nullptr));

	struct uci_message_parser parser;
	std::memset(&parser, 0, sizeof(parser));
	ASSERT_EQ(0, uci_message_get_nocopy(&parser, 0, nullptr));

	ASSERT_EQ(0, uci_message_get_nocopy(&parser, 10, nullptr));

	uint8_t *data;
	ASSERT_EQ(0, uci_message_get_nocopy(&parser, 0, &data));

	ASSERT_EQ(0, uci_message_get_nocopy(&parser, 10, &data));

	struct uci_blk blk;
	std::memset(&blk, 0, sizeof(blk));
	parser.current_block = &blk;
	ASSERT_EQ(0, uci_message_get_nocopy(&parser, 10, &data));
}

TEST_F(TestUciCore, uci_message_put_blk_bad_usage)
{
	uci_message_put_blk(nullptr, nullptr);

	struct uci_message_builder builder =
		UCI_MESSAGE_BUILDER_INITIALIZER(&uci);
	uci_message_put_blk(&builder, nullptr);
	ASSERT_FALSE(builder.message);
}

TEST_F(TestUciMessagePut, UciMessagePutBlk)
{
	uint8_t payload1[10];
	struct uci_blk block1 = CREATE_BLOCK(payload1);
	uint8_t payload2[3];
	struct uci_blk block2 = CREATE_BLOCK(payload2);
	uint8_t payload3[4];
	struct uci_blk block3 = CREATE_BLOCK(payload3);

	EXPECT_CALL(allocator, alloc)
		.WillOnce(Return(&block1))
		.WillOnce(Return(&block3));
	EXPECT_CALL(allocator, free(&block1));
	EXPECT_CALL(allocator, free(&block2));
	EXPECT_CALL(allocator, free(&block3));

	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND, 0, 0);

	/* Expect to receive a message over a single packet but in three blocks
	 */
	EXPECT_CALL(transport, Out).WillOnce([mt_gid_oid](struct uci_blk *blk) {
		EXPECT_EQ(blk->len, 6);
		EXPECT_EQ(blk->size, 10);
		EXPECT_EQ(blk->total_len, 9);
		EXPECT_TRUE(uci_blk_has_header(blk));
		EXPECT_FALSE(uci_blk_is_segment(blk));
		EXPECT_EQ(uci_blk_get_mt_gid_oid(blk), mt_gid_oid);
		EXPECT_TRUE(blk->next);

		auto blk2 = blk->next;
		EXPECT_EQ(blk2->len, 3);
		EXPECT_EQ(blk2->size, 3);
		EXPECT_EQ(blk2->total_len, 0);
		EXPECT_FALSE(uci_blk_has_header(blk2));
		EXPECT_TRUE(blk2->next);

		auto blk3 = blk2->next;
		EXPECT_EQ(blk3->len, 4);
		EXPECT_EQ(blk3->size, 4);
		EXPECT_EQ(blk3->total_len, 0);
		EXPECT_FALSE(uci_blk_has_header(blk3));
		EXPECT_FALSE(blk3->next);

		return 0;
	});

	struct uci_message_builder builder =
		UCI_MESSAGE_BUILDER_INITIALIZER(&uci);

	/* header and initial data */
	for (int i = 0; i < 2; ++i) {
		uci_message_put_8bit(&builder, i + 1);
	}

	/* block2 is constructed by external source */
	block2.next = nullptr;
	for (size_t i = 0; i < sizeof(payload2); ++i) {
		block2.data[i] = i + 42;
	}
	block2.len = sizeof(payload2);
	block2.total_len = 0;
	block2.size = block2.len;

	uci_message_put_blk(&builder, &block2);

	/* footer after external block */
	for (int i = 0; i < 4; ++i) {
		uci_message_put_8bit(&builder, i + 69);
	}

	ASSERT_FALSE(uci_message_failed(&builder));

	uci_send_message(&uci, mt_gid_oid, builder.message);

	/* uci_packet_send_done has been called and now tx is empty */
	ASSERT_FALSE(uci.tx);
}

TEST_F(TestUciMessagePut, UciMessagePutBlkFirstExternalBlk)
{
	uint8_t payload[10];
	struct uci_blk block = CREATE_BLOCK(payload);

	block.len = block.size;

	struct uci_message_builder builder =
		UCI_MESSAGE_BUILDER_INITIALIZER(&uci);
	uci_message_put_blk(&builder, &block);

	ASSERT_EQ(builder.message, &block);
}

TEST(UciCore, ClientIgnoreUciSetDeviceState)
{
	MockUciAllocator allocator;
	MockUciTransport transport;
	struct uci uci;

	/* Given this is a client, device_state should not change */
	ASSERT_EQ(uci_init(&uci, &allocator, true), 0);
	ASSERT_EQ(uci_transport_attach(&uci, &transport), 0);
	uci.device_state = UCI_DEVICE_STATE_READY;
	uci_set_device_state_notification(&uci, UCI_DEVICE_STATE_ACTIVE);
	ASSERT_EQ(uci.device_state, UCI_DEVICE_STATE_READY);
	uci_uninit(&uci);
}

TEST(UciCore, ClientIgnoreUciResetDeviceState)
{
	MockUciAllocator allocator;
	MockUciTransport transport;
	struct uci uci;

	/* Given this is a client, device_state should not change */
	ASSERT_EQ(uci_init(&uci, &allocator, true), 0);
	ASSERT_EQ(uci_transport_attach(&uci, &transport), 0);
	uci.device_state = UCI_DEVICE_STATE_ERROR;
	uci_reset_device_state(&uci);
	ASSERT_EQ(uci.device_state, UCI_DEVICE_STATE_ERROR);
	uci_uninit(&uci);
}

TEST_F(TestUciCore, UciCoreDeviceBootNtfSuccess)
{
	enum uci_qorvo_boot_reason boot_reason =
		UCI_QORVO_BOOT_REASON_FATAL_ERROR_RESET;
	expect_alloc(&allocator, 5);
	EXPECT_CALL(transport, Out).WillOnce([&](uci_blk *rsp) {
		EXPECT_TRUE(rsp);
		EXPECT_TRUE(uci_blk_has_header(rsp));
		EXPECT_EQ(uci_blk_get_mt_gid_oid(rsp),
			  UCI_MT_GID_OID(UCI_MESSAGE_TYPE_NOTIFICATION,
					 UCI_GID_QORVO_EXT2,
					 UCI_OID_QORVO_CORE_DEVICE_BOOT));
		EXPECT_FALSE(uci_blk_is_segment(rsp));
		struct uci_message_parser parser =
			UCI_MESSAGE_PARSER_INITIALIZER(rsp);
		EXPECT_EQ(uci_message_remaining(&parser), 1);
		EXPECT_EQ(uci_message_get_8bit(&parser), boot_reason);
		return 0;
	});
	uci_send_device_boot_notification(&uci, boot_reason);
}

TEST(UciCore, ClientIgnoreUciCoreDeviceBootNtf)
{
	MockUciAllocator allocator;
	MockUciTransport transport;
	struct uci uci;

	/* Given this is a client, no message should be sent. */
	ASSERT_EQ(uci_init(&uci, &allocator, true), 0);
	ASSERT_EQ(uci_transport_attach(&uci, &transport), 0);
	uci_send_device_boot_notification(
		&uci, UCI_QORVO_BOOT_REASON_FATAL_ERROR_RESET);
	uci_uninit(&uci);
}
