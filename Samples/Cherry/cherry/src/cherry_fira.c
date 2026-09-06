/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "cherry_log.h"
#include "cherry_priv.h"
#include "cherry_session_manager.h"

#include <cherry/cherry_fira.h>
#include <cherry_fira_client.h>
#include <cherry_session_client.h>
#include <qassert.h>
#include <qmalloc.h>
#include <qsemaphore.h>
#include <qtime.h>

_Static_assert(CHERRY_STATIC_STS_SIZE == STATIC_STS_IV_SIZE,
	       "inconsistent values");

_Static_assert(CHERRY_VENDOR_ID_SIZE == VENDOR_ID_SIZE, "inconsistent values");

#define TIME_SCHEDULED_RANGING 0X01

/**
 * struct cherry_fira_session - Cherry fira session structure.
 * @fira_cb: The FiRa callback used to send an event to the applicative.
 * @session_base: The cherry_session will be used by the Cherry core.
*/
struct cherry_fira_session {
	cherry_fira_cb_t fira_cb;
	struct cherry_session session_base;
};

/**
 * struct cherry_fira_twr_params - Cherry twr params structure.
 * @session_id: User provided session_id for this session.
 * @is_controller: Boolean to indicate if the session is controller or controlee.
 * @short_addr: UWB device address.
 * @address: Controlees' adressses.
 * @interval_ms: Interval between ranging, in milliseconds.
 */
struct cherry_fira_twr_params {
	uint32_t session_id;
	bool is_controller;
	uint16_t short_addr;
	struct dst_mac_addresses address;
	int interval_ms;
};

struct active_rounds {
	int n_rounds;
	uint8_t round_indexes[FIRA_DT_TAG_MAX_ACTIVE_RR];
};

/**
 * struct cherry_fira_twr_params - Cherry dltdoa params structure.
 * @session_id: User provided session_id for this session.
 * @block_skipping: Number of block to be skipped between two active ranging
 *                  blocks.
 * @active_rounds: Table of active round indexes on tag.
 * @anchor_n_rounds: Number of rounds in which the device is participating.
 * @rounds_config: Round configuration table, should contain %anchor_n_rounds.
 */
struct cherry_fira_dt_params {
	uint32_t session_id;
	uint8_t block_skipping;
	struct active_rounds active_rounds;
	int anchor_n_rounds;
	struct cherry_fira_anchor_round_config rounds_config[];
};

static struct cherry_fira_event *cherry_fira_alloc_event(void)
{
	struct cherry_fira_event *event;

	event = qmalloc(sizeof(struct cherry_fira_event));
	if (!event) {
		QLOGE("%s: Unable to allocate event.", __func__);
	}
	return event;
}

static struct cherry_fira_event *cherry_fira_alloc_event_status(void)
{
	struct cherry_fira_event *event = cherry_fira_alloc_event();
	if (!event) {
		return NULL;
	}

	event->type = CHERRY_FIRA_EVENT_TYPE_SESSION_STATUS;
	event->data.status =
		(struct cherry_fira_session_event_session_status *)qmalloc(
			sizeof(struct cherry_fira_session_event_session_status));
	if (event->data.status)
		return event;

	QLOGE("%s: Unable to allocate event.", __func__);

	qfree(event);
	return NULL;
}

static struct cherry_fira_event *cherry_fira_alloc_event_error(void)
{
	struct cherry_fira_event *event = cherry_fira_alloc_event();
	if (!event) {
		return NULL;
	}

	event->type = CHERRY_FIRA_EVENT_TYPE_SESSION_ERROR;
	event->data.error = (struct cherry_fira_session_event_error *)qmalloc(
		sizeof(struct cherry_fira_session_event_error));
	if (event->data.error)
		return event;

	QLOGE("%s: Unable to allocate event.", __func__);

	qfree(event);
	return NULL;
}

static struct cherry_fira_event *
cherry_fira_alloc_event_diag_report(uint32_t nb_reports)
{
	struct cherry_fira_event *event = cherry_fira_alloc_event();
	if (!event) {
		return NULL;
	}

	event->type = CHERRY_FIRA_EVENT_TYPE_SESSION_DIAGNOSTIC_REPORT;
	event->data.diagnostics = (struct cherry_common_diag_report *)qcalloc(
		1,
		sizeof(struct cherry_common_diag_report) +
			nb_reports * sizeof(struct cherry_common_diag_frame));
	if (event->data.diagnostics)
		return event;

	QLOGE("%s: Unable to allocate event.", __func__);

	qfree(event);
	return NULL;
}

static struct cherry_fira_event *
cherry_fira_alloc_event_twr_ranging_report(int n_measurements)
{
	struct cherry_fira_event *event = cherry_fira_alloc_event();
	if (!event) {
		return NULL;
	}

	event->type = CHERRY_FIRA_EVENT_TYPE_SESSION_TWR_RANGING_REPORT;
	event->data.twr_ranging =
		(struct cherry_fira_session_twr_ranging_report *)qmalloc(
			sizeof(struct cherry_fira_session_twr_ranging_report) +
			n_measurements *
				sizeof(struct cherry_fira_session_twr_measurements));
	if (event->data.twr_ranging)
		return event;

	QLOGE("%s: Unable to allocate event.", __func__);

	qfree(event);
	return NULL;
}

static struct cherry_fira_event *
cherry_fira_alloc_event_dt_tag_ranging_report(int n_measurements)
{
	struct cherry_fira_event *event = cherry_fira_alloc_event();
	if (!event) {
		return NULL;
	}

	event->type = CHERRY_FIRA_EVENT_TYPE_SESSION_DT_TAG_RANGING_REPORT;
	event->data.dt_tag_ranging =
		(struct cherry_fira_session_dt_tag_ranging_report *)qcalloc(
			1,
			sizeof(struct cherry_fira_session_dt_tag_ranging_report) +
				n_measurements *
					sizeof(struct cherry_fira_session_dt_tag_measurements));
	if (event->data.dt_tag_ranging)
		return event;

	QLOGE("%s: Unable to allocate event.", __func__);

	qfree(event);
	return NULL;
}

static void cherry_fira_send_error_event(struct cherry_fira_session *ctx,
					 enum cherry_err status_err)
{
	struct cherry_fira_event *event;

	event = cherry_fira_alloc_event_error();
	if (!event)
		return;

	event->data.error->session = ctx;
	event->data.error->status_err = status_err;
	ctx->fira_cb(event, ctx->session_base.user_data);
}

struct cherry_session *
cherry_fira_session_to_base(struct cherry_fira_session *session)
{
	if (session)
		return &session->session_base;
	return NULL;
}

static struct cherry_fira_session *
cherry_fira_get_child(const struct cherry_session *session_base)
{
	return qparent_of(session_base, struct cherry_fira_session,
			  session_base);
}

static enum cherry_err
cherry_fira_send_list_task(const struct cherry_session *session_base,
			   const void *params, size_t params_size,
			   cherry_thread_task_t thread_task, bool destroy)
{
	return cherry_thread_send_list_task(
		&session_base->cherry_ctx->thread_ctx, session_base, params,
		params_size, thread_task, destroy);
}

static void cherry_fira_update_error_state(struct cherry_fira_session *ctx,
					   enum cherry_err status_err)
{
	/*
	 * Indicate the core we have to go in Error state
	 * because of communication lost.
	 */
	if (status_err == CHERRY_ERR_UWBS_TIMEOUT)
		cherry_set_error_state(ctx->session_base.cherry_ctx);

	cherry_fira_send_error_event(ctx, status_err);
}

static void cherry_fira_free(struct cherry_fira_session *session)
{
	qfree(session);
}

void cherry_fira_close(struct cherry *cherry_ctx)
{
	if (cherry_ctx->fira_client_ctx)
		cherry_uci_client_fira_close(cherry_ctx->fira_client_ctx);
}

static enum cherry_fira_state_change_reason
convert_uci_session_reason_code_to_cherry_fira(enum uci_session_reason_code code)
{
	switch (code) {
	case UCI_SESSION_REASON_STATE_CHANGE_WITH_SESSION_MANAGEMENT_COMMANDS:
		return CHERRY_FIRA_STATE_CHANGE_REASON_MGMT_CMD;
	case UCI_SESSION_REASON_MAX_RANGING_ROUND_RETRY_COUNT_REACHED:
		return CHERRY_FIRA_STATE_CHANGE_REASON_MAX_RETRY;
	case UCI_SESSION_REASON_MAX_NUMBER_OF_MEASUREMENTS_REACHED:
		return CHERRY_FIRA_STATE_CHANGE_REASON_MAX_MEASUREMENT;
	default:
		break;
	}
	return CHERRY_FIRA_STATE_CHANGE_REASON_UNKNOWN;
}

static void
cherry_fira_twr_ranging_results_free(struct twr_ranging_results *twr_results)
{
	if (twr_results)
		qfree(twr_results);
}
static void cherry_thread_fira_uci_client_dltdoa_ntf_cb(const void *context,
							const void *params,
							bool abort)
{
	struct cherry_session *session_base = (struct cherry_session *)context;
	struct cherry_fira_event *event = (struct cherry_fira_event *)params;

	if (abort) {
		cherry_fira_event_free(event);
		return;
	}

	/* Directly call callback with already parsed data. */
	event->data.dt_tag_ranging->session->fira_cb(event,
						     session_base->user_data);
}

static void cherry_thread_fira_uci_client_twr_ntf_cb(const void *context,
						     const void *params,
						     bool abort)
{
	struct cherry_session *session_base = (struct cherry_session *)context;
	struct twr_ranging_results *results =
		(struct twr_ranging_results *)params;
	struct cherry_fira_session *session;
	struct cherry_fira_event *event;

	if (abort)
		goto free_twr_results;

	session = cherry_fira_get_child(session_base);
	if (session == NULL)
		goto free_twr_results;

	event = cherry_fira_alloc_event_twr_ranging_report(
		results->n_measurements);
	if (!event)
		goto free_twr_results;

	event->data.twr_ranging->session = session;
	event->data.twr_ranging->sequence_number = results->sequence_number;
	event->data.twr_ranging->n_measurements = results->n_measurements;
	for (int i = 0; i < results->n_measurements; i++) {
		const struct twr_ranging_measurements *cur_meas =
			&results->measurements[i];
		event->data.twr_ranging->measurements[i].short_addr =
			cur_meas->short_addr;
		event->data.twr_ranging->measurements[i].frame_status =
			cherry_session_frame_status_to_cherry_format(
				cur_meas->status);
		event->data.twr_ranging->measurements[i].nlos = cur_meas->nlos;
		event->data.twr_ranging->measurements[i].slot_index =
			cur_meas->slot_index;
		event->data.twr_ranging->measurements[i].distance_mm =
			cur_meas->distance_mm *
			(cur_meas->status ==
					 FIRA_STATUS_OK_NEGATIVE_DISTANCE_REPORT ?
				 -1 :
				 1);
		event->data.twr_ranging->measurements[i]
			.aoa[CHERRY_AOA_AZIMUTH]
			.aoa = cur_meas->local_aoa_measurements
				       [CHERRY_FIRA_CLIENT_AOA_AZIMUTH]
					       .aoa;
		event->data.twr_ranging->measurements[i]
			.aoa[CHERRY_AOA_AZIMUTH]
			.aoa_fom = cur_meas->local_aoa_measurements
					   [CHERRY_FIRA_CLIENT_AOA_AZIMUTH]
						   .aoa_fom;
		event->data.twr_ranging->measurements[i]
			.aoa[CHERRY_AOA_ELEVATION]
			.aoa = cur_meas->local_aoa_measurements
				       [CHERRY_FIRA_CLIENT_AOA_ELEVATION]
					       .aoa;
		event->data.twr_ranging->measurements[i]
			.aoa[CHERRY_AOA_ELEVATION]
			.aoa_fom = cur_meas->local_aoa_measurements
					   [CHERRY_FIRA_CLIENT_AOA_ELEVATION]
						   .aoa_fom;

		event->data.twr_ranging->measurements[i]
			.remote_aoa[CHERRY_AOA_AZIMUTH]
			.aoa = cur_meas->remote_aoa_azimuth;
		event->data.twr_ranging->measurements[i]
			.remote_aoa[CHERRY_AOA_AZIMUTH]
			.aoa_fom = cur_meas->remote_aoa_azimuth_fom;
		event->data.twr_ranging->measurements[i]
			.remote_aoa[CHERRY_AOA_ELEVATION]
			.aoa = cur_meas->remote_aoa_elevation;
		event->data.twr_ranging->measurements[i]
			.remote_aoa[CHERRY_AOA_ELEVATION]
			.aoa_fom = cur_meas->remote_aoa_elevation_fom;
		event->data.twr_ranging->measurements[i].rssi = cur_meas->rssi;
	}

	session->fira_cb(event, session->session_base.user_data);

free_twr_results:
	cherry_fira_twr_ranging_results_free(results);
}

void cherry_fira_twr_ranging_handler(struct cherry_session *session_base,
				     const struct session_ranging_data *data)
{
	struct cherry *ctx = (struct cherry *)data->user_data;
	struct twr_ranging_results *twr_results;

	twr_results = (struct twr_ranging_results *)qmalloc(
		sizeof(struct twr_ranging_results));
	if (!twr_results)
		return;

	if (cherry_uci_client_parse_twr_measurements(data, twr_results) !=
	    QERR_SUCCESS)
		goto free_twr_results;

	if (cherry_thread_send_prio_task(
		    &ctx->thread_ctx, session_base, twr_results, 0,
		    cherry_thread_fira_uci_client_twr_ntf_cb) ==
	    CHERRY_ERR_NONE)
		return;

free_twr_results:
	cherry_fira_twr_ranging_results_free(
		(struct twr_ranging_results *)twr_results);
}

void cherry_fira_dl_tdoa_ranging_handler(struct cherry_session *session_base,
					 const struct session_ranging_data *data)
{
	struct cherry *ctx = (struct cherry *)data->user_data;
	struct cherry_fira_event *event;
	struct cherry_fira_session *session;
	enum qerr parse_result;

	/* Set session before parsing (avoids needing dl_tdoa_results). */
	session = cherry_fira_get_child(session_base);
	if (session == NULL) {
		return;
	}

	/* Allocate event with all necessary measurements upfront. */
	event = cherry_fira_alloc_event_dt_tag_ranging_report(
		data->n_measurements);

	if (!event) {
		return;
	}

	event->data.dt_tag_ranging->session = session;

	if (data->type ==
	    FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_DL_TDOA) {
		parse_result = cherry_uci_client_parse_dltdoa_measurements(
			data, event->data.dt_tag_ranging);
	} else {
		parse_result = cherry_uci_client_parse_dltdoa_measurements_v2(
			data, event->data.dt_tag_ranging);
	}

	if (parse_result != QERR_SUCCESS) {
		cherry_fira_event_free(event);
		return;
	}

	/* Directly send the fully populated event. */
	if (cherry_thread_send_prio_task(
		    &ctx->thread_ctx, session_base, event, 0,
		    cherry_thread_fira_uci_client_dltdoa_ntf_cb) !=
	    CHERRY_ERR_NONE) {
		cherry_fira_event_free(event);
		return;
	}
}

static void cherry_thread_fira_uci_client_diag_ntf_cb(const void *context,
						      const void *params,
						      bool abort)
{
	struct cherry_session *session_base = (struct cherry_session *)context;
	struct diagnostic_info *diag = (struct diagnostic_info *)params;
	struct cherry_fira_session *session;
	struct cherry_fira_event *event;

	if (abort)
		goto free_diag;

	session = cherry_fira_get_child(session_base);
	if (session == NULL)
		goto free_diag;

	event = cherry_fira_alloc_event_diag_report(diag->nb_reports);
	if (!event)
		goto free_diag;

	event->data.diagnostics->session = session;
	event->data.diagnostics->sequence_number = diag->sequence_number;
	event->data.diagnostics->n_frame_report = diag->nb_reports;
	for (uint32_t i = 0; i < diag->nb_reports; i++) {
		const struct frame_report *cur_meas = &diag->reports[i];
		if (cur_meas->nb_seg_metrics) {
			event->data.diagnostics->frame_report[i].n_seg_metrics =
				cur_meas->nb_seg_metrics;
			event->data.diagnostics->frame_report[i].seg_metrics =
				(struct cherry_common_segment_metrics *)qmalloc(
					cur_meas->nb_seg_metrics *
					sizeof(struct cherry_common_segment_metrics));
			for (int seg_metrics_index = 0;
			     seg_metrics_index < cur_meas->nb_seg_metrics;
			     seg_metrics_index++) {
				const struct segment_metrics *cur_segment_metrics =
					&cur_meas->seg_metrics[seg_metrics_index];
				struct cherry_common_segment_metrics *metrics =
					&event->data.diagnostics
						 ->frame_report[i]
						 .seg_metrics[seg_metrics_index];
				metrics->noise_value =
					cur_segment_metrics->noise_value;
				metrics->rsl_q8 = cur_segment_metrics->rsl_q8;
				metrics->fp_index =
					cur_segment_metrics->fp_index;
				metrics->fp_rsl_q8 =
					cur_segment_metrics->fp_rsl_q8;
				metrics->fp_ns_q6 =
					cur_segment_metrics->fp_ns_q6;
				metrics->pp_index =
					cur_segment_metrics->pp_index;
				metrics->pp_rsl_q8 =
					cur_segment_metrics->pp_rsl_q8;
				metrics->pp_ns_q6 =
					cur_segment_metrics->pp_ns_q6;
				metrics->receiver_segment =
					cur_segment_metrics->receiver_segment;
			}
		}
		if (cur_meas->nb_aoa) {
			event->data.diagnostics->frame_report[i].n_aoa =
				cur_meas->nb_aoa;
			event->data.diagnostics->frame_report[i]
				.aoas = (struct cherry_common_aoa_measurement *)qmalloc(
				cur_meas->nb_aoa *
				sizeof(struct cherry_common_aoa_measurement));
			for (int aoa_index = 0; aoa_index < cur_meas->nb_aoa;
			     aoa_index++) {
				const struct aoa_measurement *cur_aoas =
					&cur_meas->aoas[aoa_index];
				struct cherry_common_aoa_measurement *aoas =
					&event->data.diagnostics
						 ->frame_report[i]
						 .aoas[aoa_index];
				aoas->tdoa = cur_aoas->tdoa;
				aoas->pdoa = cur_aoas->pdoa;
				aoas->aoa = cur_aoas->aoa;
				aoas->fom = cur_aoas->fom;
				aoas->type = cur_aoas->type;
			}
		}
		if (cur_meas->nb_cir) {
			event->data.diagnostics->frame_report[i].n_cir =
				cur_meas->nb_cir;
			event->data.diagnostics->frame_report[i].cirs =
				(struct cherry_common_cir *)qcalloc(
					cur_meas->nb_cir,
					sizeof(struct cherry_common_cir));
			for (int cir_index = 0; cir_index < cur_meas->nb_cir;
			     cir_index++) {
				const struct cir *cur_cir =
					&cur_meas->cirs[cir_index];
				struct cherry_common_cir *cir =
					&event->data.diagnostics
						 ->frame_report[i]
						 .cirs[cir_index];
				cir->receiver_segment =
					cur_cir->receiver_segment;
				cir->fpath_tap_offset =
					cur_cir->fpath_tap_offset;
				cir->tap_size = cur_cir->tap_size;
				cir->n_taps = cur_cir->n_taps;
				cir->taps =
					qmalloc(cir->tap_size * cir->n_taps *
						sizeof(uint8_t));
				memcpy(cir->taps, cur_cir->taps,
				       cir->tap_size * cir->n_taps *
					       sizeof(uint8_t));
			}
		}
		event->data.diagnostics->frame_report[i].extra_status_present =
			cur_meas->extra_status_present;
		event->data.diagnostics->frame_report[i].extra_status =
			cur_meas->extra_status;
		event->data.diagnostics->frame_report[i].cfo_present =
			cur_meas->cfo_present;
		event->data.diagnostics->frame_report[i].cfo_q26 =
			cur_meas->cfo_q26;
		event->data.diagnostics->frame_report[i]
			.emitter_short_addr_present =
			cur_meas->emitter_short_addr_present;
		event->data.diagnostics->frame_report[i].emitter_short_addr =
			cur_meas->emitter_short_addr;
		event->data.diagnostics->frame_report[i].msg_id =
			cur_meas->msg_id;
		event->data.diagnostics->frame_report[i].action =
			cur_meas->action;
		event->data.diagnostics->frame_report[i].antenna_set =
			cur_meas->antenna_set;
	}

	session->fira_cb(event, session->session_base.user_data);

free_diag:
	cherry_uci_client_fira_free_diag(diag);
}

static void fira_uci_client_diag_ntf_cb(struct diagnostic_info *diag,
					void *user_data)
{
	struct cherry *ctx = (struct cherry *)user_data;
	struct cherry_session *session_base;

	session_base = cherry_get_session_context(ctx, diag->session_handle);
	if (session_base == NULL)
		goto free_diag;

	if (cherry_thread_send_prio_task(
		    &ctx->thread_ctx, session_base, diag, 0,
		    cherry_thread_fira_uci_client_diag_ntf_cb) ==
	    CHERRY_ERR_NONE)
		return;

free_diag:
	cherry_uci_client_fira_free_diag(diag);
}

static void
cherry_fira_child_prepare_force_stop(const struct cherry_session *session_base,
				     struct session_force_stop *force_stop)
{
	struct cherry_fira_session *session =
		cherry_fira_get_child(session_base);
	struct cherry_fira_event *event;

	event = cherry_fira_alloc_event_status();
	if (!event)
		return;

	event->data.status->session = session;
	event->data.status->session_state = CHERRY_FIRA_SESSION_STATE_DEINIT;
	event->data.status->reason_code =
		CHERRY_FIRA_STATE_CHANGE_REASON_FORCE_STOPPED;

	force_stop->event = (void *)event;
	force_stop->cb = (child_cherry_cb_t)session->fira_cb;

	return;
}

static void
cherry_fira_child_error_event(const struct cherry_session *session_base,
			      enum cherry_err status_err)
{
	struct cherry_fira_session *session =
		cherry_fira_get_child(session_base);

	cherry_fira_send_error_event(session, status_err);
}

static void cherry_fira_child_free(const struct cherry_session *session_base)
{
	struct cherry_fira_session *session =
		cherry_fira_get_child(session_base);

	cherry_fira_free(session);
}

static void
cherry_fira_child_status_event(const struct cherry_session *session_base,
			       struct session_status_ntf *ntf)
{
	struct cherry_fira_session *session;
	struct cherry_fira_event *event;

	session = cherry_fira_get_child(session_base);
	if (session == NULL)
		return;

	event = cherry_fira_alloc_event_status();
	if (!event)
		return;

	event->data.status->session = session;
	event->data.status->session_state = (enum cherry_fira_session_state)
		session_status_ntf_get_session_state(ntf);
	event->data.status->reason_code =
		convert_uci_session_reason_code_to_cherry_fira(
			session_status_ntf_get_reason_code(ntf));

	session->fira_cb(event, session->session_base.user_data);
}

static int cherry_fira_uci_client_setup(struct cherry *cherry_ctx)
{
	int r = 0;

	if (cherry_ctx->fira_client_ctx == NULL) {
		r = cherry_uci_client_fira_open(&cherry_ctx->fira_client_ctx,
						&cherry_ctx->uci, cherry_ctx,
						fira_uci_client_diag_ntf_cb);
	}

	return r;
}

static void cherry_thread_task_create_twr_session(const void *context,
						  const void *params,
						  bool abort)
{
	struct cherry_session *session_base = (struct cherry_session *)context;
	struct cherry_fira_session *session =
		cherry_fira_get_child(session_base);
	struct cherry_fira_twr_params *twr_params =
		(struct cherry_fira_twr_params *)params;

	if (abort)
		return;

	/* Register the session to the cherry core. */
	cherry_register_session(session_base->cherry_ctx, session_base);

	if (!session_base->cherry_ctx->uci_transport_ctx.transport) {
		QLOGE("%s: no transport, task can not be sent.", __func__);
		cherry_fira_update_error_state(session, CHERRY_ERR_INTERNAL);
		return;
	}

	/* Initialize the session and set the session handle. */
	cherry_session_thread_init(session_base, twr_params->session_id,
				   UCI_SESSION_TYPE_RANGING);
}

static struct cherry_fira_session *
cherry_fira_session_create(struct cherry *cherry_ctx, cherry_fira_cb_t callback,
			   void *user_data)
{
	struct cherry_session_child_cb child_cb;
	enum uci_status_code r;
	struct cherry_fira_session *session;

	session = (struct cherry_fira_session *)qcalloc(
		1, sizeof(struct cherry_fira_session));
	if (!session)
		return NULL;

	session->fira_cb = callback;

	/* Initialize the session base. */
	child_cb.prepare_force_stop = cherry_fira_child_prepare_force_stop;
	child_cb.error_event = cherry_fira_child_error_event;
	child_cb.free = cherry_fira_child_free;
	child_cb.status_event = cherry_fira_child_status_event;

	cherry_session_init(&session->session_base, CHERRY_SESSION_TYPE_FIRA,
			    cherry_ctx, user_data, &child_cb);

	r = cherry_fira_uci_client_setup(session->session_base.cherry_ctx);
	if (r) {
		QLOGE("%s: client setup failed with error %d.", __func__, r);
		cherry_fira_free(session);
		return NULL;
	}

	return session;
}

static struct cherry_fira_session *
cherry_fira_session_create_twr(struct cherry *ctx, cherry_fira_cb_t callback,
			       void *user_data,
			       const struct cherry_fira_twr_params *params)
{
	struct cherry_fira_session *session;
	struct cherry_session *session_base;
	enum uci_status_code r;

	if (!ctx || !callback)
		return NULL;

	if (!cherry_state_is_valid(ctx)) {
		QLOGE("%s: internal state error.", __func__);
		return NULL;
	}

	session = cherry_fira_session_create(ctx, callback, user_data);
	if (!session)
		return NULL;

	session_base = &session->session_base;

	session_base->set_app_config_cmd =
		cherry_uci_client_session_set_app_config_cmd_create(
			session_base->cherry_ctx->session_client_ctx);
	if (!session_base->set_app_config_cmd) {
		QLOGE("%s: failed to create fira set_app_config_cmd", __func__);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_device_type(
		session_base->set_app_config_cmd,
		params->is_controller ? UCI_DEVICE_TYPE_CONTROLLER :
					UCI_DEVICE_TYPE_CONTROLEE);
	if (r) {
		QLOGE("%s: error setting fira device type with error %d.",
		      __func__, r);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_device_role(
		session_base->set_app_config_cmd,
		params->is_controller ? UCI_DEVICE_ROLE_INITIATOR :
					UCI_DEVICE_ROLE_RESPONDER);
	if (r) {
		QLOGE("%s: error setting fira device role with error %d.",
		      __func__, r);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_device_mac_address(
		session_base->set_app_config_cmd, params->short_addr);
	if (r) {
		QLOGE("%s: error setting fira device mac address with error %d.",
		      __func__, r);
		goto error;
	}
	/* Set multi node mode to one to many. */
	r = cherry_uci_client_session_set_app_config_cmd_put_multi_node_mode(
		session_base->set_app_config_cmd, 1);
	if (r) {
		QLOGE("%s: error setting fira session multi node mode with error %d.",
		      __func__, r);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_ranging_round_usage(
		session_base->set_app_config_cmd, UCI_DSTWR_DEFERRED);
	if (r) {
		QLOGE("%s: error setting fira session ranging round usage with error %d.",
		      __func__, r);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_schedule_mode(
		session_base->set_app_config_cmd, TIME_SCHEDULED_RANGING);
	if (r) {
		QLOGE("%s: error setting fira session schedule mode with error %d.",
		      __func__, r);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_dest_mac_address(
		session_base->set_app_config_cmd, &params->address);
	if (r) {
		QLOGE("%s: error setting fira session destination address with error %d.",
		      __func__, r);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_interval_ms(
		session_base->set_app_config_cmd, params->interval_ms);
	if (r) {
		QLOGE("%s: error setting fira interval ms with error %d.",
		      __func__, r);
		goto error;
	}

	if (cherry_fira_send_list_task(&session->session_base, params,
				       sizeof(struct cherry_fira_twr_params),
				       cherry_thread_task_create_twr_session,
				       false) != CHERRY_ERR_NONE) {
		QLOGE("%s: can not create the session.", __func__);
		goto error;
	}
	return session;

error:
	cherry_fira_free(session);
	return NULL;
}

struct cherry_fira_session *cherry_fira_session_create_twr_controller(
	struct cherry *ctx, cherry_fira_cb_t callback, void *user_data,
	uint32_t session_id, int interval_ms,
	const uint16_t *controlee_addresses, int n_controlee_addresses,
	uint16_t short_addr)
{
	struct cherry_fira_twr_params params;

	params.session_id = session_id;
	params.is_controller = true;
	params.short_addr = short_addr;
	params.address.n_addresses = n_controlee_addresses;
	params.interval_ms = interval_ms;
	for (int i = 0; i < n_controlee_addresses; i++) {
		params.address.addresses[i] = *controlee_addresses;
		controlee_addresses++;
	}

	return cherry_fira_session_create_twr(ctx, callback, user_data,
					      &params);
}

struct cherry_fira_session *cherry_fira_session_create_twr_controlee(
	struct cherry *ctx, cherry_fira_cb_t callback, void *user_data,
	uint32_t session_id, int interval_ms, uint16_t controller_address,
	uint16_t short_addr)
{
	struct cherry_fira_twr_params params;

	params.session_id = session_id;
	params.is_controller = false;
	params.short_addr = short_addr;
	params.address.n_addresses = 1;
	params.address.addresses[0] = controller_address;
	params.interval_ms = interval_ms;

	return cherry_fira_session_create_twr(ctx, callback, user_data,
					      &params);
}

static void cherry_thread_task_create_dt_anchor(const void *context,
						const void *params, bool abort)
{
	struct cherry_session *session_base = (struct cherry_session *)context;
	struct cherry_fira_session *session =
		cherry_fira_get_child(session_base);
	struct cherry_fira_dt_params *dt_params =
		(struct cherry_fira_dt_params *)params;
	int r;
	uint8_t *round_indexes;
	uint8_t *ranging_role;
	uint8_t *number_of_responders;
	uint16_t(*responder_address_list)[8];
	uint8_t *responder_slot_scheduling;
	uint8_t dl_tdoa_update_ranging_round_array
		[DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE];
	uint8_t dl_tdoa_update_ranging_round_array_size = 0;

	if (abort)
		return;

	/* Register the session to the cherry core. */
	cherry_register_session(session_base->cherry_ctx, session_base);

	if (!session_base->cherry_ctx->uci_transport_ctx.transport) {
		QLOGE("%s: no transport, task can not be sent.", __func__);
		cherry_fira_update_error_state(session, CHERRY_ERR_INTERNAL);
		return;
	}

	r = cherry_session_thread_init(session_base, dt_params->session_id,
				       UCI_SESSION_TYPE_RANGING);
	if (r != CHERRY_ERR_NONE)
		return;

	/* Updating ranging rounds. */
	round_indexes = qmalloc(dt_params->anchor_n_rounds * sizeof(uint8_t));
	if (!round_indexes) {
		QLOGE("%s: Unable to allocate array.", __func__);
		cherry_fira_update_error_state(session, CHERRY_ERR_INTERNAL);
		return;
	}
	ranging_role = qmalloc(dt_params->anchor_n_rounds * sizeof(uint8_t));
	if (!ranging_role) {
		qfree(round_indexes);
		QLOGE("%s: Unable to allocate array.", __func__);
		cherry_fira_update_error_state(session, CHERRY_ERR_INTERNAL);
		return;
	}
	number_of_responders =
		qmalloc(dt_params->anchor_n_rounds * sizeof(uint8_t));
	if (!number_of_responders) {
		qfree(round_indexes);
		qfree(ranging_role);
		QLOGE("%s: Unable to allocate array.", __func__);
		cherry_fira_update_error_state(session, CHERRY_ERR_INTERNAL);
		return;
	}
	responder_address_list =
		qmalloc(sizeof(uint16_t[dt_params->anchor_n_rounds][8]));
	if (!responder_address_list) {
		qfree(round_indexes);
		qfree(ranging_role);
		qfree(number_of_responders);
		QLOGE("%s: Unable to allocate array.", __func__);
		cherry_fira_update_error_state(session, CHERRY_ERR_INTERNAL);
		return;
	}
	responder_slot_scheduling =
		qmalloc(dt_params->anchor_n_rounds * sizeof(uint8_t));
	if (!responder_slot_scheduling) {
		qfree(round_indexes);
		qfree(ranging_role);
		qfree(number_of_responders);
		qfree(responder_address_list);
		QLOGE("%s: Unable to allocate array.", __func__);
		cherry_fira_update_error_state(session, CHERRY_ERR_INTERNAL);
		return;
	}
	for (int i = 0; i < dt_params->anchor_n_rounds; i++) {
		round_indexes[i] = dt_params->rounds_config[i].round_idx;
		ranging_role[i] = dt_params->rounds_config[i].role;
		number_of_responders[i] =
			dt_params->rounds_config[i].n_responders;
		for (int j = 0; j < dt_params->rounds_config[i].n_responders;
		     j++) {
			responder_address_list[i][j] =
				dt_params->rounds_config[i].responders_addr[j];
		}
		responder_slot_scheduling[i] = 0;
	}
	r = cherry_uci_client_fira_update_dt_anchor_ranging_rounds(
		session_base->cherry_ctx->fira_client_ctx,
		session_base->session_handle, dt_params->anchor_n_rounds,
		round_indexes, ranging_role, number_of_responders,
		responder_address_list, responder_slot_scheduling, NULL,
		dl_tdoa_update_ranging_round_array,
		&dl_tdoa_update_ranging_round_array_size);
	qfree(round_indexes);
	qfree(ranging_role);
	qfree(number_of_responders);
	qfree(responder_address_list);
	qfree(responder_slot_scheduling);
	if (r) {
		int index;
		for (index = 0; index < dl_tdoa_update_ranging_round_array_size;
		     index++)
			QLOGE("%s: error updating fira dl-tdoa ranging rounds number %d with error %d.",
			      __func__,
			      dl_tdoa_update_ranging_round_array[index], r);
		cherry_fira_update_error_state(session, cherry_get_error(r));
		return;
	}
}

struct cherry_fira_session *cherry_fira_session_create_dt_anchor(
	struct cherry *ctx, cherry_fira_cb_t callback, void *user_data,
	uint32_t session_id, uint32_t interval_ms, uint16_t slot_duration,
	uint16_t short_addr, int n_rounds,
	const struct cherry_fira_anchor_round_config *rounds_config,
	const struct cherry_fira_anchor_location *location,
	uint8_t time_reference_anchor)
{
	struct cherry_fira_session *session;
	struct cherry_session *session_base;
	struct cherry_fira_dt_params *params;
	size_t params_size;
	enum uci_status_code r;

	/* Validate n_rounds here as the value sent to UWBS is 8 bits based, not 32 bits */
	if (!ctx || !callback || n_rounds > 255)
		return NULL;

	if (!cherry_state_is_valid(ctx)) {
		QLOGE("%s: internal state error.", __func__);
		return NULL;
	}

	session = cherry_fira_session_create(ctx, callback, user_data);
	if (!session)
		return NULL;

	params_size = sizeof(struct cherry_fira_dt_params) +
		      n_rounds * sizeof(struct cherry_fira_anchor_round_config),

	params = (struct cherry_fira_dt_params *)qcalloc(1, params_size);

	if (!params) {
		QLOGE("%s: Unable to allocate params.", __func__);
		goto error;
	}

	session_base = &session->session_base;
	params->session_id = session_id;
	params->anchor_n_rounds = n_rounds;

	if (rounds_config && n_rounds)
		memcpy(params->rounds_config, rounds_config,
		       n_rounds *
			       sizeof(struct cherry_fira_anchor_round_config));

	session_base->set_app_config_cmd =
		cherry_uci_client_session_set_app_config_cmd_create(
			session_base->cherry_ctx->session_client_ctx);
	if (!session_base->set_app_config_cmd) {
		QLOGE("%s: failed to create fira set_app_config_cmd", __func__);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_device_role(
		session_base->set_app_config_cmd, UCI_DEVICE_ROLE_DT_ANCHOR);
	if (r) {
		QLOGE("%s: error setting fira device type with error %d.",
		      __func__, r);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_rframe_config(
		session_base->set_app_config_cmd, 0x01);
	if (r) {
		QLOGE("%s: error setting fira device rframe config with error %d.",
		      __func__, r);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_device_mac_address(
		session_base->set_app_config_cmd, short_addr);
	if (r) {
		QLOGE("%s: error setting fira device mac address with error %d.",
		      __func__, r);
		goto error;
	}
	/* Set multi node mode to one to many. */
	r = cherry_uci_client_session_set_app_config_cmd_put_multi_node_mode(
		session_base->set_app_config_cmd, 1);
	if (r) {
		QLOGE("%s: error setting fira session multi node mode with error %d.",
		      __func__, r);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_ranging_round_usage(
		session_base->set_app_config_cmd, UCI_OWR_DL_TDOA);
	if (r) {
		QLOGE("%s: error setting fira session ranging round usage with error %d.",
		      __func__, r);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_schedule_mode(
		session_base->set_app_config_cmd, TIME_SCHEDULED_RANGING);
	if (r) {
		QLOGE("%s: error setting fira session schedule mode with error %d.",
		      __func__, r);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_interval_ms(
		session_base->set_app_config_cmd, interval_ms);
	if (r) {
		QLOGE("%s: error setting fira interval ms with error %d.",
		      __func__, r);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_ranging_method(
		session_base->set_app_config_cmd, 0x01);
	if (r) {
		QLOGE("%s: error setting fira dl-tdoa ranging method with error %d.",
		      __func__, r);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_slot_duration_rstu(
		session_base->set_app_config_cmd, slot_duration);
	if (r) {
		QLOGE("%s: error setting fira slot duration with error %d.",
		      __func__, r);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_responder_tof(
		session_base->set_app_config_cmd, 1);
	if (r) {
		QLOGE("%s: error setting dl tdoa responder tof with error %d.",
		      __func__, r);
		goto error;
	}

	if (location) {
		r = cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_location(
			session_base->set_app_config_cmd, location);
		if (r) {
			QLOGE("%s: error setting fira dl-tdoa  nchor location with error %d.",
			      __func__, r);
			goto error;
		}
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_time_reference_anchor(
		session_base->set_app_config_cmd, time_reference_anchor);
	if (r) {
		QLOGE("%s: error setting time reference anchor with error %d.",
		      __func__, r);
		goto error;
	}

	if (cherry_fira_send_list_task(&session->session_base, params,
				       params_size,
				       cherry_thread_task_create_dt_anchor,
				       false) != CHERRY_ERR_NONE) {
		QLOGE("%s: can not create the session.", __func__);
		goto error;
	}
	qfree(params);
	return session;

error:
	qfree(params);
	cherry_fira_free(session);
	return NULL;
}

static void cherry_thread_task_create_dt_tag(const void *context,
					     const void *params, bool abort)
{
	struct cherry_session *session_base = (struct cherry_session *)context;
	struct cherry_fira_session *session =
		cherry_fira_get_child(session_base);
	struct cherry_fira_dt_params *dt_params =
		(struct cherry_fira_dt_params *)params;
	struct active_rounds *active_rounds = &dt_params->active_rounds;
	int r;
	uint8_t dl_tdoa_update_ranging_round_array
		[DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE];
	uint8_t dl_tdoa_update_ranging_round_array_size = 0;

	if (abort)
		return;

	/* Register the session to the cherry core. */
	cherry_register_session(session_base->cherry_ctx, session_base);

	if (!session_base->cherry_ctx->uci_transport_ctx.transport) {
		QLOGE("%s: no transport, task can not be sent.", __func__);
		cherry_fira_update_error_state(session, CHERRY_ERR_INTERNAL);
		return;
	}

	r = cherry_session_thread_init(session_base, dt_params->session_id,
				       UCI_SESSION_TYPE_RANGING);
	if (r != CHERRY_ERR_NONE)
		return;

	r = cherry_uci_client_fira_update_dt_tag_ranging_rounds(
		session_base->cherry_ctx->fira_client_ctx,
		session_base->session_handle, active_rounds->n_rounds,
		active_rounds->round_indexes,
		dl_tdoa_update_ranging_round_array,
		&dl_tdoa_update_ranging_round_array_size);
	if (r) {
		int index;
		for (index = 0; index < dl_tdoa_update_ranging_round_array_size;
		     index++)
			QLOGE("%s: error updating fira dl-tdoa ranging rounds number %d with error %d.",
			      __func__,
			      dl_tdoa_update_ranging_round_array[index], r);
		cherry_fira_update_error_state(session, cherry_get_error(r));
		return;
	}

	return;
}

struct cherry_fira_session *cherry_fira_session_create_dt_tag(
	struct cherry *ctx, cherry_fira_cb_t callback, void *user_data,
	uint32_t session_id, uint32_t interval_ms, uint16_t slot_duration,
	uint8_t block_skipping, int n_rounds, const uint8_t *round_indexes)
{
	struct cherry_fira_session *session;
	struct cherry_session *session_base;
	struct cherry_fira_dt_params params;
	enum uci_status_code r;

	if (!ctx || !callback)
		return NULL;

	if (!cherry_state_is_valid(ctx)) {
		QLOGE("%s: internal state error.", __func__);
		return NULL;
	}

	session = cherry_fira_session_create(ctx, callback, user_data);
	if (!session)
		return NULL;

	session_base = &session->session_base;
	params.session_id = session_id;
	params.active_rounds.n_rounds = n_rounds;
	params.block_skipping = block_skipping;
	memcpy(params.active_rounds.round_indexes, round_indexes,
	       n_rounds * sizeof(uint8_t));

	session_base->set_app_config_cmd =
		cherry_uci_client_session_set_app_config_cmd_create(
			session_base->cherry_ctx->session_client_ctx);
	if (!session_base->set_app_config_cmd) {
		QLOGE("%s: failed to create fira set_app_config_cmd", __func__);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_device_role(
		session_base->set_app_config_cmd, UCI_DEVICE_ROLE_DT_TAG);
	if (r) {
		QLOGE("%s: error setting fira device type with error %d.",
		      __func__, r);
		goto error;
	}
	/* Set multi node mode to one to many. */
	r = cherry_uci_client_session_set_app_config_cmd_put_multi_node_mode(
		session_base->set_app_config_cmd, 1);
	if (r) {
		QLOGE("%s: error setting fira session multi node mode with error %d.",
		      __func__, r);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_ranging_round_usage(
		session_base->set_app_config_cmd, UCI_OWR_DL_TDOA);
	if (r) {
		QLOGE("%s: error setting fira session ranging round usage with error %d.",
		      __func__, r);
		goto error;
	}
	/* Firmware wants a mac address, but it is useless, so give it a nice random one. */
	r = cherry_uci_client_session_set_app_config_cmd_put_device_mac_address(
		session_base->set_app_config_cmd, 42);
	if (r) {
		QLOGE("%s: error setting fira device mac address with error %d.",
		      __func__, r);
		goto error;
	}

	r = cherry_uci_client_session_set_app_config_cmd_put_schedule_mode(
		session_base->set_app_config_cmd, TIME_SCHEDULED_RANGING);
	if (r) {
		QLOGE("%s: error setting fira session schedule mode with error %d.",
		      __func__, r);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_rframe_config(
		session_base->set_app_config_cmd, 0x01);
	if (r) {
		QLOGE("%s: error setting fira device rframe config with error %d.",
		      __func__, r);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_interval_ms(
		session_base->set_app_config_cmd, interval_ms);
	if (r) {
		QLOGE("%s: error setting fira interval ms with error %d.",
		      __func__, r);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_slot_duration_rstu(
		session_base->set_app_config_cmd, slot_duration);
	if (r) {
		QLOGE("%s: error setting fira slot duration with error %d.",
		      __func__, r);
		goto error;
	}
	r = cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_block_skipping(
		session_base->set_app_config_cmd, block_skipping);

	if (r) {
		QLOGE("%s: error setting fira dl-tdoa block skipping with error %d.",
		      __func__, r);
		goto error;
	}
	if (cherry_fira_send_list_task(&session->session_base, &params,
				       sizeof(struct cherry_fira_dt_params),
				       cherry_thread_task_create_dt_tag,
				       false) != CHERRY_ERR_NONE) {
		QLOGE("%s: can not create the session.", __func__);
		goto error;
	}
	return session;

error:
	cherry_fira_free(session);
	return NULL;
}

static void cherry_thread_task_update_dt_tag(const void *context,
					     const void *params, bool abort)
{
	struct cherry_session *session_base = (struct cherry_session *)context;
	struct cherry_fira_session *session =
		cherry_fira_get_child(session_base);
	struct active_rounds *rounds = (struct active_rounds *)params;
	int r;
	uint8_t dl_tdoa_update_ranging_round_array
		[DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE];
	uint8_t dl_tdoa_update_ranging_round_array_size = 0;
	if (abort)
		return;
	r = cherry_uci_client_fira_update_dt_tag_ranging_rounds(
		session->session_base.cherry_ctx->fira_client_ctx,
		session->session_base.session_handle, rounds->n_rounds,
		rounds->round_indexes, dl_tdoa_update_ranging_round_array,
		&dl_tdoa_update_ranging_round_array_size);
	if (r) {
		int index;
		for (index = 0; index < dl_tdoa_update_ranging_round_array_size;
		     index++)
			QLOGE("%s: error updating fira dl-tdoa ranging rounds number %d with error %d.",
			      __func__,
			      dl_tdoa_update_ranging_round_array[index], r);
		cherry_fira_update_error_state(session, cherry_get_error(r));
		return;
	}

	return;
}

enum cherry_err cherry_fira_session_update_dt_tag_active_rounds(
	struct cherry_fira_session *session, int n_rounds,
	const uint8_t *round_indexes)
{
	struct active_rounds rounds;

	if (!session)
		return CHERRY_ERR_INVALID_PARAMETER;

	qmutex_lock(session->session_base.session_mutex, QOSAL_WAIT_FOREVER);

	if (session->session_base.deinit) {
		QLOGI("%s session deinit", __func__);
		qmutex_unlock(session->session_base.session_mutex);
		if (session->session_base.not_supported_by_uwbs)
			return CHERRY_ERR_SESSION_TYPE_NOT_SUPPORTED;
		return CHERRY_ERR_INVALID_PARAMETER;
	}

	rounds.n_rounds = n_rounds;
	memcpy(rounds.round_indexes, round_indexes, n_rounds * sizeof(uint8_t));
	cherry_fira_send_list_task(&session->session_base, &rounds,
				   sizeof(struct active_rounds),
				   cherry_thread_task_update_dt_tag, false);
	qmutex_unlock(session->session_base.session_mutex);
	return CHERRY_ERR_NONE;
}

void cherry_fira_diag_free(struct cherry_common_diag_report *report)
{
	unsigned int i, j;

	if (!report)
		return;

	for (i = 0; i < report->n_frame_report; i++) {
		struct cherry_common_diag_frame *frame = report->frame_report;

		if (frame[i].seg_metrics)
			qfree(frame[i].seg_metrics);

		if (frame[i].aoas)
			qfree(frame[i].aoas);

		if (frame[i].cirs) {
			for (j = 0; j < frame[i].n_cir; j++) {
				if (frame[i].cirs[j].taps)
					qfree(frame[i].cirs[j].taps);
			}
			qfree(frame[i].cirs);
		}
	}

	qfree(report);
}

void cherry_fira_event_free(struct cherry_fira_event *event)
{
	if (!event)
		return;

	switch (event->type) {
	case CHERRY_FIRA_EVENT_TYPE_SESSION_STATUS:
		if (event->data.status)
			qfree(event->data.status);
		break;
	case CHERRY_FIRA_EVENT_TYPE_SESSION_ERROR:
		if (event->data.error)
			qfree(event->data.error);
		break;
	case CHERRY_FIRA_EVENT_TYPE_SESSION_TWR_RANGING_REPORT:
		if (event->data.twr_ranging) {
			qfree(event->data.twr_ranging);
		}
		break;
	case CHERRY_FIRA_EVENT_TYPE_SESSION_DIAGNOSTIC_REPORT:
		cherry_fira_diag_free(event->data.diagnostics);
		break;
	case CHERRY_FIRA_EVENT_TYPE_SESSION_DT_TAG_RANGING_REPORT:
		if (event->data.dt_tag_ranging)
			qfree(event->data.dt_tag_ranging);
	default:
		break;
	}

	qfree(event);
}

enum cherry_err
cherry_fira_session_set_rr_retry(struct cherry_fira_session *session,
				 uint16_t max_rr_retry)
{
	CHERRY_SESSION_SET_PARAM(
		&session->session_base,
		cherry_uci_client_session_set_app_config_cmd_put_max_rr_retry(
			cmd, max_rr_retry));
}

enum cherry_err
cherry_fira_session_set_nb_measurement(struct cherry_fira_session *session,
				       uint16_t max_nb_measurements)
{
	CHERRY_SESSION_SET_PARAM(
		&session->session_base,
		cherry_uci_client_session_set_app_config_cmd_put_max_number_of_measurements(
			cmd, max_nb_measurements));
}

enum cherry_err
cherry_fira_session_set_report_rssi(struct cherry_fira_session *session,
				    uint8_t report_rssi)
{
	CHERRY_SESSION_SET_PARAM(
		&session->session_base,
		cherry_uci_client_session_set_app_config_cmd_put_report_rssi(
			cmd, report_rssi));
}

enum cherry_err
cherry_fira_session_set_phy(struct cherry_fira_session *session,
			    const struct cherry_fira_phy_params *phy_params)
{
	if (!phy_params)
		return CHERRY_ERR_INVALID_PARAMETER;

	/* clang-format off */
	CHERRY_SESSION_SET_PARAMS(
		&session->session_base,
		(cherry_uci_client_session_set_app_config_cmd_put_rframe_config(
			cmd, phy_params->rframe_config))
		(cherry_uci_client_session_set_app_config_cmd_put_prf_mode(
			cmd, phy_params->prf_mode))
		(cherry_uci_client_session_set_app_config_cmd_put_sfd_id(
			cmd, phy_params->sfd_id))
		(cherry_uci_client_session_set_app_config_cmd_put_preamble_duration(
			cmd, phy_params->preamble_duration))
		(cherry_uci_client_session_set_app_config_cmd_put_number_of_sts_segments(
			cmd, phy_params->nb_sts_segments))
		(cherry_uci_client_session_set_app_config_cmd_put_sts_length(
			cmd, phy_params->sts_length),
			phy_params->sts_length != CHERRY_COMMON_STS_LENGTH_NA)
		(cherry_uci_client_session_set_app_config_cmd_put_psdu_data_rate(
			cmd, phy_params->psdu_data_rate),
			phy_params->psdu_data_rate != CHERRY_COMMON_PSDU_DATA_RATE_NA)
	);
	/* clang-format on */
}

enum cherry_err cherry_fira_session_set_result_report_config(
	struct cherry_fira_session *session, uint8_t result_report_config)
{
	CHERRY_SESSION_SET_PARAM(
		&session->session_base,
		cherry_uci_client_session_set_app_config_cmd_put_result_report_config(
			cmd, result_report_config));
}

enum cherry_err
cherry_fira_session_set_hopping_mode(struct cherry_fira_session *session,
				     uint8_t hopping_mode)
{
	CHERRY_SESSION_SET_PARAM(
		&session->session_base,
		cherry_uci_client_session_set_app_config_cmd_put_hopping_mode(
			cmd, hopping_mode));
}

enum cherry_err cherry_fira_session_set_vendor_id(
	struct cherry_fira_session *session,
	const uint8_t vendor_id[CHERRY_VENDOR_ID_SIZE])
{
	CHERRY_SESSION_SET_PARAM(
		&session->session_base,
		cherry_uci_client_session_set_app_config_cmd_put_vendor_id(
			cmd, vendor_id));
}

enum cherry_err
cherry_fira_session_set_static_sts(struct cherry_fira_session *session,
				   const uint8_t sts_iv[CHERRY_STATIC_STS_SIZE])
{
	/* clang-format off */
	CHERRY_SESSION_SET_PARAMS(
		&session->session_base,
		(cherry_uci_client_session_set_app_config_cmd_put_sts_config(cmd, 0x00))
		(cherry_uci_client_session_set_app_config_cmd_put_static_sts_iv(cmd, sts_iv))
	);
	/* clang-format on */
}

enum cherry_err
cherry_fira_session_set_prov_sts(struct cherry_fira_session *session,
				 uint8_t sts_key_len, const uint8_t *sts_key,
				 uint8_t key_rotation_rate)
{
	if (sts_key_len != 16 && sts_key_len != 32) {
		QLOGE("%s: Only length of 16 or 32 are supported.", __func__);
		return CHERRY_ERR_INVALID_PARAMETER;
	}

	if (!sts_key) {
		QLOGE("%s: Sts key is invalid.", __func__);
		return CHERRY_ERR_INVALID_PARAMETER;
	}

	/* clang-format off */
	CHERRY_SESSION_SET_PARAMS(
		&session->session_base,
		(cherry_uci_client_session_set_app_config_cmd_put_sts_config(cmd, 0x03))
		(cherry_uci_client_session_set_app_config_cmd_put_key_rotation(cmd, key_rotation_rate ? 1 : 0 ))
		(cherry_uci_client_session_set_app_config_cmd_put_key_rotation_rate(cmd, key_rotation_rate))
		(cherry_uci_client_session_set_app_config_cmd_put_session_key(cmd, sts_key, sts_key_len))
	);
	/* clang-format on */
}

enum cherry_err
cherry_fira_session_set_diagnostics(struct cherry_fira_session *session,
				    struct cherry_common_diag_cfg config)
{
	uint8_t fields = 0;
	uint8_t enable_diag = 0;

	if (config.aoa) {
		fields |= 0x2;
		enable_diag = 1;
	}

	if (config.extra_status) {
		/* Enable the reporting of the extra status bit field, no bitfield fot that. */
		enable_diag = 1;
	}

	if (config.cirs) {
		fields |= 0x40;
		enable_diag = 1;
	}

	if (config.seg_metrics) {
		fields |= 0x20;
		enable_diag = 1;
	}

	if (config.cfo) {
		fields |= 0x8;
		enable_diag = 1;
	}

	if (config.emitter_addr) {
		fields |= 0x10;
		enable_diag = 1;
	}

	CHERRY_SESSION_SET_PARAMS(
		&session->session_base,
		(cherry_uci_client_session_set_app_config_cmd_put_diags_frame_report_fields(
			cmd, fields))
		/* Active or deactive diag. */
		(cherry_uci_client_session_set_app_config_cmd_put_enable_diags(
			cmd, enable_diag)));
}

enum cherry_err
cherry_fira_session_set_antenna(struct cherry_fira_session *session,
				uint8_t antenna_set)
{
	CHERRY_SESSION_SET_PARAM(
		&session->session_base,
		cherry_uci_client_session_set_app_config_cmd_put_antenna_selection(
			cmd, antenna_set));
}

enum cherry_err cherry_fira_session_set_slots_per_ranging_round(
	struct cherry_fira_session *session, uint8_t slots_per_rr)
{
	CHERRY_SESSION_SET_PARAM(
		&session->session_base,
		cherry_uci_client_session_set_app_config_cmd_put_slots_per_ranging_round(
			cmd, slots_per_rr));
}

enum cherry_err
cherry_fira_session_set_dltdoa_tx_active_rr(struct cherry_fira_session *session,
					    uint8_t tx_active_rr)
{
	CHERRY_SESSION_SET_PARAM(
		&session->session_base,
		cherry_uci_client_session_set_app_config_cmd_put_dl_tx_active_ranging_rounds(
			cmd, tx_active_rr));
}
