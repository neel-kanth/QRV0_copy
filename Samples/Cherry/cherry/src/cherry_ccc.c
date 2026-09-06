/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "cherry_log.h"
#include "cherry_priv.h"
#include "cherry_session_manager.h"

#include <cherry/cherry_ccc.h>
#include <cherry_ccc_client.h>
#include <cherry_session_client.h>
#include <qassert.h>
#include <qmalloc.h>
#include <qsemaphore.h>
#include <qtime.h>

_Static_assert(CHERRY_STATIC_STS_SIZE == STATIC_STS_IV_SIZE,
	       "inconsistent values");

_Static_assert(CHERRY_VENDOR_ID_SIZE == VENDOR_ID_SIZE, "inconsistent values");

/**
 * struct cherry_ccc_session - Cherry ccc session structure.
 * @ccc_cb: The FiRa callback used to send an event to the applicative.
 * @session_base: The cherry_session will be used by the Cherry core.
*/
struct cherry_ccc_session {
	cherry_ccc_cb_t ccc_cb;
	struct cherry_session session_base;
};

/**
 * struct cherry_ccc_aliro_params - Cherry ccc params structure.
 * @is_controller: Boolean to indicate if the session is controller or controlee.
 * @config: Basic session configuration parameters.
 * @n_responder: Number of responders.
 */
struct cherry_ccc_aliro_params {
	bool is_controller;
	struct cherry_ccc_aliro_session_config *config;
	uint8_t n_responder;
};

static struct cherry_ccc_event *cherry_ccc_alloc_event(void)
{
	struct cherry_ccc_event *event;

	event = qmalloc(sizeof(struct cherry_ccc_event));
	if (!event) {
		QLOGE("%s: Unable to allocate event.", __func__);
	}
	return event;
}

static struct cherry_ccc_event *cherry_ccc_alloc_event_status(void)
{
	struct cherry_ccc_event *event = cherry_ccc_alloc_event();
	if (!event) {
		return NULL;
	}

	event->type = CHERRY_CCC_EVENT_TYPE_SESSION_STATUS;
	event->data.status =
		(struct cherry_ccc_session_event_session_status *)qmalloc(
			sizeof(struct cherry_ccc_session_event_session_status));
	if (event->data.status)
		return event;

	QLOGE("%s: Unable to allocate event.", __func__);

	qfree(event);
	return NULL;
}

static struct cherry_ccc_event *cherry_ccc_alloc_event_error(void)
{
	struct cherry_ccc_event *event = cherry_ccc_alloc_event();
	if (!event) {
		return NULL;
	}

	event->type = CHERRY_CCC_EVENT_TYPE_SESSION_ERROR;
	event->data.error = (struct cherry_ccc_session_event_error *)qmalloc(
		sizeof(struct cherry_ccc_session_event_error));
	if (event->data.error)
		return event;

	QLOGE("%s: Unable to allocate event.", __func__);

	qfree(event);
	return NULL;
}

static struct cherry_ccc_event *cherry_ccc_alloc_event_report_controller(
	struct cherry_ccc_controller_session_report *controller_report,
	struct cherry_ccc_session *session)
{
	struct cherry_ccc_event *event = cherry_ccc_alloc_event();
	if (!event) {
		return NULL;
	}

	event->type = CHERRY_CCC_EVENT_TYPE_SESSION_CONTROLLER_REPORT;
	event->session = session;
	event->data.controller_report = controller_report;
	return event;
}

static struct cherry_ccc_event *cherry_ccc_alloc_event_report_controlee(
	struct cherry_ccc_controlee_session_report *controlee_report,
	struct cherry_ccc_session *session)
{
	struct cherry_ccc_event *event = cherry_ccc_alloc_event();
	if (!event) {
		return NULL;
	}

	event->type = CHERRY_CCC_EVENT_TYPE_SESSION_CONTROLEE_REPORT;
	event->session = session;
	event->data.controlee_report = controlee_report;
	return event;
}

static void cherry_ccc_send_error_event(struct cherry_ccc_session *ctx,
					enum cherry_err status_err)
{
	struct cherry_ccc_event *event;

	event = cherry_ccc_alloc_event_error();
	if (!event)
		return;

	event->session = ctx;
	event->data.error->status_err = status_err;
	ctx->ccc_cb(event, ctx->session_base.user_data);
}

struct cherry_session *
cherry_ccc_session_to_base(struct cherry_ccc_session *session)
{
	if (session)
		return &session->session_base;
	return NULL;
}

static struct cherry_ccc_session *
cherry_ccc_get_child(const struct cherry_session *session_base)
{
	return qparent_of(session_base, struct cherry_ccc_session,
			  session_base);
}

static enum cherry_err
cherry_ccc_send_list_task(const struct cherry_session *session_base,
			  const void *params, size_t params_size,
			  cherry_thread_task_t thread_task, bool destroy)
{
	return cherry_thread_send_list_task(
		&session_base->cherry_ctx->thread_ctx, session_base, params,
		params_size, thread_task, destroy);
}

static void cherry_ccc_update_error_state(struct cherry_ccc_session *ctx,
					  enum cherry_err status_err)
{
	/*
	 * Indicate the core we have to go in Error state
	 * because of communication lost.
	 */
	if (status_err == CHERRY_ERR_UWBS_TIMEOUT)
		cherry_set_error_state(ctx->session_base.cherry_ctx);

	cherry_ccc_send_error_event(ctx, status_err);
}

static void cherry_ccc_free(struct cherry_ccc_session *session)
{
	qfree(session);
}

void cherry_ccc_close(struct cherry *cherry_ctx)
{
}

static enum cherry_ccc_state_change_reason
convert_uci_session_reason_code_to_cherry_ccc(enum uci_session_reason_code code)
{
	switch (code) {
	case UCI_SESSION_REASON_STATE_CHANGE_WITH_SESSION_MANAGEMENT_COMMANDS:
		return CHERRY_CCC_STATE_CHANGE_REASON_MGMT_CMD;
	default:
		break;
	}
	return CHERRY_CCC_STATE_CHANGE_REASON_UNKNOWN;
}

static void cherry_thread_ccc_uci_client_report_controller_ntf_cb(
	const void *context, const void *params, bool abort)
{
	struct cherry_session *session_base = (struct cherry_session *)context;
	struct cherry_ccc_controller_session_report *controller_report =
		(struct cherry_ccc_controller_session_report *)params;
	struct cherry_ccc_session *session;
	struct cherry_ccc_event *event;

	if (abort)
		goto free_controller_report;

	session = cherry_ccc_get_child(session_base);
	if (session == NULL)
		goto free_controller_report;

	event = cherry_ccc_alloc_event_report_controller(controller_report,
							 session);
	if (!event)
		goto free_controller_report;

	session->ccc_cb(event, session_base->user_data);

	return;

free_controller_report:
	cherry_uci_client_ccc_free_data_controller_report(controller_report);
}

static void cherry_thread_ccc_uci_client_report_controlee_ntf_cb(
	const void *context, const void *params, bool abort)
{
	struct cherry_session *session_base = (struct cherry_session *)context;
	struct cherry_ccc_controlee_session_report *controlee_report =
		(struct cherry_ccc_controlee_session_report *)params;
	struct cherry_ccc_session *session;
	struct cherry_ccc_event *event;

	if (abort)
		goto free_controlee_report;

	session = cherry_ccc_get_child(session_base);
	if (session == NULL)
		goto free_controlee_report;

	event = cherry_ccc_alloc_event_report_controlee(controlee_report,
							session);
	if (!event)
		goto free_controlee_report;

	session->ccc_cb(event, session_base->user_data);

	return;

free_controlee_report:
	cherry_uci_client_ccc_free_data_controlee_report(controlee_report);
}

void cherry_ccc_controller_ranging_handler(
	struct cherry_session *session_base,
	const struct session_ranging_data *data)
{
	struct cherry *ctx = (struct cherry *)data->user_data;
	struct cherry_ccc_controller_session_report *controller_report;

	controller_report =
		(struct cherry_ccc_controller_session_report *)qmalloc(
			sizeof(struct cherry_ccc_controller_session_report));
	if (!controller_report)
		return;

	controller_report->measurements = NULL;

	if (cherry_uci_client_parse_ccc_controller_measurements(
		    data, controller_report) != QERR_SUCCESS)
		goto free_controller_report;

	if (cherry_thread_send_prio_task(
		    &ctx->thread_ctx, session_base, controller_report, 0,
		    cherry_thread_ccc_uci_client_report_controller_ntf_cb) ==
	    CHERRY_ERR_NONE)
		return;

free_controller_report:
	cherry_uci_client_ccc_free_data_controller_report(controller_report);
}

void cherry_ccc_controlee_ranging_handler(
	struct cherry_session *session_base,
	const struct session_ranging_data *data)
{
	struct cherry *ctx = (struct cherry *)data->user_data;
	struct cherry_ccc_controlee_session_report *controlee_report;

	controlee_report =
		(struct cherry_ccc_controlee_session_report *)qmalloc(
			sizeof(struct cherry_ccc_controlee_session_report));
	if (!controlee_report)
		return;

	controlee_report->measurements = NULL;

	if (cherry_uci_client_parse_ccc_controlee_measurements(
		    data, controlee_report) != QERR_SUCCESS)
		goto free_controlee_report;

	if (cherry_thread_send_prio_task(
		    &ctx->thread_ctx, session_base, controlee_report, 0,
		    cherry_thread_ccc_uci_client_report_controlee_ntf_cb) ==
	    CHERRY_ERR_NONE)
		return;

free_controlee_report:
	cherry_uci_client_ccc_free_data_controlee_report(controlee_report);
}

static void
cherry_ccc_child_prepare_force_stop(const struct cherry_session *session_base,
				    struct session_force_stop *force_stop)
{
	struct cherry_ccc_session *session = cherry_ccc_get_child(session_base);
	struct cherry_ccc_event *event;

	event = cherry_ccc_alloc_event_status();
	if (!event)
		return;

	event->session = session;
	event->data.status->session_state = CHERRY_CCC_SESSION_STATE_DEINIT;
	event->data.status->reason_code =
		CHERRY_CCC_STATE_CHANGE_REASON_FORCE_STOPPED;

	force_stop->event = (void *)event;
	force_stop->cb = (child_cherry_cb_t)session->ccc_cb;

	return;
}

static void
cherry_ccc_child_error_event(const struct cherry_session *session_base,
			     enum cherry_err status_err)
{
	struct cherry_ccc_session *session = cherry_ccc_get_child(session_base);

	cherry_ccc_send_error_event(session, status_err);
}

static void cherry_ccc_child_free(const struct cherry_session *session_base)
{
	struct cherry_ccc_session *session = cherry_ccc_get_child(session_base);

	cherry_ccc_free(session);
}

static void
cherry_ccc_child_status_event(const struct cherry_session *session_base,
			      struct session_status_ntf *ntf)
{
	struct cherry_ccc_session *session;
	struct cherry_ccc_event *event;

	session = cherry_ccc_get_child(session_base);
	if (session == NULL)
		return;

	event = cherry_ccc_alloc_event_status();
	if (!event)
		return;

	event->session = session;
	event->data.status->session_state = (enum cherry_ccc_session_state)
		session_status_ntf_get_session_state(ntf);
	event->data.status->reason_code =
		convert_uci_session_reason_code_to_cherry_ccc(
			session_status_ntf_get_reason_code(ntf));

	session->ccc_cb(event, session_base->user_data);
}

static int cherry_ccc_uci_client_setup(struct cherry *cherry_ctx)
{
	return 0;
}

static void cherry_thread_task_create_aliro_session(const void *context,
						    const void *params,
						    bool abort)
{
	struct cherry_session *session_base = (struct cherry_session *)context;
	struct cherry_ccc_session *session = cherry_ccc_get_child(session_base);
	uint32_t session_id = *(uint32_t *)params;

	if (abort)
		return;

	/* Register the session to the cherry core. */
	cherry_register_session(session_base->cherry_ctx, session_base);

	if (!session_base->cherry_ctx->uci_transport_ctx.transport) {
		QLOGE("%s: no transport, task can not be sent.", __func__);
		cherry_ccc_update_error_state(session, CHERRY_ERR_INTERNAL);
		return;
	}

	cherry_session_thread_init(session_base, session_id,
				   UCI_SESSION_TYPE_ALIRO);
}

static struct cherry_ccc_session *
cherry_ccc_session_create(struct cherry *cherry_ctx, cherry_ccc_cb_t callback,
			  void *user_data)
{
	struct cherry_session_child_cb child_cb;
	enum uci_status_code r;
	struct cherry_ccc_session *session;

	session = (struct cherry_ccc_session *)qcalloc(
		1, sizeof(struct cherry_ccc_session));
	if (!session)
		return NULL;

	session->ccc_cb = callback;

	/* Initialize the session base. */
	child_cb.prepare_force_stop = cherry_ccc_child_prepare_force_stop;
	child_cb.error_event = cherry_ccc_child_error_event;
	child_cb.free = cherry_ccc_child_free;
	child_cb.status_event = cherry_ccc_child_status_event;

	cherry_session_init(&session->session_base, CHERRY_SESSION_TYPE_CCC,
			    cherry_ctx, user_data, &child_cb);

	r = cherry_ccc_uci_client_setup(session->session_base.cherry_ctx);
	if (r) {
		QLOGE("%s: client setup failed with error %d.", __func__, r);
		cherry_ccc_free(session);
		return NULL;
	}

	return session;
}

static struct cherry_ccc_session *
cherry_ccc_session_create_aliro(struct cherry *ctx, cherry_ccc_cb_t callback,
				void *user_data,
				const struct cherry_ccc_aliro_params *params)
{
	struct cherry_ccc_session *session;
	struct cherry_session *session_base;
	enum uci_status_code r;

	if (!ctx || !callback)
		return NULL;

	if (!cherry_state_is_valid(ctx)) {
		QLOGE("%s: internal state error.", __func__);
		return NULL;
	}

	session = cherry_ccc_session_create(ctx, callback, user_data);
	if (!session)
		return NULL;

	session_base = &session->session_base;

	session_base->set_app_config_cmd =
		cherry_uci_client_session_set_app_config_cmd_create(
			session_base->cherry_ctx->session_client_ctx);
	if (!session_base->set_app_config_cmd) {
		QLOGE("%s: failed to create ccc set_app_config_cmd", __func__);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_device_type(
		session_base->set_app_config_cmd,
		params->is_controller ? UCI_DEVICE_TYPE_CCC_CONTROLLER :
					UCI_DEVICE_TYPE_CCC_CONTROLEE);
	if (r) {
		QLOGE("%s: error setting ccc device type with error %d.",
		      __func__, r);
		goto error;
	}

	r = cherry_uci_client_session_set_app_config_cmd_put_uwb_config_id(
		session_base->set_app_config_cmd,
		params->config->uwb_config_id);
	if (r) {
		QLOGE("%s: error setting ccc uwb config id with error %d.",
		      __func__, r);
		goto error;
	}

	r = cherry_uci_client_session_set_app_config_cmd_put_pulse_shape_combo(
		session_base->set_app_config_cmd,
		params->config->pulse_shape_combo);
	if (r) {
		QLOGE("%s: error setting ccc pulse shape combo with error %d.",
		      __func__, r);
		goto error;
	}

	r = cherry_uci_client_session_set_app_config_cmd_put_channel_number(
		session_base->set_app_config_cmd, params->config->channel);
	if (r) {
		QLOGE("%s: error setting ccc channel number with error %d.",
		      __func__, r);
		goto error;
	}

	r = cherry_uci_client_session_set_app_config_cmd_put_sync_code_index(
		session_base->set_app_config_cmd,
		params->config->sync_code_index);
	if (r) {
		QLOGE("%s: error setting ccc sync code index with error %d.",
		      __func__, r);
		goto error;
	}

	r = cherry_uci_client_session_set_app_config_cmd_put_interval_ms(
		session_base->set_app_config_cmd,
		params->config->ranging_duration_ms);
	if (r) {
		QLOGE("%s: error setting ccc ranging duration ms with error %d.",
		      __func__, r);
		goto error;
	}

	r = cherry_uci_client_session_set_app_config_cmd_put_slots_per_ranging_round(
		session_base->set_app_config_cmd, params->config->slot_per_rr);
	if (r) {
		QLOGE("%s: error setting ccc slots per rr with error %d.",
		      __func__, r);
		goto error;
	}

	r = cherry_uci_client_session_set_app_config_cmd_put_slot_duration_rstu(
		session_base->set_app_config_cmd,
		params->config->slot_duration);
	if (r) {
		QLOGE("%s: error setting ccc slot duration with error %d.",
		      __func__, r);
		goto error;
	}

	r = cherry_uci_client_session_set_app_config_cmd_put_hopping_mode(
		session_base->set_app_config_cmd, params->config->hopping_mode);
	if (r) {
		QLOGE("%s: error setting ccc hopping mode with error %d.",
		      __func__, r);
		goto error;
	}

	r = cherry_uci_client_session_set_app_config_cmd_put_sts_index0(
		session_base->set_app_config_cmd, params->config->sts_index);
	if (r) {
		QLOGE("%s: error setting ccc sts index with error %d.",
		      __func__, r);
		goto error;
	}

	r = cherry_uci_client_session_set_app_config_cmd_put_mac_mode(
		session_base->set_app_config_cmd, params->config->mac_mode);
	if (r) {
		QLOGE("%s: error setting ccc mac mode with error %d.", __func__,
		      r);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_initiation_time_us(
		session_base->set_app_config_cmd, params->config->uwb_time_us);
	if (r) {
		QLOGE("%s: error setting CCC UWB initiation with error %d.",
		      __func__, r);
		goto error;
	}

	r = cherry_uci_client_session_set_app_config_cmd_put_no_of_controlees(
		session_base->set_app_config_cmd, params->n_responder);
	if (r) {
		QLOGE("%s: error setting ccc nb responder with error %d.",
		      __func__, r);
		goto error;
	}

	r = cherry_uci_client_session_set_app_config_cmd_put_info_ntf_config(
		session_base->set_app_config_cmd, 1);
	if (r) {
		QLOGE("%s: error setting ccc ntf config with error %d.",
		      __func__, r);
		goto error;
	}

	r = cherry_uci_client_session_set_app_config_cmd_put_responder_slot_index(
		session_base->set_app_config_cmd, 0);
	if (r) {
		QLOGE("%s: error setting ccc responder slot index with error %d.",
		      __func__, r);
		goto error;
	}

	if (cherry_ccc_send_list_task(&session->session_base,
				      &params->config->session_id,
				      sizeof(params->config->session_id),
				      cherry_thread_task_create_aliro_session,
				      false) != CHERRY_ERR_NONE) {
		QLOGE("%s: can not create the session.", __func__);
		goto error;
	}
	return session;

error:
	cherry_ccc_free(session);
	return NULL;
}

struct cherry_ccc_session *cherry_ccc_session_create_aliro_initiator(
	struct cherry *ctx, cherry_ccc_cb_t callback, void *user_data,
	struct cherry_ccc_aliro_session_config *config, uint8_t n_responder)
{
	struct cherry_ccc_aliro_params params;

	params.is_controller = true;
	params.config = config;
	params.n_responder = n_responder;

	return cherry_ccc_session_create_aliro(ctx, callback, user_data,
					       &params);
}

struct cherry_ccc_session *cherry_ccc_session_create_aliro_responder(
	struct cherry *ctx, cherry_ccc_cb_t callback, void *user_data,
	struct cherry_ccc_aliro_session_config *config)
{
	struct cherry_ccc_aliro_params params;

	params.is_controller = false;
	params.config = config;
	params.n_responder = 1;

	return cherry_ccc_session_create_aliro(ctx, callback, user_data,
					       &params);
}

void cherry_ccc_event_free(struct cherry_ccc_event *event)
{
	if (!event)
		return;

	switch (event->type) {
	case CHERRY_CCC_EVENT_TYPE_SESSION_STATUS:
		if (event->data.status)
			qfree(event->data.status);
		break;
	case CHERRY_CCC_EVENT_TYPE_SESSION_ERROR:
		if (event->data.error)
			qfree(event->data.error);
		break;
	case CHERRY_CCC_EVENT_TYPE_SESSION_CONTROLLER_REPORT:
		if (event->data.controller_report)
			cherry_uci_client_ccc_free_data_controller_report(
				event->data.controller_report);
		break;
	case CHERRY_CCC_EVENT_TYPE_SESSION_CONTROLEE_REPORT:
		if (event->data.controlee_report)
			cherry_uci_client_ccc_free_data_controlee_report(
				event->data.controlee_report);
	default:
		break;
	}

	qfree(event);
}

enum cherry_err cherry_ccc_session_set_ursk(struct cherry_ccc_session *session,
					    const uint8_t *ursk)
{
	if (!ursk) {
		QLOGE("%s: URSK is invalid.", __func__);
		return CHERRY_ERR_INVALID_PARAMETER;
	}

	CHERRY_SESSION_SET_PARAM(
		&session->session_base,
		cherry_uci_client_session_set_app_config_cmd_put_session_key(
			cmd, ursk, 32));
}

enum cherry_err
cherry_ccc_session_set_round2_antennas(struct cherry_ccc_session *session,
				       uint8_t tx_antenna_set,
				       uint8_t rx_antenna_set)
{
	/* Set round 2 for rx and tx antennas. */
	/* clang-format off */
	CHERRY_SESSION_SET_PARAMS(
		&session->session_base,
		(cherry_uci_client_session_set_app_config_cmd_put_round2_tx_antenna_selection(cmd, tx_antenna_set))
		(cherry_uci_client_session_set_app_config_cmd_put_round2_rx_antenna_selection(cmd, rx_antenna_set))
	);
	/* clang-format on */
}
