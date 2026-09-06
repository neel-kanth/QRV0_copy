/*
 * This example purpose is to run a sample app for Radar.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "cherry_log.h"
#include "cherry_priv.h"
#include "cherry_session_manager.h"

#include <cherry/cherry_radar.h>
#include <cherry_radar_client.h>
#include <cherry_session_client.h>
#include <qassert.h>
#include <qmalloc.h>
#include <qsemaphore.h>

/**
 * struct cherry_radar_session - Cherry radar session structure.
 * @radar_cb: The Radar callback used to send an event to the applicative.
 * @session_base: The cherry_session will be used by the Cherry core.
*/
struct cherry_radar_session {
	cherry_radar_cb_t radar_cb;
	struct cherry_session session_base;
};

static struct cherry_radar_event *cherry_radar_alloc_event()
{
	struct cherry_radar_event *event;

	event = qmalloc(sizeof(struct cherry_radar_event));
	if (!event) {
		QLOGE("%s: Unable to allocate event.", __func__);
	}
	return event;
}

static struct cherry_radar_event *cherry_radar_alloc_event_status(void)
{
	struct cherry_radar_event *event = cherry_radar_alloc_event();
	if (!event) {
		return NULL;
	}

	event->type = CHERRY_RADAR_EVENT_TYPE_SESSION_STATUS;
	event->data.status = (struct cherry_radar_session_event_session_status *)
		qmalloc(sizeof(
			struct cherry_radar_session_event_session_status));
	if (event->data.status)
		return event;

	QLOGE("%s: Unable to allocate event.", __func__);

	qfree(event);
	return NULL;
}

static struct cherry_radar_event *cherry_radar_alloc_event_error(void)
{
	struct cherry_radar_event *event = cherry_radar_alloc_event();
	if (!event) {
		return NULL;
	}

	event->type = CHERRY_RADAR_EVENT_TYPE_SESSION_ERROR;
	event->data.error = (struct cherry_radar_session_event_error *)qmalloc(
		sizeof(struct cherry_radar_session_event_error));
	if (event->data.error)
		return event;

	QLOGE("%s: Unable to allocate event.", __func__);

	qfree(event);
	return NULL;
}

static struct cherry_radar_event *cherry_radar_alloc_event_radar_report(
	struct cherry_radar_session_report *report)
{
	struct cherry_radar_event *event = cherry_radar_alloc_event();
	if (!event)
		return NULL;

	event->type = CHERRY_RADAR_EVENT_TYPE_SESSION_REPORT;
	event->data.report = report;

	return event;
}

static void cherry_radar_send_error_event(struct cherry_radar_session *ctx,
					  enum cherry_err status_err)
{
	struct cherry_radar_event *event;

	event = cherry_radar_alloc_event_error();
	if (!event)
		return;

	event->data.error->session = ctx;
	event->data.error->status_err = status_err;
	ctx->radar_cb(event, ctx->session_base.user_data);
}

struct cherry_session *
cherry_radar_session_to_base(struct cherry_radar_session *session)
{
	if (session)
		return &session->session_base;
	return NULL;
}

static struct cherry_radar_session *
cherry_radar_get_child(const struct cherry_session *session_base)
{
	return qparent_of(session_base, struct cherry_radar_session,
			  session_base);
}

static enum cherry_err
cherry_radar_send_list_task(const struct cherry_session *session_base,
			    const void *params, size_t params_size,
			    cherry_thread_task_t thread_task, bool destroy)
{
	return cherry_thread_send_list_task(
		&session_base->cherry_ctx->thread_ctx, session_base, params,
		params_size, thread_task, destroy);
}

static void cherry_radar_update_error_state(struct cherry_radar_session *ctx,
					    enum cherry_err status_err)
{
	/*
	 * Indicate the core we have to go in Error state
	 * because of communication lost.
	 */
	if (status_err == CHERRY_ERR_UWBS_TIMEOUT)
		cherry_set_error_state(ctx->session_base.cherry_ctx);

	cherry_radar_send_error_event(ctx, status_err);
}

static void cherry_radar_free(struct cherry_radar_session *session)
{
	qfree(session);
}

void cherry_radar_close(struct cherry *cherry_ctx)
{
	if (cherry_ctx->radar_client_ctx)
		cherry_uci_client_radar_close(cherry_ctx->radar_client_ctx);
}

static enum cherry_radar_state_change_reason
convert_uci_session_reason_code_to_cherry_radar(
	enum uci_session_reason_code code)
{
	switch (code) {
	case UCI_SESSION_REASON_STATE_CHANGE_WITH_SESSION_MANAGEMENT_COMMANDS:
		return CHERRY_RADAR_STATE_CHANGE_REASON_MGMT_CMD;
	case UCI_SESSION_REASON_MAX_NUMBER_OF_MEASUREMENTS_REACHED:
		return CHERRY_RADAR_STATE_CHANGE_REASON_MAX_MEASUREMENT;
	default:
		break;
	}
	return CHERRY_RADAR_STATE_CHANGE_REASON_UNKNOWN;
}

static void cherry_thread_radar_uci_client_radar_report_ntf_cb(
	const void *context, const void *params, bool abort)
{
	struct cherry_session *session_base = (struct cherry_session *)context;
	struct cherry_uci_radar_ntf *report =
		(struct cherry_uci_radar_ntf *)params;
	struct cherry_radar_session *session;
	struct cherry_radar_event *event;

	if (abort)
		goto free_radar_report;

	if (!session_base)
		goto free_radar_report;

	session = cherry_radar_get_child(session_base);

	event = cherry_radar_alloc_event_radar_report(report->data);
	if (!event)
		goto free_radar_report;

	event->data.report->session = session;
	session->radar_cb(event, session->session_base.user_data);

	/*
	 * Only free the base of the notification, data are a part of the event,
	 * And it will be free in `cherry_radar_event_free`.
	 */
	cherry_uci_client_radar_free_base_report(report);
	return;

free_radar_report:
	cherry_uci_client_radar_free_data_report(report->data);
	cherry_uci_client_radar_free_base_report(report);
}

static void
radar_uci_client_radar_report_ntf_cb(const struct cherry_uci_radar_ntf *report,
				     void *user_data)
{
	struct cherry *ctx = (struct cherry *)user_data;
	struct cherry_session *session_base;

	session_base = cherry_get_session_context(ctx, report->session_handle);
	if (session_base == NULL)
		goto free_radar_report;

	if (cherry_thread_send_prio_task(
		    &ctx->thread_ctx, session_base, report, 0,
		    cherry_thread_radar_uci_client_radar_report_ntf_cb) ==
	    CHERRY_ERR_NONE)
		return;

free_radar_report:
	cherry_uci_client_radar_free_data_report(report->data);
	cherry_uci_client_radar_free_base_report(
		(struct cherry_uci_radar_ntf *)report);
}

static void
cherry_radar_child_prepare_force_stop(const struct cherry_session *session_base,
				      struct session_force_stop *force_stop)
{
	struct cherry_radar_session *session =
		cherry_radar_get_child(session_base);
	struct cherry_radar_event *event;

	event = cherry_radar_alloc_event_status();
	if (!event)
		return;

	event->data.status->session = session;
	event->data.status->session_state = CHERRY_RADAR_SESSION_STATE_DEINIT;
	event->data.status->reason_code =
		CHERRY_RADAR_STATE_CHANGE_REASON_FORCE_STOPPED;

	force_stop->event = (void *)event;
	force_stop->cb = (child_cherry_cb_t)session->radar_cb;

	return;
}

static void
cherry_radar_child_error_event(const struct cherry_session *session_base,
			       enum cherry_err status_err)
{
	struct cherry_radar_session *session =
		cherry_radar_get_child(session_base);

	cherry_radar_send_error_event(session, status_err);
}

static void cherry_radar_child_free(const struct cherry_session *session_base)
{
	struct cherry_radar_session *session =
		cherry_radar_get_child(session_base);

	cherry_radar_free(session);
}

static void
cherry_radar_child_status_event(const struct cherry_session *session_base,
				struct session_status_ntf *ntf)
{
	struct cherry_radar_session *session;
	struct cherry_radar_event *event;

	session = cherry_radar_get_child(session_base);
	if (session == NULL)
		return;

	event = cherry_radar_alloc_event_status();
	if (!event)
		return;

	event->data.status->session = session;
	event->data.status->session_state = (enum cherry_radar_session_state)
		session_status_ntf_get_session_state(ntf);
	event->data.status->reason_code =
		convert_uci_session_reason_code_to_cherry_radar(
			session_status_ntf_get_reason_code(ntf));

	session->radar_cb(event, session->session_base.user_data);
}

static int cherry_radar_uci_client_setup(struct cherry *cherry_ctx)
{
	int r;

	if (cherry_ctx->radar_client_ctx == NULL) {
		r = cherry_uci_client_radar_open(
			&cherry_ctx->radar_client_ctx, &cherry_ctx->uci,
			cherry_ctx, radar_uci_client_radar_report_ntf_cb);
		if (r)
			return r;
	}

	return 0;
}

static void cherry_thread_task_create_radar_session(const void *context,
						    const void *params,
						    bool abort)
{
	struct cherry_session *session_base = (struct cherry_session *)context;
	uint32_t session_id = *(uint32_t *)params;
	struct cherry_radar_session *session =
		cherry_radar_get_child(session_base);

	if (abort)
		return;

	/* Register the session to the cherry core. */
	cherry_register_session(session_base->cherry_ctx, session_base);

	if (!session_base->cherry_ctx->uci_transport_ctx.transport) {
		QLOGE("%s: no transport, task can not be sent.", __func__);
		cherry_radar_update_error_state(session, CHERRY_ERR_INTERNAL);
		return;
	}

	cherry_session_thread_init(session_base, session_id,
				   UCI_SESSION_TYPE_RADAR);
}

static struct cherry_radar_session *
cherry_radar_session_create_base(struct cherry *cherry_ctx,
				 cherry_radar_cb_t callback, void *user_data)
{
	struct cherry_session_child_cb child_cb;
	enum uci_status_code r;
	struct cherry_radar_session *session;

	session = (struct cherry_radar_session *)qcalloc(
		1, sizeof(struct cherry_radar_session));
	if (!session)
		return NULL;

	session->radar_cb = callback;

	/* Initialize the session base. */
	child_cb.prepare_force_stop = cherry_radar_child_prepare_force_stop;
	child_cb.error_event = cherry_radar_child_error_event;
	child_cb.free = cherry_radar_child_free;
	child_cb.status_event = cherry_radar_child_status_event;

	cherry_session_init(&session->session_base, CHERRY_SESSION_TYPE_RADAR,
			    cherry_ctx, user_data, &child_cb);

	r = cherry_radar_uci_client_setup(session->session_base.cherry_ctx);
	if (r) {
		QLOGE("%s: client setup failed with error %d.", __func__, r);
		cherry_radar_free(session);
		return NULL;
	}

	return session;
}

struct cherry_radar_session *cherry_radar_session_create(
	struct cherry *ctx, cherry_radar_cb_t callback, void *user_data,
	uint32_t session_id, uint32_t burst_period_ms,
	uint16_t sweep_period_rstu, uint8_t sweeps_per_burst,
	uint8_t samples_per_sweep, uint8_t antenna_set)
{
	struct cherry_radar_session *session;
	struct cherry_session *session_base;
	enum uci_status_code r;

	if (!ctx || !callback)
		return NULL;

	if (!cherry_state_is_valid(ctx)) {
		QLOGE("%s: internal state error.", __func__);
		return NULL;
	}

	session = cherry_radar_session_create_base(ctx, callback, user_data);
	if (!session)
		return NULL;

	session_base = &session->session_base;

	/* Prepare all parameters to set. */
	session_base->set_app_config_cmd =
		cherry_uci_client_session_set_app_config_cmd_create(
			session_base->cherry_ctx->session_client_ctx);
	if (!session_base->set_app_config_cmd) {
		QLOGE("%s: failed to create fira set_app_config_cmd", __func__);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_radar_timing_params(
		session_base->set_app_config_cmd, burst_period_ms,
		sweep_period_rstu, sweeps_per_burst);
	if (r) {
		QLOGE("%s: error setting radar timing params with error %d.",
		      __func__, r);
		goto error;
	}

	r = cherry_uci_client_session_set_app_config_cmd_put_radar_samples_per_sweep(
		session_base->set_app_config_cmd, samples_per_sweep);
	if (r) {
		QLOGE("%s: error setting radar samples per sweep with error %d.",
		      __func__, r);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_radar_antenna_set_id(
		session_base->set_app_config_cmd, antenna_set);
	if (r) {
		QLOGE("%s: error setting radar antenna set id with error %d.",
		      __func__, r);
		goto error;
	}

	if (cherry_radar_send_list_task(&session->session_base, &session_id,
					sizeof(session_id),
					cherry_thread_task_create_radar_session,
					false) != CHERRY_ERR_NONE) {
		QLOGE("%s: can not create the session.", __func__);
		goto error;
	}

	return session;

error:
	cherry_radar_free(session);
	return NULL;
}

void cherry_radar_event_free(struct cherry_radar_event *event)
{
	if (!event)
		return;

	switch (event->type) {
	case CHERRY_RADAR_EVENT_TYPE_SESSION_STATUS:
		if (event->data.status)
			qfree(event->data.status);
		break;
	case CHERRY_RADAR_EVENT_TYPE_SESSION_ERROR:
		if (event->data.error)
			qfree(event->data.error);
		break;
	case CHERRY_RADAR_EVENT_TYPE_SESSION_REPORT:
		if (event->data.report)
			cherry_uci_client_radar_free_data_report(
				event->data.report);
		break;
	default:
		break;
	}

	qfree(event);
}

enum cherry_err
cherry_radar_session_set_number_of_bursts(struct cherry_radar_session *session,
					  uint16_t max_burst)
{
	CHERRY_SESSION_SET_PARAM(
		&session->session_base,
		cherry_uci_client_session_set_app_config_cmd_put_radar_max_burst(
			cmd, max_burst));
}

enum cherry_err
cherry_radar_session_set_sweep_offset(struct cherry_radar_session *session,
				      int16_t offset)
{
	CHERRY_SESSION_SET_PARAM(
		&session->session_base,
		cherry_uci_client_session_set_app_config_cmd_put_radar_sweep_offset(
			cmd, offset));
}

enum cherry_err
cherry_radar_session_set_tx_profile_idx(struct cherry_radar_session *session,
					uint8_t idx)
{
	CHERRY_SESSION_SET_PARAM(
		&session->session_base,
		cherry_uci_client_session_set_app_config_cmd_put_radar_tx_profile_idx(
			cmd, idx));
}
