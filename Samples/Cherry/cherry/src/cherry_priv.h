/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#ifndef CHERRY_PRIV_H
#define CHERRY_PRIV_H

#include "cherry_core_client.h"
#include "cherry_thread.h"
#include "cherry_uci_transport.h"

#include <cherry/cherry.h>
#include <qtils.h>

#define CHERRY_NOTIFICATION_TIMEOUT_US 1500000
#define CHERRY_BOOT_NOTIFICATION_TIMEOUT_US 500000
#define CHERRY_DEVICE_STATE_NOTIFICATION_TIMEOUT_US 5000000

#define _CHERRY_CAT(a, b) a##b
#define CHERRY_CAT(a, b) _CHERRY_CAT(a, b)

#define CHERRY_SESSION_PUT_PARAM(SetParamFct, Cond, ...) \
	if (err == CHERRY_ERR_NONE && Cond)              \
	err = cherry_get_error(SetParamFct)

#define CHERRY_SESSION_PUT_ITEM(...) CHERRY_SESSION_PUT_PARAM(__VA_ARGS__, true)

#define CHERRY_SESSION_PUT_LOOP1(...)             \
	CHERRY_SESSION_PUT_ITEM(__VA_ARGS__, ""); \
	CHERRY_SESSION_PUT_LOOP2
#define CHERRY_SESSION_PUT_LOOP2(...)             \
	CHERRY_SESSION_PUT_ITEM(__VA_ARGS__, ""); \
	CHERRY_SESSION_PUT_LOOP1

#define CHERRY_SESSION_PUT_LOOP1_END
#define CHERRY_SESSION_PUT_LOOP2_END

#define CHERRY_SESSION_SET_PARAMS(SessionBase, ...)                       \
	do {                                                              \
		enum cherry_err err;                                      \
		struct cherry_uci_client_session_set_app_config_cmd *cmd; \
		err = cherry_session_set_app_config_begin(SessionBase);   \
		cmd = (SessionBase)->set_app_config_cmd;                  \
		CHERRY_CAT(CHERRY_SESSION_PUT_LOOP1 __VA_ARGS__, _END)    \
		cherry_session_set_app_config_end(SessionBase);           \
		return err;                                               \
	} while (42 == 24)

#define CHERRY_SESSION_SET_PARAM(SessionBase, SetParamFct) \
	CHERRY_SESSION_SET_PARAMS(SessionBase, (SetParamFct))

enum cherry_core_internal_state {
	CHERRY_CORE_INTERNAL_STATE_READY = 0x1,
	CHERRY_CORE_INTERNAL_STATE_ACTIVE,
	CHERRY_CORE_INTERNAL_STATE_ERROR,
	CHERRY_CORE_INTERNAL_STATE_REBOOTING
};

enum cherry_session_type {
	CHERRY_SESSION_TYPE_FIRA,
	CHERRY_SESSION_TYPE_RADAR,
	CHERRY_SESSION_TYPE_CCC,
};

/**
 * struct cherry - Cherry structure.
 * @thread_ctx: The Cherry thread context.
 * @uci: The UCI context.
 * @uci_transport_ctx: The UCI transport context.
 * @core_client_ctx: Pointer to the core client context.
 * @session_client_ctx: Pointer to the session client context.
 * @fira_clientctx: Pointer to the fira client context.
 * @radar_client_ctx: Pointer to the radar client context.
 * @ccc_client_ctx: Pointer to the ccc client context.
 * @core_cb: The core callback used to send an event to the applicative.
 * @user_data: Pointer to user data provided by applicative and to give to the core callback.
 * @boot_reason: Boot reason.
 * @internal_state: Cherry internal state.
 * @calib: Pointer to the calibration structure.
 * @sessions: Linked list to sessions attached to this core.
 * @nb_sessions: Number of sessions in the list.
 * @sessions_mutex: Mutex to protect the sessions list.
 * @sessions_ntf: Semaphore to wait for session ntf.
 */
struct cherry {
	struct cherry_thread thread_ctx;
	struct uci uci;
	struct cherry_uci_transport uci_transport_ctx;
	struct cherry_core_context *core_client_ctx;
	struct cherry_session_context *session_client_ctx;
	struct cherry_fira_context *fira_client_ctx;
	struct cherry_radar_context *radar_client_ctx;
	struct cherry_ccc_context *ccc_client_ctx;
	cherry_core_cb_t core_cb;
	void *user_data;
	enum cherry_core_state_change_reason boot_reason;
	enum cherry_core_internal_state internal_state;
	const struct cherry_calib *calib;
	struct cherry_session *sessions;
	int nb_sessions;
	struct qmutex *sessions_mutex;
	struct qsemaphore *sessions_ntf;
};

/* Translate uci status code to cherry error. */
enum cherry_err cherry_get_error(enum uci_status_code code);

void cherry_fira_close(struct cherry *cherry_ctx);
void cherry_radar_close(struct cherry *cherry_ctx);

void cherry_session_close(struct cherry *cherry_ctx);
int cherry_session_uci_client_setup(struct cherry *cherry_ctx);

enum cherry_err
cherry_send_calib(const struct cherry_calib *calib,
		  struct cherry_uci_transport *uci_transport_ctx);

bool cherry_state_is_valid(struct cherry *cherry_ctx);

void cherry_set_error_state(struct cherry *ctx);

struct cherry_session *cherry_get_session_context(struct cherry *ctx,
						  uint32_t session_handle);

void cherry_session_force_stop(struct cherry_session *session);

void cherry_force_stop_session(struct cherry_session *session_base);

struct cherry_session_child_cb;
struct cherry_ctx;

void cherry_session_init(struct cherry_session *session_base,
			 enum cherry_session_type type,
			 struct cherry *cherry_ctx, void *user_data,
			 const struct cherry_session_child_cb *child_cb);

enum cherry_err cherry_session_thread_init(struct cherry_session *session_base,
					   const uint32_t session_id,
					   enum uci_session_type session_type);

enum cherry_err
cherry_session_set_app_config_begin(struct cherry_session *session_base);

void cherry_session_set_app_config_end(struct cherry_session *session_base);

enum cherry_err
cherry_session_send_current_app_config(struct cherry_session *session_base,
				       bool create_new_command);

/* Handler for ranging notifications. */
struct session_ranging_data;
void cherry_fira_twr_ranging_handler(struct cherry_session *session_base,
				     const struct session_ranging_data *data);
void cherry_fira_dl_tdoa_ranging_handler(
	struct cherry_session *session_base,
	const struct session_ranging_data *data);
void cherry_ccc_controller_ranging_handler(
	struct cherry_session *session_base,
	const struct session_ranging_data *data);
void cherry_ccc_controlee_ranging_handler(
	struct cherry_session *session_base,
	const struct session_ranging_data *data);

#endif /* CHERRY_PRIV_H */
