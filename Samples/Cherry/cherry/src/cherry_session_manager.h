/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#ifndef CHERRY_SESSION_MANAGER_H
#define CHERRY_SESSION_MANAGER_H

#include "cherry_priv.h"
#include "cherry_thread.h"

#include <cherry/cherry.h>
#include <cherry/cherry_fira.h>

/**
 * enum cherry_session_state - State of a session
 */
enum cherry_session_state {
	/**
	 * @CHERRY_SESSION_STATE_INIT: The session is created but not
	 * yet started
	 */
	CHERRY_SESSION_STATE_INIT,
	/**
	 * @CHERRY_SESSION_STATE_DEINIT: The session is destroyed, it can
	 * no longer be used.
	 */
	CHERRY_SESSION_STATE_DEINIT,
	/**
	 * @CHERRY_SESSION_STATE_ACTIVE: The session is started.
	 */
	CHERRY_SESSION_STATE_ACTIVE,
	/**
	 * @CHERRY_SESSION_STATE_IDLE: The session is stopped.
	 * Either because the max number of ranging as been reached,
	 * or because the session stop was requested.
	 */
	CHERRY_SESSION_STATE_IDLE,
};

struct session_status_ntf;

typedef void (*child_cherry_cb_t)(void *event, void *user_data);

struct session_force_stop {
	char *event;
	child_cherry_cb_t cb;
};

typedef void (*child_prepare_force_stop_t)(
	const struct cherry_session *session,
	struct session_force_stop *force_stop);
typedef void (*child_error_event_t)(const struct cherry_session *session,
				    enum cherry_err status_err);
typedef void (*child_free_t)(const struct cherry_session *session);
typedef void (*child_status_event_t)(const struct cherry_session *session,
				     struct session_status_ntf *ntf);

/**
 * struct cherry_session_child_cb - Child callbacks.
 * @prepare_force_stop: The callback to force stop the child.
 * @error_event: The callback to send error event to the child.
 * @free: The callback to the child free.
 * @status_event: The callback to send status event to the child.
 */
struct cherry_session_child_cb {
	child_prepare_force_stop_t prepare_force_stop;
	child_error_event_t error_event;
	child_free_t free;
	child_status_event_t status_event;
};

/**
 * struct cherry_session - Generic Cherry session.
 * @next_session: The next session.
 * @cherry_ctx: Pointer to the Cherry context.
 * @type: The type of the session.
 * @child_cb: The child callbacks.
 * @session_handle: FiRa session handle returned by the stack on session creation.
 * @user_data: Pointer to user data provided by applicative and to give to the FiRa callback.
 * @deinit: True if a deinit is in progress, in this case no call is accepted in the session.
 * @session_mutex: Protect the deinit step.
 * @session_state: Session state.
 * @own_by_user: False if force stop can free the session pointer.
 *                        This allows to cover the case where cherry_fira_session destroy is
 *                        called after cherry_destroy, that will trig a force stop.
 * @not_supported_by_uwbs: True if session init failed with CHERRY_ERR_SESSION_TYPE_NOT_SUPPORTED
 */
struct cherry_session {
	struct cherry_session *next_session;
	struct cherry *cherry_ctx;
	enum cherry_session_type type;
	struct cherry_session_child_cb child_cb;
	uint32_t session_handle;
	void *user_data;
	bool deinit;
	struct qmutex *session_mutex;
	enum cherry_session_state session_state;
	bool own_by_user;
	struct cherry_uci_client_session_set_app_config_cmd *set_app_config_cmd;
	bool not_supported_by_uwbs;
};

/* Register a new session in sessions list. */
void cherry_register_session(struct cherry *ctx,
			     struct cherry_session *new_session);

/* Unregister a new session in sessions list. */
void cherry_unregister_session(struct cherry *ctx,
			       struct cherry_session *session);

/* Return True if session is registered into cherry's context */
bool cherry_session_is_registered(struct cherry *ctx,
				  struct cherry_session *session);

/* Destroy all pending sessions and unregister them. */
void cherry_force_stop_all_sessions(struct cherry *ctx);

#endif /* CHERRY_SESSION_MANAGER_H */
