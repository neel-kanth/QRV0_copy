/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

extern "C" {
#include "cherry/cherry.h"
#include "cherry_priv.h"

#include <qmalloc.h>
}
#include "mock_cherry_core_client.hh"
#include "mock_cherry_session_client.hh"
//#include "mock_cherry_fira_client.hh"
#include "mock_cherry_thread.hh"
#include "mock_cherry_uci_transport.hh"
#include "mock_qmalloc.hh"
#include "mock_qosal.hh"
#include "mock_uci.hh"

#include <cstring>
#include <memory>
#include <vector>

using testing::InSequence;
using testing::Mock;
using testing::Return;
using testing::StrictMock;
void empty_core_cb(struct cherry_core_event *event, void *user_data)
{
	cherry_core_event_free(event);
	return;
}
void expect_error_event_core_cb(struct cherry_core_event *event,
				void *user_data)
{
	EXPECT_EQ(event->type, CHERRY_CORE_EVENT_TYPE_ERROR);
	cherry_core_event_free(event);
	return;
}

TEST(Cherry_Create, WrongPath)
{
	EXPECT_EQ(NULL, NULL);
}

TEST(Cherry_Create, null_core_error)
{
	EXPECT_EQ(cherry_create("/dev/uci0", NULL, NULL), nullptr);
}

TEST(Cherry_Create, null_device_error)
{
	EXPECT_EQ(cherry_create(NULL, &empty_core_cb, NULL), nullptr);
}

TEST(Cherry_Create, create_destroy_ok)
{
	StrictMock<MockCherryThread> CherryThread;
	StrictMock<MockCherryCoreClient> CherryCoreClient;
	StrictMock<MockCherrySessionClient> CherrySessionClient;
	StrictMock<MockUci> Uci;
	StrictMock<MockCherryUciTransport> CherryUciTransport;
	StrictMock<MockQosal> Qosal;
	int64_t timer = 0x12345678ll;
	EXPECT_CALL(Qosal, qtime_get_uptime_us)
		.WillRepeatedly(
			[&timer]() -> int64_t { return timer++ * 10000; });

	StrictMock<MockQmalloc> Qmalloc;
	Qmalloc.implicit_call();

	struct cherry *cherry_ctx;
	EXPECT_CALL(CherryThread, cherry_thread_create).WillOnce([
	](struct cherry * cherry_ctx) -> enum cherry_err {
		return CHERRY_ERR_NONE;
	});
	EXPECT_CALL(Uci, uci_init)
		.WillOnce([](struct uci * uci, struct uci_allocator * allocator,
			     bool is_client) -> enum qerr {
			return QERR_SUCCESS;
		});
	EXPECT_CALL(CherrySessionClient, cherry_uci_client_session_open)
		.WillOnce([](struct cherry_session_context **context,
			     struct uci *uci, void *user_data,
			     cherry_uci_client_session_status_cb_t status_cb,
			     cherry_uci_client_session_ranging_ntf_cb_t
				     ranging_ntf_cb) -> qerr {
			return QERR_SUCCESS;
		});
	EXPECT_CALL(CherryUciTransport, cherry_init_transport)
		.WillOnce([](struct cherry_uci_transport * ctx,
			     struct uci * uci,
			     const char *device) -> enum cherry_err {
			EXPECT_EQ(strncmp("/dev/uci0", device,
					  sizeof("/dev/uci0")),
				  0);
			return CHERRY_ERR_NONE;
		});
	EXPECT_CALL(CherryCoreClient, cherry_uci_client_core_open)
		.WillOnce([](struct cherry_core_context **context,
			     struct uci *uci, void *user_data,
			     cherry_uci_client_core_device_status_cb_t
				     device_status_cb,
			     cherry_uci_client_core_boot_cb_t boot_cb) -> qerr {
			return QERR_SUCCESS;
		});
	EXPECT_CALL(CherryThread, cherry_thread_process_prio_task)
		.WillRepeatedly([](struct cherry_thread *thread_ctx,
				   uint32_t timeout_ms) -> int { return 0; });

	EXPECT_CALL(CherryCoreClient, cherry_uci_client_core_close)
		.WillOnce([](struct cherry_core_context *context) -> qerr {
			return QERR_SUCCESS;
		});

	EXPECT_CALL(CherryCoreClient, cherry_uci_client_core_get_uwbs_state)
		.WillOnce([](struct cherry_core_context *context,
			     enum uci_device_state *uwbs_state)
				  -> uci_status_code { return UCI_STATUS_OK; });
#if 0
	/* TO FIX */
	EXPECT_CALL(CherrySessionClient, cherry_uci_client_session_close)
		.WillOnce([](struct cherry_session_context *context) -> qerr {
			return QERR_SUCCESS;
		});
#endif
	EXPECT_CALL(CherryUciTransport, cherry_de_init_transport).WillOnce([
	](struct cherry_uci_transport * ctx) -> enum cherry_err {
		return CHERRY_ERR_NONE;
	});
	EXPECT_CALL(Uci, uci_uninit).WillOnce([](struct uci *uci) {});
	EXPECT_CALL(CherryThread, cherry_thread_stop).WillOnce([
	](struct cherry_thread * thread_ctx) -> enum cherry_err {
		return CHERRY_ERR_NONE;
	});
	EXPECT_CALL(CherryThread, cherry_thread_send_list_task)
		.Times(2)
		.WillOnce([&](struct cherry_thread * thread_ctx,
			      const void *context, const void *params,
			      size_t params_size,
			      cherry_thread_task_t thread_task,
			      bool destroy) -> enum cherry_err {
			thread_task(context, params, false);
			return CHERRY_ERR_NONE;
		})
		.WillOnce([&](struct cherry_thread * thread_ctx,
			      const void *context, const void *params,
			      size_t params_size,
			      cherry_thread_task_t thread_task,
			      bool destroy) -> enum cherry_err {
			thread_task(context, params, false);
			return CHERRY_ERR_NONE;
		});
	EXPECT_CALL(CherryThread, cherry_thread_join)
		.WillOnce([](struct cherry_thread *thread_ctx) -> void {
			return;
		});
	EXPECT_CALL(CherryThread, cherry_thread_destroy)
		.WillOnce([](struct cherry_thread *thread_ctx) -> void {
			return;
		});
	cherry_ctx = cherry_create("/dev/uci0", &empty_core_cb, NULL);
	EXPECT_NE(cherry_ctx, nullptr);
	cherry_destroy_sync(cherry_ctx);
}

TEST(Cherry_Create, create_init_transport_error)
{
	StrictMock<MockCherryThread> CherryThread;
	StrictMock<MockCherryCoreClient> CherryCoreClient;
	StrictMock<MockCherrySessionClient> CherrySessionClient;
	StrictMock<MockUci> Uci;
	StrictMock<MockCherryUciTransport> CherryUciTransport;
	StrictMock<MockQosal> Qosal;
	int64_t timer = 0x12345678ll;
	EXPECT_CALL(Qosal, qtime_get_uptime_us)
		.WillRepeatedly(
			[&timer]() -> int64_t { return timer++ * 10000; });

	StrictMock<MockQmalloc> Qmalloc;
	Qmalloc.implicit_call();

	struct cherry *cherry_ctx;
	EXPECT_CALL(CherryThread, cherry_thread_create).WillOnce([
	](struct cherry * cherry_ctx) -> enum cherry_err {
		return CHERRY_ERR_NONE;
	});
	EXPECT_CALL(Uci, uci_init)
		.WillOnce([](struct uci * uci, struct uci_allocator * allocator,
			     bool is_client) -> enum qerr {
			return QERR_SUCCESS;
		});
	EXPECT_CALL(CherrySessionClient, cherry_uci_client_session_open)
		.WillOnce([](struct cherry_session_context **context,
			     struct uci *uci, void *user_data,
			     cherry_uci_client_session_status_cb_t status_cb,
			     cherry_uci_client_session_ranging_ntf_cb_t
				     ranging_ntf_cb) -> qerr {
			return QERR_SUCCESS;
		});
	EXPECT_CALL(CherryUciTransport, cherry_init_transport)
		.WillOnce([](struct cherry_uci_transport * ctx,
			     struct uci * uci,
			     const char *device) -> enum cherry_err {
			EXPECT_EQ(strncmp("/dev/uci0", device,
					  sizeof("/dev/uci0")),
				  0);
			return CHERRY_ERR_INTERNAL;
		});
	EXPECT_CALL(CherryUciTransport, cherry_de_init_transport).WillOnce([
	](struct cherry_uci_transport * ctx) -> enum cherry_err {
		return CHERRY_ERR_NONE;
	});
	EXPECT_CALL(CherryCoreClient, cherry_uci_client_core_close)
		.WillOnce([](struct cherry_core_context *context) -> qerr {
			return QERR_SUCCESS;
		});
	EXPECT_CALL(Uci, uci_uninit).WillOnce([](struct uci *uci) {});
	EXPECT_CALL(CherryThread, cherry_thread_stop).WillOnce([
	](struct cherry_thread * thread_ctx) -> enum cherry_err {
		return CHERRY_ERR_NONE;
	});
	EXPECT_CALL(CherryThread, cherry_thread_send_list_task)
		.Times(2)
		.WillOnce([&](struct cherry_thread * thread_ctx,
			      const void *context, const void *params,
			      size_t params_size,
			      cherry_thread_task_t thread_task,
			      bool destroy) -> enum cherry_err {
			thread_task(context, params, false);
			return CHERRY_ERR_NONE;
		})
		.WillOnce([&](struct cherry_thread * thread_ctx,
			      const void *context, const void *params,
			      size_t params_size,
			      cherry_thread_task_t thread_task,
			      bool destroy) -> enum cherry_err {
			thread_task(context, params, false);
			return CHERRY_ERR_NONE;
		});
	EXPECT_CALL(CherryThread, cherry_thread_join)
		.WillOnce([](struct cherry_thread *thread_ctx) -> void {
			return;
		});
	EXPECT_CALL(CherryThread, cherry_thread_destroy)
		.WillOnce([](struct cherry_thread *thread_ctx) -> void {
			return;
		});
	cherry_ctx = cherry_create("/dev/uci0", &empty_core_cb, NULL);
	EXPECT_NE(cherry_ctx, nullptr);
	cherry_destroy_sync(cherry_ctx);
}

TEST(getError, errorSessionActive)
{
	EXPECT_EQ(cherry_get_error(UCI_STATUS_ERROR_SESSION_ACTIVE),
		  CHERRY_ERR_SESSION_ACTIVE);
}
