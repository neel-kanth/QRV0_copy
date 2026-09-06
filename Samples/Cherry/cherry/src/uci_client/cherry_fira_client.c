/*
 * Implementation for fira client functions
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "cherry_fira_client.h"

#define LOG_TAG "cherry_uci_client"
#include "cherry_log.h"

#include <qmalloc.h>
#include <qsemaphore.h>
#include <qtils.h>
#include <stdio.h>
#include <uci/uci_message.h>
#include <uci/uci_unit_converter.h>

struct cherry_fira_context {
	struct uci *uci;
	cherry_uci_client_fira_diag_notification_cb_t diag_notification_cb;
	void *user_data;
	int response_status;
	struct qsemaphore *sem_response;
	void *cmd_data;
	size_t cmd_data_sz;
	uint32_t session_handle;
};

struct active_round_config {
	uint8_t index;
	uint8_t acting_role;
	uint8_t number_of_responders;
	uint16_t *responders;
	uint8_t slots_scheduling;
	uint16_t *slots;
};

struct tmp_cir {
	uint8_t receiver_segment;
	int16_t fpath_tap_offset;
	uint16_t n_taps;
	uint8_t tap_size;
	uint8_t *taps;
	struct tmp_cir *next;
};

static inline int cherry_fira_wait_rsp(struct cherry_fira_context *context)
{
	int r = qsemaphore_take(context->sem_response, 1000);

	/* Error or timeout. */
	if (r)
		return 0;

	/* Completed in time. */
	return 1;
}

static inline void cherry_fira_set_rsp(struct cherry_fira_context *context)
{
	qsemaphore_give(context->sem_response);
}

static enum uci_status_code parse_status(struct uci_message_parser *parser)
{
	/* check that we have a reset config value */
	if (uci_message_remaining(parser) < 1) {
		return UCI_STATUS_INVALID_MESSAGE_SIZE;
	}

	return uci_message_get_8bit(parser);
}

static enum qerr
uci_rsp_handler_dt_ranging_rounds(struct uci *uci, uint16_t mt_gid_oid,
				  const struct uci_blk *payload,
				  void *user_data)
{
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(payload);
	struct cherry_fira_context *context = user_data;

	uint8_t number_of_ranging_rounds;
	uint8_t no_ranging_round = 0;

	if (uci_message_remaining(&parser) < 2) {
		context->cmd_data_sz = 0;
		context->response_status = UCI_STATUS_INVALID_MESSAGE_SIZE;
		cherry_fira_set_rsp(context);
		return QERR_EINVAL;
	}

	context->response_status = parse_status(&parser);

	number_of_ranging_rounds = uci_message_get_8bit(&parser);

	if (number_of_ranging_rounds > 0) {
		if (uci_message_remaining(&parser) < number_of_ranging_rounds) {
			context->cmd_data_sz = 0;
			context->response_status =
				UCI_STATUS_INVALID_MESSAGE_SIZE;
			cherry_fira_set_rsp(context);
			return QERR_EINVAL;
		}
		uci_message_get(&parser, context->cmd_data,
				number_of_ranging_rounds);
		context->cmd_data_sz =
			number_of_ranging_rounds * sizeof(uint8_t);

	} else {
		memcpy(context->cmd_data, &no_ranging_round, sizeof(uint8_t));
		context->cmd_data_sz = 0;
	}

	cherry_fira_set_rsp(context);

	return QERR_SUCCESS;
}

/* Handlers in table need to have their OID's ordered ascending. */
static const struct uci_message_handler uci_rsp_session_config_handlers[] = {
	{
		.mt_gid_oid = UCI_MT_GID_OID(
			UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_SESSION_CONFIG,
			UCI_OID_SESSION_UPDATE_DT_ANCHOR_RANGING_ROUNDS),
		.handler = uci_rsp_handler_dt_ranging_rounds,
	},
	{
		.mt_gid_oid = UCI_MT_GID_OID(
			UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_SESSION_CONFIG,
			UCI_OID_SESSION_UPDATE_DT_TAG_RANGING_ROUNDS),
		.handler = uci_rsp_handler_dt_ranging_rounds,
	},
};

static struct uci_message_handlers uci_rsp_session_config_handlers_list = {
	.next = NULL,
	.handlers = uci_rsp_session_config_handlers,
	.n_handlers = qarray_size(uci_rsp_session_config_handlers),
	.user_data = NULL,
};

enum qerr cherry_uci_client_parse_twr_measurements(
	const struct session_ranging_data *data,
	struct twr_ranging_results *twr_results)
{
	struct uci_message_parser *parser = data->parser;

	twr_results->sequence_number = data->sequence_number;
	twr_results->session_handle = data->session_handle;
	twr_results->ranging_interval_ms = data->ranging_interval_ms;
	twr_results->n_measurements = data->n_measurements;

	/* TODO: check the remaining size. */
	for (int i = 0; i < twr_results->n_measurements; i++) {
		twr_results->measurements[i].short_addr =
			uci_message_get_16bit(parser);
		twr_results->measurements[i].status =
			uci_message_get_8bit(parser);
		twr_results->measurements[i].nlos =
			uci_message_get_8bit(parser);
		twr_results->measurements[i].distance_mm =
			uci_message_get_16bit(parser) * 10; /* cm -> mm */
		twr_results->measurements[i]
			.local_aoa_measurements[CHERRY_FIRA_CLIENT_AOA_AZIMUTH]
			.aoa = uci_message_get_16bit(parser);
		twr_results->measurements[i]
			.local_aoa_measurements[CHERRY_FIRA_CLIENT_AOA_AZIMUTH]
			.aoa_fom = uci_message_get_8bit(parser);
		twr_results->measurements[i]
			.local_aoa_measurements[CHERRY_FIRA_CLIENT_AOA_ELEVATION]
			.aoa = uci_message_get_16bit(parser);
		twr_results->measurements[i]
			.local_aoa_measurements[CHERRY_FIRA_CLIENT_AOA_ELEVATION]
			.aoa_fom = uci_message_get_8bit(parser);
		twr_results->measurements[i].remote_aoa_azimuth =
			uci_message_get_16bit(parser);
		twr_results->measurements[i].remote_aoa_azimuth_fom =
			uci_message_get_8bit(parser);
		twr_results->measurements[i].remote_aoa_elevation =
			uci_message_get_16bit(parser);
		twr_results->measurements[i].remote_aoa_elevation_fom =
			uci_message_get_8bit(parser);
		twr_results->measurements[i].slot_index =
			uci_message_get_8bit(parser);
		twr_results->measurements[i].rssi =
			uci_message_get_8bit(parser);
		uci_message_skip(parser, 8); //skip RFU 1st 8 octets
		uci_message_skip(parser, 2); //skip RFU 2nd 2 octets
		uci_message_skip(parser, 1); //skip RFU 3rd octect
	}

	return QERR_SUCCESS;
}

static int
parse_location_coor_wgs(struct cherry_fira_session_dt_tag_measurements *meas,
			uint8_t *anchor_location)
{
	uint64_t temp[3];
	uint8_t *latitude = (uint8_t *)&temp[0];
	uint8_t *longitude = (uint8_t *)&temp[1];
	uint8_t *altitude = (uint8_t *)&temp[2];
	temp[0] = 0;
	temp[1] = 0;
	temp[2] = 0;
	meas->location.type = CHERRY_FIRA_ANCHOR_LOCATION_TYPE_WGS84;

	/* 33b for latitute / 33b for longitude / 30b for altitude. */
	latitude[0] = anchor_location[1];
	latitude[1] = anchor_location[2];
	latitude[2] = anchor_location[3];
	latitude[3] = anchor_location[4];
	latitude[4] = (anchor_location[5] >> 7) & 0x01;

	longitude[0] = ((anchor_location[5] << 1) & 0xFE) |
		       ((anchor_location[6] >> 7) & 0x01);
	longitude[1] = ((anchor_location[6] << 1) & 0xFE) |
		       ((anchor_location[7] >> 7) & 0x01);
	longitude[2] = ((anchor_location[7] << 1) & 0xFE) |
		       ((anchor_location[8] >> 7) & 0x01);
	longitude[3] = ((anchor_location[8] << 1) & 0xFE) |
		       ((anchor_location[9] >> 7) & 0x01);
	longitude[4] = (anchor_location[9] >> 6) & 0x01;

	altitude[0] = ((anchor_location[9] << 2) & 0xFC) |
		      ((anchor_location[10] >> 6) & 0x03);
	altitude[1] = ((anchor_location[10] << 2) & 0xFC) |
		      ((anchor_location[11] >> 6) & 0x03);
	altitude[2] = ((anchor_location[11] << 2) & 0xFC) |
		      ((anchor_location[12] >> 6) & 0x03);
	altitude[3] = anchor_location[12] & 0x3F;

	meas->location.data.wgs84.latitude = qle64toh(temp[0]);
	meas->location.data.wgs84.longitude = qle64toh(temp[1]);
	meas->location.data.wgs84.altitude = qle64toh(temp[2]);

	return 0;
}

static int
parse_location_coor_rel(struct cherry_fira_session_dt_tag_measurements *meas,
			uint8_t *anchor_location)
{
	uint64_t temp[3];
	uint8_t *relative_x = (uint8_t *)&temp[0];
	uint8_t *relative_y = (uint8_t *)&temp[1];
	uint8_t *relative_z = (uint8_t *)&temp[2];
	temp[0] = 0;
	temp[1] = 0;
	temp[2] = 0;
	meas->location.type = CHERRY_FIRA_ANCHOR_LOCATION_TYPE_REL;
	/* 28b for x coordinates / 28b for y coordinates / 24b z coordinates */
	relative_x[0] = anchor_location[1];
	relative_x[1] = anchor_location[2];
	relative_x[2] = anchor_location[3];
	relative_x[3] = (anchor_location[4] >> 4) & 0x0F;

	relative_y[0] = ((anchor_location[4] << 4) & 0xF0) |
			((anchor_location[5] >> 4) & 0x0F);
	relative_y[1] = ((anchor_location[5] << 4) & 0xF0) |
			((anchor_location[6] >> 4) & 0x0F);
	relative_y[2] = ((anchor_location[6] << 4) & 0xF0) |
			((anchor_location[7] >> 4) & 0x0F);
	relative_y[3] = anchor_location[7] & 0x0F;

	relative_z[0] = anchor_location[8];
	relative_z[1] = anchor_location[9];
	relative_z[2] = anchor_location[10];

	meas->location.data.relative.x = qle64toh(temp[0]);
	meas->location.data.relative.y = qle64toh(temp[1]);
	meas->location.data.relative.z = qle64toh(temp[2]);

	return 0;
}

static int parse_location_coor_wgs_z_ext(
	struct cherry_fira_session_dt_tag_measurements *meas,
	uint8_t *anchor_location)
{
	uint64_t temp[3];
	uint8_t *latitude = (uint8_t *)&temp[0];
	uint8_t *longitude = (uint8_t *)&temp[1];
	uint8_t *altitude = (uint8_t *)&temp[2];
	temp[0] = 0;
	temp[1] = 0;
	temp[2] = 0;
	meas->location.type = CHERRY_FIRA_ANCHOR_LOCATION_TYPE_WGS84_Z_EXT;

	/* 33b for latitute / 33b for longitude / 30b for altitude. */
	latitude[0] = anchor_location[1];
	latitude[1] = anchor_location[2];
	latitude[2] = anchor_location[3];
	latitude[3] = anchor_location[4];
	latitude[4] = (anchor_location[5] >> 7) & 0x01;

	longitude[0] = ((anchor_location[5] << 1) & 0xFE) |
		       ((anchor_location[6] >> 7) & 0x01);
	longitude[1] = ((anchor_location[6] << 1) & 0xFE) |
		       ((anchor_location[7] >> 7) & 0x01);
	longitude[2] = ((anchor_location[7] << 1) & 0xFE) |
		       ((anchor_location[8] >> 7) & 0x01);
	longitude[3] = ((anchor_location[8] << 1) & 0xFE) |
		       ((anchor_location[9] >> 7) & 0x01);
	longitude[4] = (anchor_location[9] >> 6) & 0x01;

	altitude[0] = ((anchor_location[9] << 2) & 0xFC) |
		      ((anchor_location[10] >> 6) & 0x03);
	altitude[1] = ((anchor_location[10] << 2) & 0xFC) |
		      ((anchor_location[11] >> 6) & 0x03);
	altitude[2] = ((anchor_location[11] << 2) & 0xFC) |
		      ((anchor_location[12] >> 6) & 0x03);
	altitude[3] = anchor_location[12] & 0x3F;

	meas->location.data.wgs84_z_ext.latitude = qle64toh(temp[0]);
	meas->location.data.wgs84_z_ext.longitude = qle64toh(temp[1]);
	meas->location.data.wgs84_z_ext.altitude = qle64toh(temp[2]);
	meas->location.data.wgs84_z_ext.floor =
		qle16toh((anchor_location[13] << 6) +
			 ((anchor_location[14] & 0xFC) >> 2)); //14 bits
	meas->location.data.wgs84_z_ext.moveable =
		(anchor_location[14] & 0x6); // 2 bits
	meas->location.data.wgs84_z_ext.height = qle32toh(
		(anchor_location[15] << 16) + (anchor_location[16] << 8) +
		anchor_location[17]); //24 bits
	meas->location.data.wgs84_z_ext.height_uncertainty =
		anchor_location[18]; //8 bits

	return 0;
}

static int parse_location_coor_rel_z_ext(
	struct cherry_fira_session_dt_tag_measurements *meas,
	uint8_t *anchor_location)
{
	uint64_t temp[3];
	uint8_t *relative_x = (uint8_t *)&temp[0];
	uint8_t *relative_y = (uint8_t *)&temp[1];
	uint8_t *relative_z = (uint8_t *)&temp[2];
	temp[0] = 0;
	temp[1] = 0;
	temp[2] = 0;
	meas->location.type = CHERRY_FIRA_ANCHOR_LOCATION_TYPE_REL_Z_EXT;
	/* 28b for x coordinates / 28b for y coordinates / 24b z coordinates */
	relative_x[0] = anchor_location[1];
	relative_x[1] = anchor_location[2];
	relative_x[2] = anchor_location[3];
	relative_x[3] = (anchor_location[4] >> 4) & 0x0F;

	relative_y[0] = ((anchor_location[4] << 4) & 0xF0) |
			((anchor_location[5] >> 4) & 0x0F);
	relative_y[1] = ((anchor_location[5] << 4) & 0xF0) |
			((anchor_location[6] >> 4) & 0x0F);
	relative_y[2] = ((anchor_location[6] << 4) & 0xF0) |
			((anchor_location[7] >> 4) & 0x0F);
	relative_y[3] = anchor_location[7] & 0x0F;

	relative_z[0] = anchor_location[8];
	relative_z[1] = anchor_location[9];
	relative_z[2] = anchor_location[10];

	meas->location.data.relative_z_ext.x = qle64toh(temp[0]);
	meas->location.data.relative_z_ext.y = qle64toh(temp[1]);
	meas->location.data.relative_z_ext.z = qle64toh(temp[2]);
	meas->location.data.relative_z_ext.floor =
		qle16toh((anchor_location[11] << 6) +
			 ((anchor_location[12] & 0xFC) >> 2)); //14 bits
	meas->location.data.relative_z_ext.moveable =
		(anchor_location[12] & 0x6); // 2 bits
	meas->location.data.relative_z_ext.height = qle32toh(
		(anchor_location[13] << 16) + (anchor_location[14] << 8) +
		anchor_location[15]); //24 bits
	meas->location.data.relative_z_ext.height_uncertainty =
		anchor_location[16]; //8 bits

	return 0;
}

enum qerr cherry_uci_client_parse_dltdoa_measurements(
	const struct session_ranging_data *data,
	struct cherry_fira_session_dt_tag_ranging_report *dest)
{
	struct uci_message_parser *parser = data->parser;
	uint8_t anchor_location[FIRA_DL_TDOA_ANCHOR_LOCATION_SIZE_MAX];
	uint64_t temp[3];
	dest->sequence_number = data->sequence_number;
	dest->n_measurements = data->n_measurements;

	if ((uci_message_remaining(parser)) <
	    (size_t)(41 * dest->n_measurements)) {
		QLOGE("Incorrect size of ranging measurements received, ignore it");
		return QERR_EINVAL;
	}

	for (int i = 0; i < dest->n_measurements; i++) {
		struct cherry_fira_session_dt_tag_measurements *meas =
			&dest->measurements[i];

		uint16_t message_control;

		temp[0] = 0;
		temp[1] = 0;
		temp[2] = 0;

		meas->short_addr = uci_message_get_16bit(parser);
		meas->frame_status =
			cherry_session_frame_status_to_cherry_format(
				uci_message_get_8bit(parser));
		meas->msg_type =
			(enum cherry_fira_dltdoa_msg)uci_message_get_8bit(
				parser);
		message_control = uci_message_get_16bit(parser);
		meas->common_time_base =
			(message_control & 0x1) ==
			FIRA_OWR_DTM_TIMESTAMP_COMMON_TIME_BASE; /* b0 */
		meas->n_rr_indexes = (message_control >> 7) & 0xF; /* b7-10 */
		meas->block_index = uci_message_get_16bit(parser);
		meas->round_index = uci_message_get_8bit(parser);
		meas->nlos = uci_message_get_8bit(parser);
		meas->aoa[CHERRY_AOA_AZIMUTH].aoa =
			uci_message_get_16bit(parser);
		meas->aoa[CHERRY_AOA_AZIMUTH].aoa_fom =
			uci_message_get_8bit(parser);
		meas->aoa[CHERRY_AOA_ELEVATION].aoa =
			uci_message_get_16bit(parser);
		meas->aoa[CHERRY_AOA_ELEVATION].aoa_fom =
			uci_message_get_8bit(parser);
		meas->rssi = uci_message_get_8bit(parser);
		if (((message_control >> 1) & 0x3) ==
		    FIRA_OWR_DTM_TIMESTAMP_40BITS)
			meas->tx_timestamp = uci_message_get_40bit(parser);
		else /* FIRA_OWR_DTM_TIMESTAMP_64BITS */
			meas->tx_timestamp = uci_message_get_64bit(parser);
		if (((message_control >> 3) & 0x3) ==
		    FIRA_OWR_DTM_TIMESTAMP_40BITS)
			meas->rx_timestamp = uci_message_get_40bit(parser);
		else /* FIRA_OWR_DTM_TIMESTAMP_64BITS */
			meas->rx_timestamp = uci_message_get_64bit(parser);
		meas->anchor_cfo = uci_message_get_16bit(parser);
		meas->cfo = uci_message_get_16bit(parser);
		meas->initiator_reply_time = uci_message_get_32bit(parser);
		meas->responder_reply_time = uci_message_get_32bit(parser);
		meas->tof = uci_message_get_16bit(parser);
		if (((message_control >> 5) & 0x3) == 1) {
			uint8_t *latitude = (uint8_t *)&temp[0];
			uint8_t *longitude = (uint8_t *)&temp[1];
			uint8_t *altitude = (uint8_t *)&temp[2];
			meas->location.type =
				CHERRY_FIRA_ANCHOR_LOCATION_TYPE_WGS84;
			uci_message_get(parser, (uint8_t *)anchor_location, 12);

			/* 33b for latitute / 33b for longitude / 30b for altitude. */
			latitude[0] = anchor_location[0];
			latitude[1] = anchor_location[1];
			latitude[2] = anchor_location[2];
			latitude[3] = anchor_location[3];
			latitude[4] = (anchor_location[4] >> 7) & 0x01;

			longitude[0] = ((anchor_location[4] << 1) & 0xFE) |
				       ((anchor_location[5] >> 7) & 0x01);
			longitude[1] = ((anchor_location[5] << 1) & 0xFE) |
				       ((anchor_location[6] >> 7) & 0x01);
			longitude[2] = ((anchor_location[6] << 1) & 0xFE) |
				       ((anchor_location[7] >> 7) & 0x01);
			longitude[3] = ((anchor_location[7] << 1) & 0xFE) |
				       ((anchor_location[8] >> 7) & 0x01);
			longitude[4] = (anchor_location[8] >> 6) & 0x01;

			altitude[0] = ((anchor_location[8] << 2) & 0xFC) |
				      ((anchor_location[9] >> 6) & 0x03);
			altitude[1] = ((anchor_location[9] << 2) & 0xFC) |
				      ((anchor_location[10] >> 6) & 0x03);
			altitude[2] = ((anchor_location[10] << 2) & 0xFC) |
				      ((anchor_location[11] >> 6) & 0x03);
			altitude[3] = anchor_location[11] & 0x3F;

			meas->location.data.wgs84.latitude = qle64toh(temp[0]);
			meas->location.data.wgs84.longitude = qle64toh(temp[1]);
			meas->location.data.wgs84.altitude = qle64toh(temp[2]);

		} else if (((message_control >> 5) & 0x3) == 2) {
			uint8_t *relative_x = (uint8_t *)&temp[0];
			uint8_t *relative_y = (uint8_t *)&temp[1];
			uint8_t *relative_z = (uint8_t *)&temp[2];
			meas->location.type =
				CHERRY_FIRA_ANCHOR_LOCATION_TYPE_REL;
			uci_message_get(parser, (uint8_t *)anchor_location, 10);
			/* 28b for x coordinates / 28b for y coordinates / 24b z coordinates */
			relative_x[0] = anchor_location[0];
			relative_x[1] = anchor_location[1];
			relative_x[2] = anchor_location[2];
			relative_x[3] = (anchor_location[3] >> 4) & 0x0F;

			relative_y[0] = ((anchor_location[3] << 4) & 0xF0) |
					((anchor_location[4] >> 4) & 0x0F);
			relative_y[1] = ((anchor_location[4] << 4) & 0xF0) |
					((anchor_location[5] >> 4) & 0x0F);
			relative_y[2] = ((anchor_location[5] << 4) & 0xF0) |
					((anchor_location[6] >> 4) & 0x0F);
			relative_y[3] = anchor_location[6] & 0x0F;

			relative_z[0] = anchor_location[7];
			relative_z[1] = anchor_location[8];
			relative_z[2] = anchor_location[9];

			meas->location.data.relative.x = qle64toh(temp[0]);
			meas->location.data.relative.y = qle64toh(temp[1]);
			meas->location.data.relative.z = qle64toh(temp[2]);
		} else {
			meas->location.type =
				CHERRY_FIRA_ANCHOR_LOCATION_TYPE_NONE;
		}
		memset(meas->rr_indexes, 0, sizeof(meas->rr_indexes));
		uci_message_get(parser, meas->rr_indexes, meas->n_rr_indexes);
	}

	return QERR_SUCCESS;
}

enum qerr cherry_uci_client_parse_dltdoa_measurements_v2(
	const struct session_ranging_data *data,
	struct cherry_fira_session_dt_tag_ranging_report *dest)
{
	struct uci_message_parser *parser = data->parser;
	uint8_t anchor_location[FIRA_DL_TDOA_ANCHOR_LOCATION_SIZE_MAX];
	dest->sequence_number = data->sequence_number;
	dest->n_measurements = data->n_measurements;

	for (int i = 0; i < dest->n_measurements; i++) {
		struct cherry_fira_session_dt_tag_measurements *meas =
			&dest->measurements[i];

		uint16_t message_control;
		uint8_t len;

		uci_message_skip(parser, 2); /* meas_len not used here */
		meas->short_addr = uci_message_get_16bit(parser);
		meas->frame_status =
			cherry_session_frame_status_to_cherry_format(
				uci_message_get_8bit(parser));
		meas->msg_type =
			(enum cherry_fira_dltdoa_msg)uci_message_get_8bit(
				parser);
		meas->block_index = uci_message_get_16bit(parser);
		meas->round_index = uci_message_get_8bit(parser);
		meas->nlos = uci_message_get_8bit(parser);
		meas->aoa[CHERRY_AOA_AZIMUTH].aoa =
			uci_message_get_16bit(parser);
		meas->aoa[CHERRY_AOA_AZIMUTH].aoa_fom =
			uci_message_get_8bit(parser);
		meas->aoa[CHERRY_AOA_ELEVATION].aoa =
			uci_message_get_16bit(parser);
		meas->aoa[CHERRY_AOA_ELEVATION].aoa_fom =
			uci_message_get_8bit(parser);
		meas->rssi = uci_message_get_8bit(parser);
		meas->anchor_cfo = uci_message_get_16bit(parser);
		meas->cfo = uci_message_get_16bit(parser);
		meas->initiator_reply_time = uci_message_get_32bit(parser);
		meas->responder_reply_time = uci_message_get_32bit(parser);
		meas->tof = uci_message_get_16bit(parser);
		len = uci_message_get_8bit(parser);
		uci_message_get(parser, (uint8_t *)meas->rx_timestamp, len);
		message_control = uci_message_get_16bit(parser);
		len = uci_message_get_8bit(parser);
		uci_message_get(parser, (uint8_t *)meas->tx_timestamp, len);
		meas->common_time_base =
			(message_control & 0x1) ==
			FIRA_OWR_DTM_TIMESTAMP_COMMON_TIME_BASE; /* b0 */
		if (((message_control >> 1) & 0x1)) {
			meas->n_rr_indexes = uci_message_get_8bit(parser);
			memset(meas->rr_indexes, 0, sizeof(meas->rr_indexes));
			uci_message_get(parser, meas->rr_indexes,
					meas->n_rr_indexes);
		}
		if (((message_control >> 2) & 0x1)) {
			len = uci_message_get_8bit(parser);
			uci_message_get(parser, (uint8_t *)anchor_location,
					len);
			if (anchor_location[0] ==
			    FIRA_DT_LOCATION_COORD_WGS84) {
				parse_location_coor_wgs(meas, anchor_location);
			} else if (anchor_location[0] ==
				   FIRA_DT_LOCATION_COORD_RELATIVE) {
				parse_location_coor_rel(meas, anchor_location);
			} else if (anchor_location[0] ==
				   FIRA_DT_LOCATION_COORD_WGS84_WITH_Z) {
				parse_location_coor_wgs_z_ext(meas,
							      anchor_location);
			} else if (anchor_location[0] ==
				   FIRA_DT_LOCATION_COORD_RELATIVE_WITH_Z) {
				parse_location_coor_rel_z_ext(meas,
							      anchor_location);
			} else {
				meas->location.type =
					CHERRY_FIRA_ANCHOR_LOCATION_TYPE_NONE;
			}
		}
	}

	return QERR_SUCCESS;
}

static void free_tmp_cirs(struct tmp_cir *cir)
{
	struct tmp_cir *todel;
	while (cir != NULL) {
		todel = cir;
		cir = cir->next;
		if (todel->taps != NULL)
			qfree(todel->taps);
		qfree(todel);
	}
}

#define TLV_LENGTH_SEG_METRICS 17
#define TLV_LENGTH_AOA 8
#define TLV_FIXED_LENGTH_CIR 4

static enum qerr uci_rsp_diag_ntf_handler(struct uci *uci, uint16_t mt_gid_oid,
					  const struct uci_blk *payload,
					  void *user_data)
{
	struct cherry_fira_context *context = user_data;
	enum qerr ret = QERR_SUCCESS;
	struct diagnostic_info *diag;
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(payload);
	uint8_t nb_fields;
	uint8_t field;
	uint16_t field_size;

	if (!context->diag_notification_cb)
		return QERR_EINVAL;

	if (uci_message_remaining(&parser) < 9) {
		return QERR_EINVAL;
	}

	diag = (struct diagnostic_info *)qcalloc(
		1, sizeof(struct diagnostic_info));
	if (!diag) {
		return QERR_ENOMEM;
	}

	diag->session_handle = uci_message_get_32bit(&parser);
	diag->sequence_number = uci_message_get_32bit(&parser);
	diag->nb_reports = uci_message_get_8bit(&parser);

	diag->reports = (struct frame_report *)qcalloc(
		1, sizeof(struct frame_report) * diag->nb_reports);
	if (!diag->reports) {
		qfree(diag);
		return QERR_ENOMEM;
	}

	for (uint32_t i = 0; i < diag->nb_reports; i++) {
		struct frame_report *current = &diag->reports[i];

		/* Check size for minimum diag report data. */
		if (uci_message_remaining(&parser) < 4) {
			ret = QERR_EINVAL;
			goto free_data;
		}

		current->msg_id = uci_message_get_8bit(&parser);
		current->action = uci_message_get_8bit(&parser);
		current->antenna_set = uci_message_get_8bit(&parser);
		nb_fields = uci_message_get_8bit(&parser);
		for (int field_index = 0; field_index < nb_fields;
		     field_index++) {
			/* Check size for field and size. */
			if (uci_message_remaining(&parser) < 3) {
				ret = QERR_EINVAL;
				goto free_data;
			}
			field = uci_message_get_8bit(&parser);
			field_size = uci_message_get_16bit(&parser);

			/* Check size for field data. */
			if (uci_message_remaining(&parser) < field_size) {
				ret = QERR_EINVAL;
				goto free_data;
			}

			switch (field) {
			case UCI_DIAG_REPORT_AOAS:
				current->nb_aoa = field_size / TLV_LENGTH_AOA;
				current->aoas =
					(struct aoa_measurement *)qcalloc(
						1,
						sizeof(struct aoa_measurement) *
							current->nb_aoa);

				if (!current->aoas) {
					ret = QERR_ENOMEM;
					goto free_data;
				}

				for (int naoa = 0; naoa < current->nb_aoa;
				     naoa++) {
					struct aoa_measurement *cur_aoa =
						&current->aoas[naoa];
					cur_aoa->tdoa =
						uci_message_get_16bit(&parser);
					cur_aoa->pdoa =
						uci_message_get_16bit(&parser);
					cur_aoa->aoa =
						uci_message_get_16bit(&parser);
					cur_aoa->fom =
						uci_message_get_8bit(&parser);
					cur_aoa->type =
						uci_message_get_8bit(&parser);
				}
				break;
			case UCI_DIAG_REPORT_EXTRA_STATUS:
				current->extra_status =
					uci_message_get_16bit(&parser);
				current->extra_status_present = true;
				break;
			case UCI_DIAG_REPORT_CFO_Q26:
				current->cfo_q26 =
					uci_message_get_32bit(&parser);
				current->cfo_present = true;
				break;
			case UCI_DIAG_REPORT_EMITTER_SHORT_ADDR:
				current->emitter_short_addr =
					uci_message_get_16bit(&parser);
				current->emitter_short_addr_present = true;
				break;
			case UCI_DIAG_REPORT_SEGMENT_METRICS:
				current->nb_seg_metrics =
					field_size / TLV_LENGTH_SEG_METRICS;
				current->seg_metrics =
					(struct segment_metrics *)qcalloc(
						1,
						sizeof(struct segment_metrics) *
							current->nb_seg_metrics);

				if (!current->seg_metrics) {
					ret = QERR_ENOMEM;
					goto free_data;
				}

				for (int nmetrics = 0;
				     nmetrics < current->nb_seg_metrics;
				     nmetrics++) {
					struct segment_metrics
						*cur_segment_metrics =
							&current->seg_metrics
								 [nmetrics];
					cur_segment_metrics->receiver_segment =
						uci_message_get_8bit(&parser);
					cur_segment_metrics->noise_value =
						uci_message_get_16bit(&parser);
					cur_segment_metrics->rsl_q8 =
						uci_message_get_16bit(&parser);
					cur_segment_metrics->fp_index =
						uci_message_get_16bit(&parser);
					cur_segment_metrics->fp_rsl_q8 =
						uci_message_get_16bit(&parser);
					cur_segment_metrics->fp_ns_q6 =
						uci_message_get_16bit(&parser);
					cur_segment_metrics->pp_index =
						uci_message_get_16bit(&parser);
					cur_segment_metrics->pp_rsl_q8 =
						uci_message_get_16bit(&parser);
					cur_segment_metrics->pp_ns_q6 =
						uci_message_get_16bit(&parser);
				}
				break;
			case UCI_DIAG_REPORT_CIRS: {
				uint16_t remaining_size = field_size;
				uint8_t cir_number = 0;
				struct tmp_cir *head = NULL;
				struct tmp_cir *tmp;
				while (remaining_size != 0) {
					if (remaining_size <
					    TLV_FIXED_LENGTH_CIR) {
						uci_message_skip(
							&parser,
							remaining_size);
						goto free_tmp_cir_step;
					}
					cir_number++;
					remaining_size -= TLV_FIXED_LENGTH_CIR;
					tmp = head;
					head = (struct tmp_cir *)qcalloc(
						1, sizeof(struct tmp_cir));
					if (!head) {
						ret = QERR_ENOMEM;
						if (tmp)
							free_tmp_cirs(tmp);
						goto free_data;
					}
					head->next = tmp;
					head->receiver_segment =
						uci_message_get_8bit(&parser);
					head->fpath_tap_offset =
						uci_message_get_8bit(&parser);
					head->n_taps =
						uci_message_get_8bit(&parser);
					head->tap_size =
						uci_message_get_8bit(&parser);
					if (remaining_size <
					    (head->n_taps * head->tap_size)) {
						uci_message_skip(
							&parser,
							remaining_size);
						goto free_tmp_cir_step;
					}
					remaining_size -=
						(head->n_taps * head->tap_size);
					head->taps = qcalloc(
						(head->n_taps * head->tap_size),
						sizeof(uint8_t));
					if (!head->taps) {
						ret = QERR_ENOMEM;
						free_tmp_cirs(head);
						goto free_data;
					}
					uci_message_get(&parser, head->taps,
							(head->n_taps *
							 head->tap_size));
				}
				current->nb_cir = cir_number;
				tmp = head;
				current->cirs = (struct cir *)qcalloc(
					cir_number, sizeof(struct cir));
				if (!current->cirs) {
					ret = QERR_ENOMEM;
					free_tmp_cirs(head);
					goto free_data;
				}
				for (int cir_index = cir_number - 1;
				     cir_index >= 0; cir_index--) {
					current->cirs[cir_index]
						.receiver_segment =
						tmp->receiver_segment;
					current->cirs[cir_index]
						.fpath_tap_offset =
						tmp->fpath_tap_offset;
					current->cirs[cir_index].n_taps =
						tmp->n_taps;
					current->cirs[cir_index].tap_size =
						tmp->tap_size;
					current->cirs[cir_index].taps = qcalloc(
						(tmp->n_taps * tmp->tap_size),
						sizeof(uint8_t));
					memcpy(current->cirs[cir_index].taps,
					       tmp->taps,
					       (tmp->n_taps * tmp->tap_size) *
						       sizeof(uint8_t));
					tmp = tmp->next;
				}
			free_tmp_cir_step:
				free_tmp_cirs(head);
				break;
			}
			default:
				uci_message_skip(&parser, field_size);
				break;
			}
		}
	}

	context->diag_notification_cb(diag, (void *)context->user_data);
	return ret;

free_data:
	cherry_uci_client_fira_free_diag(diag);
	return ret;
}

void cherry_uci_client_fira_free_diag(struct diagnostic_info *diag)
{
	if (!diag)
		return;

	for (uint32_t i = 0; i < diag->nb_reports; i++) {
		struct frame_report *current = &diag->reports[i];
		if (current->seg_metrics)
			qfree(current->seg_metrics);
		if (current->aoas)
			qfree(current->aoas);
		if (current->cirs) {
			for (int cir_index = 0; cir_index < current->nb_cir;
			     cir_index++) {
				if (current->cirs[cir_index].taps)
					qfree(current->cirs[cir_index].taps);
			}
			qfree(current->cirs);
		}
	}
	qfree(diag->reports);
	qfree(diag);
}

static const struct uci_message_handler uci_qorvo_cmd_handlers[] = {
	{
		.mt_gid_oid = UCI_MT_GID_OID(
			UCI_MESSAGE_TYPE_NOTIFICATION, UCI_GID_QORVO_EXT2,
			UCI_OID_QORVO_FIRA_RANGE_DIAGNOSTICS),
		.handler = uci_rsp_diag_ntf_handler,
	},
};

static struct uci_message_handlers uci_rsp_qorvo_handlers_list = {
	.next = NULL,
	.handlers = uci_qorvo_cmd_handlers,
	.n_handlers = qarray_size(uci_qorvo_cmd_handlers),
	.user_data = NULL,
};

enum qerr cherry_uci_client_fira_open(
	struct cherry_fira_context **context, struct uci *uci, void *user_data,
	cherry_uci_client_fira_diag_notification_cb_t diag_cb)
{
	if (!diag_cb)
		return QERR_EINVAL;

	*context = (struct cherry_fira_context *)qmalloc(
		sizeof(struct cherry_fira_context));

	if (!context) {
		QLOGE("%s failed to allocate context", __func__);
		return QERR_EINVAL;
	}

	(*context)->uci = uci;
	(*context)->user_data = user_data;
	(*context)->diag_notification_cb = diag_cb;

	(*context)->sem_response = qsemaphore_init(0, 1);

	/* setup handlers */
	uci_rsp_session_config_handlers_list.user_data = (*context);
	uci_message_handlers_register((*context)->uci,
				      &uci_rsp_session_config_handlers_list);
	/* setup qorvo handlers */
	uci_rsp_qorvo_handlers_list.user_data = (*context);
	uci_message_handlers_register((*context)->uci,
				      &uci_rsp_qorvo_handlers_list);

	return QERR_SUCCESS;
}

void cherry_uci_client_fira_close(struct cherry_fira_context *context)
{
	if (!context)
		return;

	context->diag_notification_cb = NULL;
	context->user_data = NULL;

	qsemaphore_deinit(context->sem_response);

	uci_rsp_session_config_handlers_list.user_data = NULL;
	uci_message_handlers_unregister(context->uci,
					&uci_rsp_session_config_handlers_list);
	uci_rsp_qorvo_handlers_list.user_data = NULL;
	uci_message_handlers_unregister(context->uci,
					&uci_rsp_qorvo_handlers_list);
	qfree(context);
}

enum uci_status_code cherry_uci_client_fira_update_dt_anchor_ranging_rounds(
	struct cherry_fira_context *context, uint32_t session_id,
	uint8_t number_of_active_ranging_rounds, uint8_t *round_indexes,
	uint8_t *ranging_role, uint8_t *number_of_responders,
	uint16_t responder_address_list[][8],
	uint8_t *responder_slot_scheduling, uint8_t responder_slots[][8],
	uint8_t dl_tdoa_update_ranging_round_array
		[DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE],
	uint8_t *dl_tdoa_update_ranging_round_array_size)
{
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND, UCI_GID_SESSION_CONFIG,
			       UCI_OID_SESSION_UPDATE_DT_ANCHOR_RANGING_ROUNDS);
	struct uci_message_builder builder;
	uint8_t ret = 0;

	if (!context || number_of_active_ranging_rounds == 0 ||
	    number_of_active_ranging_rounds >= FIRA_DT_ANCHOR_MAX_ACTIVE_RR ||
	    !round_indexes || !ranging_role ||
	    !dl_tdoa_update_ranging_round_array ||
	    !dl_tdoa_update_ranging_round_array_size) {
		return UCI_STATUS_INVALID_PARAM;
	}

	uci_message_builder_init(&builder, context->uci);
	uci_message_put_32bit(&builder, session_id);
	uci_message_put_8bit(&builder, number_of_active_ranging_rounds);

	for (uint8_t i = 0; i < number_of_active_ranging_rounds; i++) {
		uci_message_put_8bit(&builder, round_indexes[i]);
		uci_message_put_8bit(&builder, ranging_role[i]);

		if (ranging_role[i] == UCI_DT_ANCHOR_INITIATOR) {
			if (!number_of_responders ||
			    number_of_responders[i] < 1 ||
			    number_of_responders[i] > 8 ||
			    !responder_address_list ||
			    !responder_slot_scheduling) {
				uci_blk_free_all(context->uci, builder.message);
				return UCI_STATUS_INVALID_PARAM;
			}

			uci_message_put_8bit(&builder, number_of_responders[i]);

			for (uint8_t j = 0; j < number_of_responders[i]; j++) {
				uci_message_put_16bit(
					&builder, responder_address_list[i][j]);
			}

			uci_message_put_8bit(&builder,
					     responder_slot_scheduling[i]);

			if (responder_slot_scheduling[i] ==
			    RESPONDER_SLOTS_PRESENT) {
				if (!responder_slots) {
					uci_blk_free_all(context->uci,
							 builder.message);
					return UCI_STATUS_INVALID_PARAM;
				}
				for (uint8_t j = 0; j < number_of_responders[i];
				     j++) {
					uci_message_put_8bit(
						&builder,
						responder_slots[i][j]);
				}
			}
		}
	}

	context->cmd_data = dl_tdoa_update_ranging_round_array;
	context->cmd_data_sz = DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE;

	/* Send message */
	uci_send_message(context->uci, mt_gid_oid, builder.message);
	/* Wait for response */
	ret = cherry_fira_wait_rsp(context);

	/* Gather response size*/

	if (ret)
		*dl_tdoa_update_ranging_round_array_size = context->cmd_data_sz;
	else
		*dl_tdoa_update_ranging_round_array_size = 0;
	return ret ? context->response_status : UCI_STATUS_UCI_MESSAGE_RETRY;
}

enum uci_status_code cherry_uci_client_fira_update_dt_tag_ranging_rounds(
	struct cherry_fira_context *context, uint32_t session_id,
	uint8_t number_of_active_ranging_rounds, uint8_t *ranging_round_indexes,
	uint8_t dl_tdoa_update_ranging_round_array
		[DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE],
	uint8_t *dl_tdoa_update_ranging_round_array_size)
{
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND, UCI_GID_SESSION_CONFIG,
			       UCI_OID_SESSION_UPDATE_DT_TAG_RANGING_ROUNDS);
	struct uci_message_builder builder;
	uint8_t ret;

	if (!context || number_of_active_ranging_rounds == 0 ||
	    number_of_active_ranging_rounds >= FIRA_DT_TAG_MAX_ACTIVE_RR ||
	    !ranging_round_indexes || !dl_tdoa_update_ranging_round_array ||
	    !dl_tdoa_update_ranging_round_array_size) {
		return UCI_STATUS_INVALID_PARAM;
	}

	uci_message_builder_init(&builder, context->uci);
	uci_message_put_32bit(&builder, session_id);
	uci_message_put_8bit(&builder, number_of_active_ranging_rounds);
	for (uint8_t i = 0; i < number_of_active_ranging_rounds; i++) {
		uci_message_put_8bit(&builder, ranging_round_indexes[i]);
	}

	context->cmd_data = dl_tdoa_update_ranging_round_array;
	context->cmd_data_sz = DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE;

	/* Send message */
	uci_send_message(context->uci, mt_gid_oid, builder.message);
	/* Wait for response */
	ret = cherry_fira_wait_rsp(context);

	/* Gather response size*/
	if (ret)
		*dl_tdoa_update_ranging_round_array_size = context->cmd_data_sz;
	else
		*dl_tdoa_update_ranging_round_array_size = 0;
	return ret ? context->response_status : UCI_STATUS_UCI_MESSAGE_RETRY;
}
