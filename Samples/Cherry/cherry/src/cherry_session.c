/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "cherry_log.h"
#include "cherry_priv.h"
#include "cherry_session_manager.h"

#include <cherry/cherry_session.h>
#include <cherry_session_client.h>
#include <qassert.h>
#include <qmalloc.h>
#include <qsemaphore.h>
#include <qtime.h>

static void cherry_send_session_error_event(struct cherry_session *ctx,
					    enum cherry_err status_err)
{
	ctx->child_cb.error_event(ctx, status_err);
}

static void cherry_session_update_error_state(struct cherry_session *ctx,
					      enum cherry_err status_err)
{
	/*
	 * Indicate the core we has to go in Error state
	 * in case of communication lost.
	 */
	if (status_err == CHERRY_ERR_UWBS_TIMEOUT)
		cherry_set_error_state(ctx->cherry_ctx);

	cherry_send_session_error_event(ctx, status_err);
}

void cherry_session_init(struct cherry_session *session_base,
			 enum cherry_session_type type,
			 struct cherry *cherry_ctx, void *user_data,
			 const struct cherry_session_child_cb *child_cb)
{
	session_base->type = type;
	session_base->user_data = user_data;
	session_base->cherry_ctx = cherry_ctx;
	session_base->deinit = false;
	session_base->own_by_user = true;
	session_base->session_state = CHERRY_SESSION_STATE_INIT;
	session_base->set_app_config_cmd = NULL;
	session_base->not_supported_by_uwbs = false;

	memcpy(&session_base->child_cb, child_cb,
	       sizeof(struct cherry_session_child_cb));

	if ((session_base->session_mutex = qmutex_init()) == NULL) {
		QLOGE("%s: qmutex_init failed.", __func__);
	}
}

enum cherry_err cherry_session_thread_init(struct cherry_session *session_base,
					   const uint32_t session_id,
					   enum uci_session_type session_type)
{
	enum uci_status_code r;
	enum cherry_err err;

	r = cherry_uci_client_session_init_session(
		session_base->cherry_ctx->session_client_ctx, session_id,
		session_type, &session_base->session_handle);
	if (r) {
		QLOGE("%s: error initializing session with error %d.", __func__,
		      r);
		/* Filter on error to know if UWBS does not support the session type. */
		if (r == UCI_STATUS_INVALID_RANGE) {
			err = CHERRY_ERR_SESSION_TYPE_NOT_SUPPORTED;
			session_base->not_supported_by_uwbs = true;
			/* Force stop the unitialized session. */
			cherry_force_stop_session(session_base);
		} else
			err = cherry_get_error(r);

		cherry_session_update_error_state(session_base, err);

		return err;
	}

	/* Send current parameter set right after init to switch session state from INIT to IDLE and
	 * prepare next command to send right before start. */
	return cherry_session_send_current_app_config(session_base, true);
}

struct cherry_session *cherry_get_session_context(struct cherry *ctx,
						  uint32_t session_handle)
{
	struct cherry_session *session_current;

	qmutex_lock(ctx->sessions_mutex, QOSAL_WAIT_FOREVER);
	session_current = ctx->sessions;
	while (session_current) {
		if (session_current->session_handle == session_handle) {
			qmutex_unlock(ctx->sessions_mutex);
			return session_current;
		}
		session_current = session_current->next_session;
	}
	qmutex_unlock(ctx->sessions_mutex);
	QLOGI("%s: No session found with session_handle %04X (can happen after softreset)",
	      __func__, session_handle);
	return NULL;
}

static void cherry_thread_status_cb(const void *user_data, const void *params,
				    bool abort)
{
	struct cherry *ctx = (struct cherry *)user_data;
	struct session_status_ntf *ntf = (struct session_status_ntf *)params;
	struct cherry_session *session_base;

	if (abort)
		goto free_ntf;

	session_base = cherry_get_session_context(ctx, ntf->session_handle);
	if (session_base == NULL)
		goto free_ntf;

	session_base->session_state =
		(enum cherry_session_state)session_status_ntf_get_session_state(
			ntf);

	session_base->child_cb.status_event(session_base, ntf);

free_ntf:
	cherry_uci_client_session_free_status_ntf(ntf);
}

static void uci_client_session_status_cb(const struct session_status_ntf *ntf,
					 void *user_data)
{
	struct cherry *ctx = (struct cherry *)user_data;

	/*
	 * In case where a soft reset is in progress,
	 * all received data have to be ignored.
	 */
	if (ctx->internal_state == CHERRY_CORE_INTERNAL_STATE_REBOOTING)
		goto free;

	/*
	 * Here we pass the cherry context instead of the session context because
	 * it could not be retrieved if this status notification is received before
	 * session initialization response and so session handle would be unset yet.
	 */
	if (cherry_thread_send_prio_task(&ctx->thread_ctx, ctx, ntf, 0,
					 cherry_thread_status_cb) ==
	    CHERRY_ERR_NONE)
		return;
free:
	cherry_uci_client_session_free_status_ntf(
		(struct session_status_ntf *)ntf);
}

static void cherry_session_ntf_cb(const struct session_ranging_data *data)
{
	struct cherry *ctx = (struct cherry *)data->user_data;
	struct cherry_session *session_base;

	session_base = cherry_get_session_context(ctx, data->session_handle);

	if (session_base == NULL)
		return;

	if (data->type == FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_TWR) {
		cherry_fira_twr_ranging_handler(session_base, data);
	} else if (
		data->type ==
			FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_DL_TDOA ||
		data->type ==
			FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_DL_TDOA_V2) {
		cherry_fira_dl_tdoa_ranging_handler(session_base, data);
	} else if (data->type ==
		   FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_CCC_CONTROLLER) {
		cherry_ccc_controller_ranging_handler(session_base, data);
	} else if (data->type ==
		   FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_CCC_CONTROLEE) {
		cherry_ccc_controlee_ranging_handler(session_base, data);
	}
}

int cherry_session_uci_client_setup(struct cherry *cherry_ctx)
{
	int r;

	if (cherry_ctx->session_client_ctx == NULL) {
		r = cherry_uci_client_session_open(
			&cherry_ctx->session_client_ctx, &cherry_ctx->uci,
			cherry_ctx, uci_client_session_status_cb,
			cherry_session_ntf_cb);
		if (r)
			return r;
	}
	return 0;
}

void cherry_session_close(struct cherry *cherry_ctx)
{
	if (cherry_ctx->session_client_ctx)
		cherry_uci_client_session_close(cherry_ctx->session_client_ctx);
}

/* get user data. */

void *cherry_session_get_user_data(struct cherry_session *session)
{
	void *ptr;

	if (!session)
		return NULL;

	qmutex_lock(session->session_mutex, QOSAL_WAIT_FOREVER);
	if (session->deinit) {
		QLOGI("%s session deinit", __func__);
		qmutex_unlock(session->session_mutex);
		return NULL;
	}
	ptr = session->user_data;

	qmutex_unlock(session->session_mutex);
	return ptr;
}

/* session state. */

static __attribute__((unused)) char *
get_state_str(enum cherry_session_state state)
{
	switch (state) {
	case CHERRY_SESSION_STATE_INIT:
		return "Init";
	case CHERRY_SESSION_STATE_DEINIT:
		return "Deinit";
	case CHERRY_SESSION_STATE_ACTIVE:
		return "Active";
	case CHERRY_SESSION_STATE_IDLE:
		return "Idle";
	default:
		return "Unknown";
	}
	return NULL;
}

static bool wait_stop_state(struct cherry_session *session)
{
	int64_t timeout;
	int64_t timeout_ms;

	timeout = qtime_get_uptime_us() + CHERRY_NOTIFICATION_TIMEOUT_US;
	do {
		if (session->session_state != CHERRY_SESSION_STATE_ACTIVE)
			return true;
		timeout_ms = (timeout - qtime_get_uptime_us()) / 1000;
		if (timeout_ms <= 0)
			break;

		cherry_thread_process_prio_task(
			&session->cherry_ctx->thread_ctx, timeout_ms);
	} while (true);

	QLOGE("Session stop notification not received.");
	cherry_session_update_error_state(session, CHERRY_ERR_UWBS_TIMEOUT);

	return false;
}

static void wait_state(struct cherry_session *session,
		       enum cherry_session_state state)
{
	int64_t timeout;
	int64_t timeout_ms;

	timeout = qtime_get_uptime_us() + CHERRY_NOTIFICATION_TIMEOUT_US;
	do {
		if (session->session_state == state)
			return;
		timeout_ms = (timeout - qtime_get_uptime_us()) / 1000;
		if (timeout_ms <= 0)
			break;

		cherry_thread_process_prio_task(
			&session->cherry_ctx->thread_ctx, timeout_ms);
	} while (true);

	QLOGE("Session state %s notification not received.",
	      get_state_str(state));
	cherry_session_update_error_state(session, CHERRY_ERR_UWBS_TIMEOUT);
}

/* session destroy. */

static void cherry_session_free(struct cherry_session *session)
{
	struct qmutex *session_mutex = session->session_mutex;

	cherry_uci_client_session_set_app_config_cmd_abort(
		session->set_app_config_cmd);
	session->child_cb.free(session);

	if (session_mutex)
		qmutex_deinit(session_mutex);
}

static void cherry_session_thread_task_deinit(const void *context,
					      const void *params, bool abort)
{
	struct cherry_session *session = (struct cherry_session *)context;
	struct cherry *ctx;
	int r;
	bool stopped = false;

	if (abort)
		return;

	QASSERT(session);
	ctx = session->cherry_ctx;

	/* Call stop if needed. */
	if (session->session_state == CHERRY_SESSION_STATE_ACTIVE) {
		if (!ctx->uci_transport_ctx.transport) {
			QLOGE("%s: no transport, task can not be sent.",
			      __func__);
		} else {
			r = cherry_uci_client_session_stop_session(
				ctx->session_client_ctx,
				session->session_handle);
			if (r) {
				QLOGE("%s: error stopping session session with error %d",
				      __func__, r);
			} else
				stopped = wait_stop_state(session);
		}
	} else
		stopped = true;

	/*
	 * If stop has not been done (no available transport)
	 * or stop notification not received : no need to try do deinit the session.
	 */
	if (stopped) {
		/* Call deinit. */
		if (!ctx->uci_transport_ctx.transport) {
			QLOGE("%s: no transport, task can not be sent.",
			      __func__);
		} else {
			r = cherry_uci_client_session_deinit_session(
				ctx->session_client_ctx,
				session->session_handle);
			if (r) {
				QLOGD("%s: error on deinit session session with error %d.",
				      __func__, r);
			}
			wait_state(session, CHERRY_SESSION_STATE_DEINIT);
		}
	}

	/*
	 * Unregister the session on cherry core:
	 * If stop command has been sent and notification has not been received (same
	 * for deinit command), a force stop has been called and cherry_ctx can be null.
	 */
	if (session->cherry_ctx)
		cherry_unregister_session(session->cherry_ctx, session);
	/*
	 * In all cases `cherry_session_destroy` has been called and session pointer
	 * has to be freed.
	 */
	cherry_session_free(session);
}

void cherry_session_destroy(struct cherry_session *session)
{
	if (!session)
		return;

	qmutex_lock(session->session_mutex, QOSAL_WAIT_FOREVER);

	/* Check if session has been forced stop. */
	if (!session->cherry_ctx) {
		/* Mutex is deinit in `cherry_session_free` and unlock can not be called */
		cherry_session_free(session);
		return;
	}

	/* Protect from double call to destroy. */
	if (session->deinit) {
		QLOGI("%s session deinit", __func__);
		qmutex_unlock(session->session_mutex);
		return;
	}

	session->deinit = true;

	/* Do deinit asynchronously. */
	if (cherry_thread_send_list_task(&session->cherry_ctx->thread_ctx,
					 session, NULL, 0,
					 cherry_session_thread_task_deinit,
					 false) != CHERRY_ERR_NONE)
		/* Session context can be freed in `cherry_session_thread_task_deinit` or `cherry_session_force_stop` */
		session->own_by_user = false;
	qmutex_unlock(session->session_mutex);
}

void cherry_session_force_stop(struct cherry_session *session_base)
{
	struct session_force_stop force_stop;
	bool own_by_user = session_base->own_by_user;
	struct cherry_thread *thread_ctx =
		&session_base->cherry_ctx->thread_ctx;
	void *user_data = session_base->user_data;

	/*
	 * Save some session context data to be able to send the DEINIT message
	 * The session context can be freed if applicative calls `cherry_session_destroy`
	 * after `session_base->cherry_ctx` is set to null.
	 */
	session_base->child_cb.prepare_force_stop(session_base, &force_stop);

	qmutex_lock(session_base->session_mutex, QOSAL_WAIT_FOREVER);
	/* Deinit set: this indicates the DEINIT EVENT has been sent and prevent
	 * futher calls excepted the `cherry_***_session_destroy` call.
	 */
	session_base->deinit = true;
	/* Force stop: the session does not reference any more Cherry context. */
	session_base->cherry_ctx = NULL;
	session_base->session_state = CHERRY_SESSION_STATE_DEINIT;
	qmutex_unlock(session_base->session_mutex);

	/*
	 * At this step the session context can be freed if applicative calls
	 * `cherry_session_destroy` after `session_base->cherry_ctx` is set to
	 * null. That's why we called a `prepare_force_stop` callback and all
	 * used data now were saved before.
	 */
	force_stop.cb(force_stop.event, user_data);

	/* Remove the pending tasks on the session. */
	cherry_thread_remove_pending_tasks(thread_ctx, session_base);

	if (!own_by_user)
		cherry_session_free(session_base);
}

enum cherry_err
cherry_session_send_current_app_config(struct cherry_session *session_base,
				       bool create_new_cmd)
{
	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	struct cherry_uci_client_session_set_app_config_cmd *cmd_next = NULL;
	enum uci_status_code r;

	if (create_new_cmd) {
		cmd_next = cherry_uci_client_session_set_app_config_cmd_create(
			session_base->cherry_ctx->session_client_ctx);
		if (!cmd_next) {
			QLOGE("%s: failed to create session next command",
			      __func__);
			return CHERRY_ERR_INTERNAL;
		}
	}

	qmutex_lock(session_base->session_mutex, QOSAL_WAIT_FOREVER);
	cmd = session_base->set_app_config_cmd;
	session_base->set_app_config_cmd = cmd_next;
	qmutex_unlock(session_base->session_mutex);

	if (!cmd)
		return CHERRY_ERR_NONE;

	r = cherry_uci_client_session_set_app_config_cmd_send(
		cmd, session_base->session_handle);
	if (r != UCI_STATUS_OK) {
		QLOGE("%s: error setting parameters with error %d.", __func__,
		      r);
		cherry_session_update_error_state(session_base,
						  cherry_get_error(r));
	}
	return cherry_get_error(r);
}

/* session start. */

static void cherry_session_thread_task_start(const void *context,
					     const void *params, bool abort)
{
	struct cherry_session *session = (struct cherry_session *)context;
	int r;
	enum cherry_err err;

	if (abort)
		return;

	if (!session->cherry_ctx->uci_transport_ctx.transport) {
		QLOGE("%s: no transport, task can not be sent.", __func__);
		cherry_session_update_error_state(session, CHERRY_ERR_INTERNAL);
		return;
	}

	err = cherry_session_send_current_app_config(session, false);
	if (err != CHERRY_ERR_NONE)
		return;

	r = cherry_uci_client_session_start_session(
		session->cherry_ctx->session_client_ctx,
		session->session_handle);
	if (r) {
		QLOGE("%s: error starting session with error %d", __func__, r);
		cherry_session_update_error_state(session, cherry_get_error(r));
		return;
	}

	wait_state(session, CHERRY_SESSION_STATE_ACTIVE);
}

enum cherry_err cherry_session_start(struct cherry_session *session)
{
	enum cherry_err err = CHERRY_ERR_INVALID_PARAMETER;

	if (!session) {
		QLOGE("%s no session", __func__);
		return err;
	}

	qmutex_lock(session->session_mutex, QOSAL_WAIT_FOREVER);
	if (session->deinit) {
		QLOGI("%s session deinit", __func__);
		if (session->not_supported_by_uwbs)
			err = CHERRY_ERR_SESSION_TYPE_NOT_SUPPORTED;
		goto error;
	}

	err = cherry_thread_send_list_task(&session->cherry_ctx->thread_ctx,
					   session, session, sizeof(session),
					   cherry_session_thread_task_start,
					   false);
error:
	qmutex_unlock(session->session_mutex);
	return err;
}

/* session stop.*/

static void cherry_session_thread_task_stop(const void *context,
					    const void *params, bool abort)
{
	struct cherry_session *session = (struct cherry_session *)context;
	int r;

	if (abort)
		return;

	if (!session->cherry_ctx->uci_transport_ctx.transport) {
		QLOGE("%s: no transport, task can not be sent.", __func__);
		cherry_session_update_error_state(session, CHERRY_ERR_INTERNAL);
		return;
	}
	r = cherry_uci_client_session_stop_session(
		session->cherry_ctx->session_client_ctx,
		session->session_handle);
	if (r) {
		QLOGE("%s: error stopping session session with error %d",
		      __func__, r);
		cherry_session_update_error_state(session, cherry_get_error(r));
		return;
	}

	wait_stop_state(session);
}
enum cherry_err cherry_session_stop(struct cherry_session *session)
{
	enum cherry_err err = CHERRY_ERR_INVALID_PARAMETER;

	if (!session) {
		QLOGE("%s no session", __func__);
		return err;
	}

	qmutex_lock(session->session_mutex, QOSAL_WAIT_FOREVER);
	if (session->deinit) {
		QLOGI("%s session deinit", __func__);
		if (session->not_supported_by_uwbs)
			err = CHERRY_ERR_SESSION_TYPE_NOT_SUPPORTED;
		goto error;
	}

	err = cherry_thread_send_list_task(&session->cherry_ctx->thread_ctx,
					   session, NULL, 0,
					   cherry_session_thread_task_stop,
					   false);
error:
	qmutex_unlock(session->session_mutex);
	return err;
}

static void cherry_thread_session_set_app_config_cmd_send(const void *context,
							  const void *params,
							  bool abort)
{
	struct cherry_session *session_base = (struct cherry_session *)context;

	if (abort)
		return;

	if (!session_base->cherry_ctx->uci_transport_ctx.transport) {
		QLOGE("%s: no transport, task can not be sent.", __func__);
		cherry_session_update_error_state(session_base,
						  CHERRY_ERR_INTERNAL);
		return;
	}

	cherry_session_send_current_app_config(session_base, false);
}

static enum cherry_err
cherry_session_set_app_config_cmd_create(struct cherry_session *session_base)
{
	enum cherry_err err;

	if (session_base->set_app_config_cmd)
		return CHERRY_ERR_NONE;

	session_base->set_app_config_cmd =
		cherry_uci_client_session_set_app_config_cmd_create(
			session_base->cherry_ctx->session_client_ctx);
	if (!session_base->set_app_config_cmd) {
		QLOGE("%s: failed to create session set_app_config_cmd",
		      __func__);
		return CHERRY_ERR_INTERNAL;
	}

	err = cherry_thread_send_list_task(
		&session_base->cherry_ctx->thread_ctx, session_base, NULL, 0,
		cherry_thread_session_set_app_config_cmd_send, false);

	return err;
}

enum cherry_err
cherry_session_set_app_config_begin(struct cherry_session *session_base)
{
	if (!session_base) {
		QLOGE("%s no session", __func__);
		return CHERRY_ERR_INVALID_PARAMETER;
	}

	qmutex_lock(session_base->session_mutex, QOSAL_WAIT_FOREVER);
	if (session_base->deinit) {
		QLOGI("%s session deinit", __func__);
		if (session_base->not_supported_by_uwbs)
			return CHERRY_ERR_SESSION_TYPE_NOT_SUPPORTED;
		return CHERRY_ERR_INVALID_PARAMETER;
	}

	return cherry_session_set_app_config_cmd_create(session_base);
}

void cherry_session_set_app_config_end(struct cherry_session *session_base)
{
	qmutex_unlock(session_base->session_mutex);
}

/* se channel. */

enum cherry_err cherry_session_set_channel(struct cherry_session *session,
					   int channel)
{
	CHERRY_SESSION_SET_PARAM(
		session,
		cherry_uci_client_session_set_app_config_cmd_put_channel_number(
			cmd, channel));
}

/* set preamble code index. */

enum cherry_err
cherry_session_set_preamble_code_index(struct cherry_session *session,
				       uint8_t preamble_code_index)
{
	CHERRY_SESSION_SET_PARAM(
		session,
		cherry_uci_client_session_set_app_config_cmd_put_preamble_code_index(
			cmd, preamble_code_index));
}

/* set priority */

enum cherry_err cherry_session_set_priority(struct cherry_session *session,
					    uint8_t session_priority)
{
	CHERRY_SESSION_SET_PARAM(
		session,
		cherry_uci_client_session_set_app_config_cmd_put_priority(
			cmd, session_priority));
}

/* set antennas */

enum cherry_err cherry_session_set_antennas(struct cherry_session *session,
					    uint8_t tx_antenna_set,
					    uint8_t rx_antenna_set)
{
	/* clang-format off */
	CHERRY_SESSION_SET_PARAMS(
		session,
		(cherry_uci_client_session_set_app_config_cmd_put_tx_antenna_selection(cmd, tx_antenna_set))
		(cherry_uci_client_session_set_app_config_cmd_put_rx_antenna_selection(cmd, rx_antenna_set))
	);
	/* clang-format on */
}

struct ref_time_params {
	struct cherry_session *session_reference;
	uint32_t offset_us;
};

static void cherry_thread_task_set_ref_time(const void *context,
					    const void *params, bool abort)
{
	struct cherry_session *session_base = (struct cherry_session *)context;
	struct ref_time_params *ref_time_params =
		(struct ref_time_params *)params;
	struct cherry_session *session_reference =
		ref_time_params->session_reference;
	uint32_t offset_us = ref_time_params->offset_us;
	enum uci_status_code r;

	if (abort)
		return;

	qmutex_lock(session_base->session_mutex, QOSAL_WAIT_FOREVER);
	r = cherry_uci_client_session_set_app_config_cmd_put_session_time_base(
		session_base->set_app_config_cmd, true, true, true,
		session_reference->session_handle, offset_us);
	if (r)
		cherry_session_update_error_state(session_base,
						  cherry_get_error(r));
	qmutex_unlock(session_base->session_mutex);
	return;
}

enum cherry_err
cherry_session_set_ref_time_base(struct cherry_session *session,
				 struct cherry_session *session_reference,
				 uint32_t offset_us)
{
	struct ref_time_params params;

	params.offset_us = offset_us;
	params.session_reference = session_reference;

	qmutex_lock(session->session_mutex, QOSAL_WAIT_FOREVER);

	if (session->deinit) {
		QLOGI("%s session deinit", __func__);
		qmutex_unlock(session->session_mutex);
		if (session->not_supported_by_uwbs)
			return CHERRY_ERR_SESSION_TYPE_NOT_SUPPORTED;
		return CHERRY_ERR_INVALID_PARAMETER;
	}
	cherry_thread_send_list_task(&session->cherry_ctx->thread_ctx, session,
				     &params, sizeof(struct ref_time_params),
				     cherry_thread_task_set_ref_time, false);

	qmutex_unlock(session->session_mutex);
	return CHERRY_ERR_NONE;
}
