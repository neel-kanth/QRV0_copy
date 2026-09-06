/*
 * Implementation for ccc client functions
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "cherry_ccc_client.h"

#include "cherry_session_client.h"

#include <qmalloc.h>
#include <qtils.h>
#include <stdio.h>
#include <uci/uci_message.h>
#include <uci/uci_unit_converter.h>

#define CCC_INFO_NTF_HEADER_SIZE 25

static bool parse_ccc_controller_report(
	struct uci_message_parser *parser,
	struct cherry_ccc_controller_session_report *controller_report,
	uint32_t sequence_number, uint8_t nb_measurements)
{
	struct cherry_ccc_session_controller_measurements *measurements = NULL;
	struct cherry_ccc_session_controller_measurements *measurements_tmp =
		NULL;
	uint8_t status;
	bool ret;

	controller_report->sequence_number = sequence_number;
	controller_report->n_measurements = nb_measurements;
	controller_report->measurements = NULL;

	for (; nb_measurements > 0; --nb_measurements) {
		measurements_tmp = measurements;
		measurements = qmalloc(sizeof(
			struct cherry_ccc_session_controller_measurements));
		if (!measurements)
			return false;
		measurements->next = NULL;
		if (!measurements_tmp)
			controller_report->measurements = measurements;
		else
			controller_report->measurements->next = measurements;

		ret = uci_message_get_8bit_no_assert(parser, &status) &&
		      uci_message_get_8bit_no_assert(
			      parser, &measurements->slot_index) &&
		      uci_message_get_16bit_no_assert(
			      parser, &measurements->rr_index) &&
		      uci_message_get_32bit_no_assert(
			      parser, &measurements->sts_index) &&
		      uci_message_get_8bit_no_assert(
			      parser, &measurements->ranging_round);

		if (ret)
			measurements->frame_status =
				cherry_session_frame_status_to_cherry_format(
					status);
		else
			return ret;

		uci_message_skip(parser, 11); /* RFU. */
	}

	return true;
}

static bool parse_ccc_controlee_report(
	struct uci_message_parser *parser,
	struct cherry_ccc_controlee_session_report *controlee_report,
	uint32_t sequence_number, uint8_t nb_measurements)
{
	struct cherry_ccc_session_controlee_measurements *measurements = NULL;
	struct cherry_ccc_session_controlee_measurements *measurements_tmp =
		NULL;
	uint8_t status;
	bool ret;

	controlee_report->sequence_number = sequence_number;
	controlee_report->n_measurements = nb_measurements;
	controlee_report->measurements = NULL;

	for (; nb_measurements > 0; --nb_measurements) {
		measurements_tmp = measurements;
		measurements = qmalloc(sizeof(
			struct cherry_ccc_session_controlee_measurements));
		if (!measurements)
			return false;
		measurements->next = NULL;
		if (!measurements_tmp)
			controlee_report->measurements = measurements;
		else
			controlee_report->measurements->next = measurements;

		ret = uci_message_get_8bit_no_assert(parser, &status) &&
		      uci_message_get_8bit_no_assert(
			      parser, &measurements->slot_index) &&
		      uci_message_get_16bit_no_assert(
			      parser, &measurements->rr_index) &&
		      uci_message_get_32bit_no_assert(
			      parser, &measurements->sts_index) &&
		      uci_message_get_16bit_no_assert(
			      parser, &measurements->distance_cm) &&
		      uci_message_get_8bit_no_assert(
			      parser, &measurements->uncertainty_anchor) &&
		      uci_message_get_8bit_no_assert(
			      parser, &measurements->uncertainty_initiator) &&
		      uci_message_get_8bit_no_assert(
			      parser, &measurements->ranging_round);

		if (ret)
			measurements->frame_status =
				cherry_session_frame_status_to_cherry_format(
					status);
		else
			return ret;

		uci_message_skip(parser, 11); /* RFU. */
	}

	return true;
}

enum qerr cherry_uci_client_parse_ccc_controller_measurements(
	const struct session_ranging_data *data,
	struct cherry_ccc_controller_session_report *controller_report)
{
	bool ret = false;

	ret = parse_ccc_controller_report(data->parser, controller_report,
					  data->sequence_number,
					  data->n_measurements);

	if (!ret)
		goto err;

	/* If data remaining we consider the notification false. */
	if (uci_message_remaining(data->parser))
		goto err;

	return QERR_SUCCESS;

err:
	return QERR_EINVAL;
}

enum qerr cherry_uci_client_parse_ccc_controlee_measurements(
	const struct session_ranging_data *data,
	struct cherry_ccc_controlee_session_report *controlee_report)
{
	bool ret = false;

	ret = parse_ccc_controlee_report(data->parser, controlee_report,
					 data->sequence_number,
					 data->n_measurements);

	if (!ret)
		goto err;

	/* If data remaining we consider the notification false. */
	if (uci_message_remaining(data->parser))
		goto err;

	return QERR_SUCCESS;

err:
	return QERR_EINVAL;
}

void cherry_uci_client_ccc_free_data_controller_report(
	struct cherry_ccc_controller_session_report *controller_report)
{
	struct cherry_ccc_session_controller_measurements *measurements,
		*measurements_tmp;

	if (!controller_report)
		return;

	measurements = controller_report->measurements;
	while (measurements) {
		measurements_tmp = measurements;
		measurements = measurements_tmp->next;
		qfree(measurements_tmp);
	}

	qfree(controller_report);
}

void cherry_uci_client_ccc_free_data_controlee_report(
	struct cherry_ccc_controlee_session_report *controlee_report)
{
	struct cherry_ccc_session_controlee_measurements *measurements,
		*measurements_tmp;

	if (!controlee_report)
		return;

	measurements = controlee_report->measurements;
	while (measurements) {
		measurements_tmp = measurements;
		measurements = measurements_tmp->next;
		qfree(measurements_tmp);
	}

	qfree(controlee_report);
}
