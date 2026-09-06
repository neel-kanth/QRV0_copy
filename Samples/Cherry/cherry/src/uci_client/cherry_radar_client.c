/*
 * Implementation for radar client functions
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "cherry_radar_client.h"

#include <qmalloc.h>
#include <qtils.h>
#include <stdio.h>
#include <uci/uci_message.h>
#include <uci/uci_unit_converter.h>

struct cherry_radar_context {
	struct uci *uci;
	cherry_uci_client_radar_notification_cb_t radar_notification_cb;
	void *user_data;
};

static bool parse_radar_samples(struct uci_message_parser *parser,
				struct cherry_radar_sweep *sweeps,
				uint8_t n_sweeps, uint8_t sample_size,
				uint8_t samples_per_sweep)
{
	uint8_t vendor_data_len;
	uint8_t bytes_per_sample;

	for (int sweep_idx = 0; sweep_idx < n_sweeps; sweep_idx++) {
		sweeps[sweep_idx].vendor_data = NULL;
		sweeps[sweep_idx].data_fragments = NULL;

		if (!uci_message_get_32bit_no_assert(
			    parser, &sweeps[sweep_idx].sequence_number) ||
		    !uci_message_get_32bit_no_assert(
			    parser, &sweeps[sweep_idx].timestamp) ||
		    !uci_message_get_8bit_no_assert(parser, &vendor_data_len))
			return false;

		sweeps[sweep_idx].vendor_data_len = vendor_data_len;
		if (vendor_data_len) {
			sweeps[sweep_idx].vendor_data =
				(uint8_t *)qmalloc(vendor_data_len);
			if (!sweeps[sweep_idx].vendor_data)
				return false;
			uci_message_get(parser, sweeps[sweep_idx].vendor_data,
					vendor_data_len);
		} else
			sweeps[sweep_idx].vendor_data = NULL;

		/* Number of data fragments is set to 1. */
		sweeps[sweep_idx].n_data_fragments = 1;

		sweeps[sweep_idx].data_fragments =
			(struct data_fragment *)qmalloc(
				sizeof(struct data_fragment) *
				sweeps[sweep_idx].n_data_fragments);
		if (!sweeps[sweep_idx].data_fragments)
			return false;

		bytes_per_sample = (sample_size * 2) + 4;
		sweeps[sweep_idx].data_fragments[0].size =
			bytes_per_sample * samples_per_sweep;
		sweeps[sweep_idx].data_fragments[0].data = (uint8_t *)qmalloc(
			sweeps[sweep_idx].data_fragments[0].size);
		if (!sweeps[sweep_idx].data_fragments[0].data)
			return false;

		if (uci_message_remaining(parser) >=
		    sweeps[sweep_idx].data_fragments[0].size)
			uci_message_get(
				parser,
				sweeps[sweep_idx].data_fragments[0].data,
				sweeps[sweep_idx].data_fragments[0].size);
		else
			return false;
	}
	return true;
}

static enum qerr uci_radar_data_handler(struct uci *uci, uint16_t mt_dpf,
					const struct uci_blk *payload,
					void *user_data)
{
	struct cherry_radar_context *context = user_data;
	struct cherry_uci_radar_ntf *report;
	uint8_t status;
	uint8_t sample_size;
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(payload);

	if (uci_message_remaining(&parser) < 5) {
		return QERR_EINVAL;
	}

	report = qmalloc(sizeof(*report));

	if (!report) {
		return QERR_ENOMEM;
	}

	report->data = qmalloc(sizeof(struct cherry_radar_session_report));
	report->data->sweeps = NULL;

	if (!uci_message_get_32bit_no_assert(&parser,
					     &report->session_handle) ||
	    !uci_message_get_8bit_no_assert(&parser, &status))
		goto err;

	report->data->status = status;
	if (report->data->status == CHERRY_RADAR_REPORT_STATUS_ERROR)
		goto callback;

	if (report->data->status != CHERRY_RADAR_REPORT_STATUS_SUCCESS)
		goto err;

	if (!uci_message_skip(&parser, 1) /* Skip data type. */ ||
	    !uci_message_get_8bit_no_assert(&parser, &report->data->n_sweeps) ||
	    !uci_message_get_8bit_no_assert(&parser,
					    &report->data->samples_per_sweep) ||
	    !uci_message_get_8bit_no_assert(&parser, &sample_size) ||
	    !uci_message_skip(&parser, 2) /* Skip sweep offset. */ ||
	    !uci_message_skip(&parser, 2) /* Skip sweep data size. */
	)
		goto err;

	report->data->sample_size = sample_size;
	if (report->data->sample_size > CHERRY_RADAR_SAMPLE_SIZE_64_BITS)
		goto err;

	report->data->sweeps = (struct cherry_radar_sweep *)qmalloc(
		sizeof(struct cherry_radar_sweep) * report->data->n_sweeps);
	if (!report->data->sweeps)
		goto err;

	if (!parse_radar_samples(
		    &parser, report->data->sweeps, report->data->n_sweeps,
		    report->data->sample_size, report->data->samples_per_sweep))
		goto err;

	/* If data remaining we consider the notification false. */
	if (uci_message_remaining(&parser))
		goto err;

callback:
	context->radar_notification_cb(report, context->user_data);

	return QERR_SUCCESS;

err:
	cherry_uci_client_radar_free_data_report(report->data);
	cherry_uci_client_radar_free_base_report(report);
	return QERR_EINVAL;
}

void cherry_uci_client_radar_free_base_report(
	struct cherry_uci_radar_ntf *report)
{
	qfree(report);
}

void cherry_uci_client_radar_free_data_report(
	struct cherry_radar_session_report *data)
{
	if (!data)
		return;

	if (data->sweeps) {
		for (int i = 0; i < data->n_sweeps; i++) {
			if (data->sweeps[i].vendor_data)
				qfree(data->sweeps[i].vendor_data);
			if (data->sweeps[i].data_fragments) {
				for (int j = 0;
				     j < data->sweeps[i].n_data_fragments; j++)
					qfree(data->sweeps[i]
						      .data_fragments[j]
						      .data);
				qfree((data->sweeps[i].data_fragments));
			}
		}
		qfree(data->sweeps);
	}
	qfree(data);
}

/* Handlers in table need to have their OID's ordered ascending. */
static const struct uci_message_handler uci_ntf_radar_dpf_handlers[] = {
	{
		.mt_dpf = UCI_MT_DPF(UCI_MESSAGE_TYPE_DATA,
				     UCI_MESSAGE_DPF_RADAR),
		.handler = uci_radar_data_handler,
	},
};

static struct uci_message_handlers uci_ntf_radar_dpf_handlers_list = {
	.next = NULL,
	.handlers = uci_ntf_radar_dpf_handlers,
	.n_handlers = qarray_size(uci_ntf_radar_dpf_handlers),
	.user_data = NULL,
};

enum qerr
cherry_uci_client_radar_open(struct cherry_radar_context **context,
			     struct uci *uci, void *user_data,
			     cherry_uci_client_radar_notification_cb_t radar_cb)
{
	struct cherry_radar_context *ctx;

	if (!radar_cb)
		return QERR_EINVAL;

	ctx = (struct cherry_radar_context *)qmalloc(
		sizeof(struct cherry_radar_context));

	if (!ctx)
		return QERR_EINVAL;

	ctx->uci = uci;
	ctx->user_data = user_data;
	ctx->radar_notification_cb = radar_cb;

	uci_ntf_radar_dpf_handlers_list.user_data = ctx;
	uci_message_handlers_register(ctx->uci,
				      &uci_ntf_radar_dpf_handlers_list);

	*context = ctx;

	return QERR_SUCCESS;
}

void cherry_uci_client_radar_close(struct cherry_radar_context *context)
{
	if (!context)
		return;

	context->radar_notification_cb = NULL;
	context->user_data = NULL;

	uci_ntf_radar_dpf_handlers_list.user_data = NULL;
	uci_message_handlers_unregister(context->uci,
					&uci_ntf_radar_dpf_handlers_list);
	qfree(context);
}
