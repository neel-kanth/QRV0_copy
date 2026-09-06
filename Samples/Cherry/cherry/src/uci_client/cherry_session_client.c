/*
 * Implementation for session client functions
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "cherry_session_client.h"

#define LOG_TAG "cherry_uci_client"
#include "cherry_log.h"

#include <qmalloc.h>
#include <qsemaphore.h>
#include <qtils.h>
#include <stdio.h>
#include <uci/uci_message.h>
#include <uci/uci_unit_converter.h>

#define INFO_NTF_HEADER_SIZE 25

struct cherry_session_context {
	struct uci *uci;
	cherry_uci_client_session_status_cb_t session_status_cb;
	cherry_uci_client_session_ranging_ntf_cb_t ranging_ntf_cb;
	void *user_data;
	int response_status;
	struct qsemaphore *sem_response;
	void *cmd_data;
	uint32_t session_handle;
};

static inline int
cherry_session_wait_rsp(struct cherry_session_context *context)
{
	int r = qsemaphore_take(context->sem_response, 1000);

	/* Error or timeout. */
	if (r)
		return 0;

	/* Completed in time. */
	return 1;
}

static inline void
cherry_session_set_rsp(struct cherry_session_context *context)
{
	qsemaphore_give(context->sem_response);
}

uint32_t
session_status_ntf_get_session_handle(const struct session_status_ntf *ntf)
{
	return ntf->session_handle;
}

enum uci_session_state
session_status_ntf_get_session_state(const struct session_status_ntf *ntf)
{
	return ntf->session_state;
}

enum uci_session_reason_code
session_status_ntf_get_reason_code(const struct session_status_ntf *ntf)
{
	return ntf->reason_code;
}

static enum uci_status_code parse_status(struct uci_message_parser *parser)
{
	/* check that we have a reset config value */
	if (uci_message_remaining(parser) < 1) {
		return UCI_STATUS_INVALID_MESSAGE_SIZE;
	}

	return uci_message_get_8bit(parser);
}

void cherry_uci_client_session_free_status_ntf(struct session_status_ntf *ntf)
{
	if (ntf)
		qfree(ntf);
}

static enum qerr uci_session_ntf_handler(struct uci *uci, uint16_t mt_gid_oid,
					 const struct uci_blk *payload,
					 void *user_data)
{
	struct cherry_session_context *context = user_data;
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(payload);
	struct session_status_ntf *ntf;

	if (uci_message_remaining(&parser) < 6) {
		return QERR_EINVAL;
	}

	ntf = (struct session_status_ntf *)qcalloc(
		1, sizeof(struct session_status_ntf));
	if (ntf) {
		ntf->session_handle = uci_message_get_32bit(&parser);
		ntf->session_state = uci_message_get_8bit(&parser);
		ntf->reason_code = uci_message_get_8bit(&parser);
		context->session_status_cb(ntf, context->user_data);
	} else
		QLOGE("%s: Unable to allocate data.", __func__);

	return QERR_SUCCESS;
}

static enum qerr uci_session_init_rsp_handler(struct uci *uci,
					      uint16_t mt_gid_oid,
					      const struct uci_blk *payload,
					      void *user_data)
{
	struct cherry_session_context *context = user_data;
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(payload);

	if (uci_message_remaining(&parser) < 1)
		context->response_status = UCI_STATUS_INVALID_MESSAGE_SIZE;

	context->response_status = uci_message_get_8bit(&parser);

	if (!uci_message_get_32bit_no_assert(&parser, &context->session_handle))
		context->session_handle = 0;

	/* something to unlock the waiting */
	cherry_session_set_rsp(context);

	return QERR_SUCCESS;
}

/* Default response handler for command only returning a status. */
static enum qerr uci_rsp_handler_set_app_config(struct uci *uci,
						uint16_t mt_gid_oid,
						const struct uci_blk *payload,
						void *user_data)
{
	struct cherry_session_context *context = user_data;
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(payload);
	uint8_t num_app;

	context->response_status = parse_status(&parser);
	if (uci_message_remaining(&parser) < 1) {
		cherry_session_set_rsp(context);
		return QERR_SUCCESS;
	}
	num_app = uci_message_get_8bit(&parser);
	if (num_app != 0) {
		context->response_status = UCI_STATUS_FAILED;
	}

	/* something to unlock the waiting */
	cherry_session_set_rsp(context);
	return QERR_SUCCESS;
}

static enum qerr uci_rsp_handler(struct uci *uci, uint16_t mt_gid_oid,
				 const struct uci_blk *payload, void *user_data)
{
	struct cherry_session_context *context = user_data;
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(payload);

	context->response_status = parse_status(&parser);
	/* something to unlock the waiting */
	cherry_session_set_rsp(context);
	return QERR_SUCCESS;
}

typedef enum uci_status_code (*cherry_uci_client_session_get_app_config_handler)(
	struct uci_message_parser *parser, void *data, uint8_t len);

struct cherry_uci_client_session_get_app_config_param {
	uint16_t param_type;
	uint8_t maxlen;
	void *data;
	cherry_uci_client_session_get_app_config_handler handler;
};

static enum qerr uci_rsp_handler_get_app_config(struct uci *uci,
						uint16_t mt_gid_oid,
						const struct uci_blk *payload,
						void *user_data)
{
	struct cherry_session_context *context = user_data;
	struct cherry_uci_client_session_get_app_config_param *config_param =
		context->cmd_data;
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(payload);
	uint8_t num_app;
	uint16_t param_type;
	uint8_t param_len;

	context->response_status = parse_status(&parser);

	if (uci_message_remaining(&parser) < 3)
		goto err;

	num_app = uci_message_get_8bit(&parser);
	if (num_app != 1) {
		goto err;
	}
	/* Get parameter type and check if correspond to expected one. */
	param_type = uci_message_get_8bit(&parser);
	if (param_type >= 0xE0 && param_type <= 0xE2) {
		if (uci_message_remaining(&parser) < 2)
			goto err;

		param_type = param_type << 8 | uci_message_get_8bit(&parser);
	}
	if (config_param->param_type != param_type)
		goto err;

	/* Check if parameter length can store received data. */
	param_len = uci_message_get_8bit(&parser);
	if (param_len > config_param->maxlen ||
	    uci_message_remaining(&parser) < param_len)
		context->response_status = UCI_STATUS_INVALID_MESSAGE_SIZE;
	else
		context->response_status = config_param->handler(
			&parser, config_param->data, param_len);
	cherry_session_set_rsp(context);
	return QERR_SUCCESS;

err:
	context->response_status = UCI_STATUS_INVALID_MESSAGE_SIZE;
	cherry_session_set_rsp(context);
	return QERR_EINVAL;
}

static enum qerr uci_rsp_handler_get_count_state(struct uci *uci,
						 uint16_t mt_gid_oid,
						 const struct uci_blk *payload,
						 void *user_data)
{
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(payload);
	struct cherry_session_context *context = user_data;
	int value_app;

	if (uci_message_remaining(&parser) < 2) {
		cherry_session_set_rsp(context);
		return QERR_EINVAL;
	}

	context->response_status = parse_status(&parser);
	value_app = uci_message_get_8bit(&parser);
	memcpy(context->cmd_data, &value_app, sizeof(int));
	cherry_session_set_rsp(context);

	return QERR_SUCCESS;
}

static enum qerr uci_rsp_range_data_ntf_handler(struct uci *uci,
						uint16_t mt_gid_oid,
						const struct uci_blk *payload,
						void *user_data)
{
	struct cherry_session_context *context = user_data;
	struct session_ranging_data data;
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(payload);

	if (uci_message_remaining(&parser) < INFO_NTF_HEADER_SIZE)
		return QERR_EINVAL;

	data.sequence_number = uci_message_get_32bit(&parser);
	data.session_handle = uci_message_get_32bit(&parser);
	uci_message_skip(&parser, 1); /* RFU. */
	data.ranging_interval_ms = uci_message_get_32bit(&parser);
	data.type = uci_message_get_8bit(&parser);

	uci_message_skip(&parser, 1); // skip RFU
	uci_message_skip(&parser, 1); // skip MAC Addressing mode indicator
	uci_message_skip(&parser, 4); // skip Session Handle of primary session
	uci_message_skip(&parser, 4); // skip RFU
	data.n_measurements = uci_message_get_8bit(&parser);

	data.parser = &parser;
	data.user_data = context->user_data;

	context->ranging_ntf_cb(&data);

	return QERR_SUCCESS;
}

/* Handlers in table need to have their OID's ordered ascending. */
static const struct uci_message_handler uci_rsp_session_config_handlers[] = {
	{
		.mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_INIT),
		.handler = uci_session_init_rsp_handler,
	},
	{
		.mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_DEINIT),
		.handler = uci_rsp_handler,
	},
	{
		.mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_NOTIFICATION,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_STATUS),
		.handler = uci_session_ntf_handler,
	},
	{
		.mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_SET_APP_CONFIG),
		.handler = uci_rsp_handler_set_app_config,
	},
	{
		.mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_APP_CONFIG),
		.handler = uci_rsp_handler_get_app_config,
	},
	{
		.mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_COUNT),
		.handler = uci_rsp_handler_get_count_state,
	},
	{
		.mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_STATE),
		.handler = uci_rsp_handler_get_count_state,
	},
};

static struct uci_message_handlers uci_rsp_session_config_handlers_list = {
	.next = NULL,
	.handlers = uci_rsp_session_config_handlers,
	.n_handlers = qarray_size(uci_rsp_session_config_handlers),
	.user_data = NULL,
};

/* Handlers in table need to have their OID's ordered ascending. */
static const struct uci_message_handler uci_rsp_session_control_handlers[] = {
	{
		.mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONTROL,
					     UCI_OID_SESSION_INFO),
		.handler = uci_rsp_handler,
	},
	{
		.mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_NOTIFICATION,
					     UCI_GID_SESSION_CONTROL,
					     UCI_OID_SESSION_INFO),
		.handler = uci_rsp_range_data_ntf_handler,
	},
	{
		.mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_SESSION_CONTROL,
					     UCI_OID_SESSION_STOP),
		.handler = uci_rsp_handler,
	},
};

static struct uci_message_handlers uci_rsp_session_control_handlers_list = {
	.next = NULL,
	.handlers = uci_rsp_session_control_handlers,
	.n_handlers = qarray_size(uci_rsp_session_control_handlers),
	.user_data = NULL,
};

enum qerr cherry_uci_client_session_open(
	struct cherry_session_context **context, struct uci *uci,
	void *user_data,
	cherry_uci_client_session_status_cb_t session_status_cb,
	cherry_uci_client_session_ranging_ntf_cb_t ranging_ntf_cb)
{
	if (!session_status_cb || !ranging_ntf_cb)
		return QERR_EINVAL;

	*context = (struct cherry_session_context *)qmalloc(
		sizeof(struct cherry_session_context));

	(*context)->uci = uci;
	(*context)->user_data = user_data;
	(*context)->session_status_cb = session_status_cb;
	(*context)->ranging_ntf_cb = ranging_ntf_cb;

	(*context)->sem_response = qsemaphore_init(0, 1);

	/* setup handlers */
	uci_rsp_session_config_handlers_list.user_data = (*context);
	uci_rsp_session_control_handlers_list.user_data = (*context);
	uci_message_handlers_register((*context)->uci,
				      &uci_rsp_session_config_handlers_list);
	uci_message_handlers_register((*context)->uci,
				      &uci_rsp_session_control_handlers_list);

	return QERR_SUCCESS;
}

enum uci_status_code cherry_uci_client_session_init_session(
	struct cherry_session_context *context, uint32_t session_id,
	uint8_t session_type, uint32_t *session_handle)
{
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_INIT);
	struct uci_message_builder builder;
	int ret;

	if (!context) {
		return UCI_STATUS_INVALID_PARAM;
	}

	uci_message_builder_init(&builder, context->uci);
	uci_message_put_32bit(&builder, session_id);
	uci_message_put_8bit(&builder, session_type);
	/* Send message */
	uci_send_message(context->uci, mt_gid_oid, builder.message);
	/* Wait for response */
	ret = cherry_session_wait_rsp(context);

	/* Get session handler from context */
	*session_handle = context->session_handle;

	return ret ? context->response_status : UCI_STATUS_UCI_MESSAGE_RETRY;
}

enum uci_status_code
cherry_uci_client_session_deinit_session(struct cherry_session_context *context,
					 uint32_t session_handle)
{
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_DEINIT);
	struct uci_message_builder builder;
	int ret;

	if (!context) {
		return UCI_STATUS_INVALID_PARAM;
	}

	uci_message_builder_init(&builder, context->uci);
	uci_message_put_32bit(&builder, session_handle);
	/* Send message */
	uci_send_message(context->uci, mt_gid_oid, builder.message);
	/* Wait for response */
	ret = cherry_session_wait_rsp(context);

	return ret ? context->response_status : UCI_STATUS_UCI_MESSAGE_RETRY;
}

void cherry_uci_client_session_close(struct cherry_session_context *context)
{
	if (!context)
		return;

	context->session_status_cb = NULL;
	context->ranging_ntf_cb = NULL;
	context->user_data = NULL;

	qsemaphore_deinit(context->sem_response);

	uci_rsp_session_config_handlers_list.user_data = NULL;
	uci_rsp_session_control_handlers_list.user_data = NULL;
	uci_message_handlers_unregister(context->uci,
					&uci_rsp_session_config_handlers_list);
	uci_message_handlers_unregister(context->uci,
					&uci_rsp_session_control_handlers_list);

	qfree(context);
}

enum uci_status_code
cherry_uci_client_session_start_session(struct cherry_session_context *context,
					uint32_t session_handle)
{
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND,
					     UCI_GID_SESSION_CONTROL,
					     UCI_OID_SESSION_INFO);
	struct uci_message_builder builder;
	int ret;

	if (!context) {
		return UCI_STATUS_INVALID_PARAM;
	}

	uci_message_builder_init(&builder, context->uci);
	uci_message_put_32bit(&builder, session_handle);
	/* Send message */
	uci_send_message(context->uci, mt_gid_oid, builder.message);
	/* Wait for response */
	ret = cherry_session_wait_rsp(context);

	return ret ? context->response_status : UCI_STATUS_UCI_MESSAGE_RETRY;
}

enum uci_status_code
cherry_uci_client_session_stop_session(struct cherry_session_context *context,
				       uint32_t session_handle)
{
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND,
					     UCI_GID_SESSION_CONTROL,
					     UCI_OID_SESSION_STOP);
	struct uci_message_builder builder;
	int ret;

	if (!context) {
		return UCI_STATUS_INVALID_PARAM;
	}

	uci_message_builder_init(&builder, context->uci);
	uci_message_put_32bit(&builder, session_handle);
	/* Send message */
	uci_send_message(context->uci, mt_gid_oid, builder.message);
	/* Wait for response */
	ret = cherry_session_wait_rsp(context);

	return ret ? context->response_status : UCI_STATUS_UCI_MESSAGE_RETRY;
}

static enum uci_status_code cherry_uci_client_session_get_app_config(
	struct cherry_session_context *context, uint32_t session_handle,
	uint16_t param, void *data, uint8_t maxlen,
	cherry_uci_client_session_get_app_config_handler handler)
{
	const uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND, UCI_GID_SESSION_CONFIG,
			       UCI_OID_SESSION_GET_APP_CONFIG);
	struct uci_message_builder builder;
	uint8_t no_of_app = 0x01;
	int ret;
	struct cherry_uci_client_session_get_app_config_param config_param = {
		.param_type = param,
		.maxlen = maxlen,
		.data = data,
		.handler = handler
	};

	if (!context || !data || maxlen == 0 || !handler) {
		return UCI_STATUS_INVALID_PARAM;
	}

	uci_message_builder_init(&builder, context->uci);
	uci_message_put_32bit(&builder, session_handle);
	uci_message_put_8bit(&builder, no_of_app);
	uci_message_put_8bit(&builder, param);
	context->cmd_data = &config_param;
	/* Send message */
	uci_send_message(context->uci, mt_gid_oid, builder.message);
	/* Wait for response */
	ret = cherry_session_wait_rsp(context);

	return ret ? context->response_status : UCI_STATUS_UCI_MESSAGE_RETRY;
}

static enum uci_status_code
cherry_uci_client_session_get_app_config_raw_handler(
	struct uci_message_parser *parser, void *data, uint8_t param_len)
{
	uci_message_get(parser, data, param_len);
	return UCI_STATUS_OK;
}

static enum uci_status_code
cherry_uci_client_session_get_app_config_uint8_handler(
	struct uci_message_parser *parser, void *data, uint8_t param_len)
{
	if (param_len != sizeof(uint8_t))
		return UCI_STATUS_INVALID_MESSAGE_SIZE;

	*((uint8_t *)data) = uci_message_get_8bit(parser);
	return UCI_STATUS_OK;
}

static enum uci_status_code
cherry_uci_client_session_get_app_config_uint16_handler(
	struct uci_message_parser *parser, void *data, uint8_t param_len)
{
	if (param_len != sizeof(uint16_t))
		return UCI_STATUS_INVALID_MESSAGE_SIZE;

	*((uint16_t *)data) = uci_message_get_16bit(parser);
	return UCI_STATUS_OK;
}

enum uci_status_code cherry_uci_client_session_get_app_config_preamble_duration(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *preamble_duration)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_PREAMBLE_DURATION, preamble_duration,
		sizeof(*preamble_duration),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code cherry_uci_client_session_get_app_config_no_of_controlees(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *no_of_controlees)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_NUMBER_OF_CONTROLEES,
		no_of_controlees, sizeof(*no_of_controlees),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code
cherry_uci_client_session_get_app_config_device_mac_address(
	struct cherry_session_context *context, uint32_t session_handle,
	uint16_t *device_mac_address)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_DEVICE_MAC_ADDRESS,
		device_mac_address, sizeof(*device_mac_address),
		cherry_uci_client_session_get_app_config_uint16_handler);
}

enum uci_status_code
cherry_uci_client_session_get_app_config_number_of_sts_segments(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *sts_segment)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_NUMBER_OF_STS_SEGMENTS, sts_segment,
		sizeof(*sts_segment),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code cherry_uci_client_session_get_app_config_short_sts_config(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *sts_config)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle, UCI_APPLICATION_PARAMETER_STS_CONFIG,
		sts_config, sizeof(*sts_config),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code cherry_uci_client_session_get_app_config_sts_length(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *sts_length)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle, UCI_APPLICATION_PARAMETER_STS_LENGTH,
		sts_length, sizeof(*sts_length),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code
cherry_uci_client_session_get_app_config_dest_mac_address_handler(
	struct uci_message_parser *parser, void *data, uint8_t param_len)
{
	struct dst_mac_addresses *addr = data;

	if ((param_len / 2) > FIRA_CONTROLEES_MAX)
		return UCI_STATUS_INVALID_MESSAGE_SIZE;

	addr->n_addresses = param_len / 2;
	if (uci_message_remaining(parser) != param_len)
		return UCI_STATUS_INVALID_MESSAGE_SIZE;

	for (int i = 0; i < addr->n_addresses; i++) {
		addr->addresses[i] = uci_message_get_16bit(parser);
	}
	return UCI_STATUS_OK;
}

enum uci_status_code cherry_uci_client_session_get_app_config_dest_mac_address(
	struct cherry_session_context *context, uint32_t session_handle,
	struct dst_mac_addresses *dst_mac_address)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_DST_MAC_ADDRESS, dst_mac_address,
		2 * FIRA_CONTROLEES_MAX,
		cherry_uci_client_session_get_app_config_dest_mac_address_handler);
}

enum uci_status_code cherry_uci_client_session_get_app_config_info_ntf_config(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *session_info_ntf_config)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_SESSION_INFO_NTF_CONFIG,
		session_info_ntf_config, sizeof(*session_info_ntf_config),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code cherry_uci_client_session_get_app_config_channel_number(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *channel_number)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_CHANNEL_NUMBER, channel_number,
		sizeof(*channel_number),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code
cherry_uci_client_session_get_app_config_preamble_code_index(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *preamble_code_index)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_PREAMBLE_CODE_INDEX,
		preamble_code_index, sizeof(*preamble_code_index),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code cherry_uci_client_session_get_app_config_report_rssi(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *report_rssi)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_RSSI_REPORTING, report_rssi,
		sizeof(*report_rssi),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code
cherry_uci_client_session_get_app_config_max_number_of_measurements(
	struct cherry_session_context *context, uint32_t session_handle,
	uint16_t *max_number_of_measurements)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_MAX_NUMBER_OF_MEASUREMENTS,
		max_number_of_measurements, sizeof(*max_number_of_measurements),
		cherry_uci_client_session_get_app_config_uint16_handler);
}

enum uci_status_code cherry_uci_client_session_get_app_config_max_rr_retry(
	struct cherry_session_context *context, uint32_t session_handle,
	uint16_t *max_rr_retry)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle, UCI_APPLICATION_PARAMETER_MAX_RR_RETRY,
		max_rr_retry, sizeof(*max_rr_retry),
		cherry_uci_client_session_get_app_config_uint16_handler);
}

enum uci_status_code cherry_uci_client_session_get_app_config_psdu_data_rate(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *psdu_data_rate)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_PSDU_DATA_RATE, psdu_data_rate,
		sizeof(*psdu_data_rate),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code cherry_uci_client_session_get_app_config_rframe_config(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *rframe_config)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_RFRAME_CONFIG, rframe_config,
		sizeof(*rframe_config),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code cherry_uci_client_session_get_app_config_prf_mode(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *prf_mode)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle, UCI_APPLICATION_PARAMETER_PRF_MODE,
		prf_mode, sizeof(*prf_mode),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code cherry_uci_client_session_get_app_config_sfd_id(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *sfd_id)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle, UCI_APPLICATION_PARAMETER_SFD_ID,
		sfd_id, sizeof(*sfd_id),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

struct cherry_uci_client_session_set_app_config_cmd {
	struct cherry_session_context *context;
	struct uci_message_builder builder;
	/* session_handle is a void pointer as not aligned. Use memcpy to update it. */
	void *session_handle;
	uint8_t *nb_app_config;
};

struct cherry_uci_client_session_set_app_config_cmd *
cherry_uci_client_session_set_app_config_cmd_create(
	struct cherry_session_context *context)
{
	struct cherry_uci_client_session_set_app_config_cmd *cmd;
	struct uci_message_builder *builder;

	if (!context) {
		QLOGE("%s: no context", __func__);
		return NULL;
	}

	cmd = qmalloc(sizeof(*cmd));
	if (!cmd) {
		QLOGE("%s: qmalloc failed", __func__);
		return NULL;
	}

	cmd->context = context;
	builder = &cmd->builder;
	uci_message_builder_init(builder, context->uci);
	cmd->session_handle = uci_message_reserve_32bit(builder, -1);
	if (!cmd->session_handle) {
		QLOGE("%s: reserve session handle", __func__);
		goto failed;
	}
	cmd->nb_app_config = uci_message_reserve_8bit(builder, 0);
	if (!cmd->nb_app_config) {
		QLOGE("%s: reserve nb app config", __func__);
		goto failed;
	}

	return cmd;

failed:
	qfree(cmd);
	return NULL;
}

enum uci_status_code cherry_uci_client_session_set_app_config_cmd_put(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint16_t param_id, const void *data, uint8_t size)
{
	struct uci_message_builder *builder;
	int ret;

	if (!cmd || !data || size == 0)
		return UCI_STATUS_INVALID_PARAM;

	/* Check parameter ID range */
	if (param_id > 0xFF && (param_id < 0xE000 || param_id > 0xE200))
		return UCI_STATUS_INVALID_PARAM;

	builder = &cmd->builder;

	if (param_id <= 0xFF)
		ret = uci_message_put_8bit(builder, param_id);
	else
		ret = uci_message_put_16bit(builder, param_id);

	if (ret)
		return UCI_STATUS_FAILED;

	ret = uci_message_put_8bit(builder, size);
	if (ret)
		return UCI_STATUS_FAILED;
	ret = uci_message_put(builder, data, size);
	if (ret)
		return UCI_STATUS_FAILED;

	++(*cmd->nb_app_config);
	return UCI_STATUS_OK;
}

static void cherry_uci_client_session_set_app_config_cmd_destroy(
	struct cherry_uci_client_session_set_app_config_cmd *cmd)
{
	if (!cmd)
		return;

	uci_blk_free_all(cmd->context->uci, cmd->builder.message);
	qfree(cmd);
}

enum uci_status_code cherry_uci_client_session_set_app_config_cmd_send(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint32_t session_handle)
{
	const uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND, UCI_GID_SESSION_CONFIG,
			       UCI_OID_SESSION_SET_APP_CONFIG);
	int ret;
	struct cherry_session_context *context;

	if (!cmd)
		return UCI_STATUS_INVALID_PARAM;

	/* No configuration set, just abort the command and fake success return code. */
	if (*cmd->nb_app_config == 0) {
		cherry_uci_client_session_set_app_config_cmd_abort(cmd);
		return UCI_STATUS_OK;
	}

	context = cmd->context;
	memcpy(cmd->session_handle, &session_handle, sizeof(session_handle));

	/* Send message */
	uci_send_message(context->uci, mt_gid_oid, cmd->builder.message);
	cmd->builder.message = NULL;
	/* Wait for response */
	ret = cherry_session_wait_rsp(context);

	cherry_uci_client_session_set_app_config_cmd_destroy(cmd);

	return ret ? context->response_status : UCI_STATUS_UCI_MESSAGE_RETRY;
}

void cherry_uci_client_session_set_app_config_cmd_abort(
	struct cherry_uci_client_session_set_app_config_cmd *cmd)
{
	cherry_uci_client_session_set_app_config_cmd_destroy(cmd);
}

enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_session_time_base(
	struct cherry_uci_client_session_set_app_config_cmd *cmd, bool enable,
	bool continue_session, bool resync, uint32_t session_handle,
	uint32_t offset_us)
{
	int ret;
	uint8_t flags = 0;
	struct uci_message_builder *builder;

	if (!cmd)
		return UCI_STATUS_INVALID_PARAM;

	if (enable)
		flags |= 0x1;
	if (continue_session)
		flags |= 0x2;
	if (resync)
		flags |= 0x4;
	builder = &cmd->builder;
	++(*cmd->nb_app_config);
	ret = uci_message_put_8bit(builder,
				   UCI_APPLICATION_PARAMETER_SESSION_TIME_BASE);
	if (ret)
		return UCI_STATUS_FAILED;
	ret = uci_message_put_8bit(builder, 9);
	if (ret)
		return UCI_STATUS_FAILED;
	ret = uci_message_put_8bit(builder, flags);
	if (ret)
		return UCI_STATUS_FAILED;
	ret = uci_message_put_32bit(builder, session_handle);
	if (ret)
		return UCI_STATUS_FAILED;
	ret = uci_message_put_32bit(builder, offset_us);
	if (ret)
		return UCI_STATUS_FAILED;
	return UCI_STATUS_OK;
}

enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_dest_mac_address(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	const struct dst_mac_addresses *dest_mac_add)
{
	enum uci_status_code st;
	int ret;
	struct uci_message_builder *builder;

	if (!cmd || !dest_mac_add)
		return UCI_STATUS_INVALID_PARAM;

	st = cherry_uci_client_session_set_app_config_cmd_put_no_of_controlees(
		cmd, dest_mac_add->n_addresses);
	if (st != UCI_STATUS_OK)
		return st;

	builder = &cmd->builder;
	++(*cmd->nb_app_config);
	ret = uci_message_put_8bit(builder,
				   UCI_APPLICATION_PARAMETER_DST_MAC_ADDRESS);
	if (ret)
		return UCI_STATUS_FAILED;
	ret = uci_message_put_8bit(builder, sizeof(dest_mac_add->addresses[0]) *
						    dest_mac_add->n_addresses);
	if (ret)
		return UCI_STATUS_FAILED;

	for (int i = 0; i < dest_mac_add->n_addresses; ++i) {
		ret = uci_message_put_16bit(builder,
					    dest_mac_add->addresses[i]);
		if (ret)
			return UCI_STATUS_FAILED;
	}
	return UCI_STATUS_OK;
}

enum uci_status_code cherry_uci_client_session_get_app_config_vendor_id(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t vendor_id[VENDOR_ID_SIZE])
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle, UCI_APPLICATION_PARAMETER_VENDOR_ID,
		vendor_id, 2,
		cherry_uci_client_session_get_app_config_raw_handler);
}

enum uci_status_code cherry_uci_client_session_get_app_config_static_sts_IV(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t static_sts_IV[STATIC_STS_IV_SIZE])
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_STATIC_STS_IV, static_sts_IV,
		STATIC_STS_IV_SIZE,
		cherry_uci_client_session_get_app_config_raw_handler);
}

enum uci_status_code cherry_uci_client_session_get_app_config_device_type(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *device_type)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle, UCI_APPLICATION_PARAMETER_DEVICE_TYPE,
		device_type, sizeof(*device_type),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code cherry_uci_client_session_get_app_config_device_role(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *device_role)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle, UCI_APPLICATION_PARAMETER_DEVICE_ROLE,
		device_role, sizeof(*device_role),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code
cherry_uci_client_session_get_app_config_ranging_round_usage(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *ranging_round_usage)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_RANGING_ROUND_USAGE,
		ranging_round_usage, sizeof(*ranging_round_usage),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code cherry_uci_client_session_get_app_config_multi_node_mode(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *multi_node_mode)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_MULTI_NODE_MODE, multi_node_mode,
		sizeof(*multi_node_mode),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code cherry_uci_client_session_get_app_config_schedule_mode(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *schedule_mode)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_SCHEDULE_MODE, schedule_mode,
		sizeof(*schedule_mode),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code
cherry_uci_client_session_get_app_config_data_repetition_count(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *data_repetition_count)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_DATA_REPETITION_COUNT,
		data_repetition_count, sizeof(*data_repetition_count),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code
cherry_uci_client_session_get_app_config_dl_tdoa_ranging_method(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *dl_tdoa_ranging_method)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_DL_TDOA_RANGING_METHOD,
		dl_tdoa_ranging_method, sizeof(*dl_tdoa_ranging_method),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code
cherry_uci_client_session_get_app_config_dl_tdoa_tx_timestamp_conf(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *dl_tdoa_tx_timestamp_conf)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_DL_TDOA_TX_TIMESTAMP_CONF,
		dl_tdoa_tx_timestamp_conf, sizeof(*dl_tdoa_tx_timestamp_conf),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code cherry_uci_client_session_get_app_config_dl_tdoa_hop_count(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *dl_tdoa_hop_count)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_DL_TDOA_HOP_COUNT, dl_tdoa_hop_count,
		sizeof(*dl_tdoa_hop_count),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code
cherry_uci_client_session_get_app_config_dl_tdoa_anchor_cfo(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *dl_tdoa_anchor_cfo)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_DL_TDOA_ANCHOR_CFO,
		dl_tdoa_anchor_cfo, sizeof(*dl_tdoa_anchor_cfo),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code
cherry_uci_client_session_get_app_config_dl_tx_active_ranging_rounds(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *dl_tdoa_tx_active_ranging_rounds)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_DL_TDOA_TX_ACTIVE_RANGING_ROUNDS,
		dl_tdoa_tx_active_ranging_rounds,
		sizeof(*dl_tdoa_tx_active_ranging_rounds),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code
cherry_uci_client_session_get_app_config_dl_tdoa_block_skipping(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *dl_tdoa_block_skipping)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_DL_TDOA_BLOCK_SKIPPING,
		dl_tdoa_block_skipping, sizeof(*dl_tdoa_block_skipping),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code
cherry_uci_client_session_get_app_config_dl_tdoa_time_reference_anchor(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *dl_tdoa_time_reference_anchor)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_DL_TDOA_TIME_REFERENCE_ANCHOR,
		dl_tdoa_time_reference_anchor,
		sizeof(*dl_tdoa_time_reference_anchor),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code
cherry_uci_client_session_get_app_config_dl_tdoa_responder_tof(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *dl_tdoa_responder_tof)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_DL_TDOA_RESPONDER_TOF,
		dl_tdoa_responder_tof, sizeof(*dl_tdoa_responder_tof),
		cherry_uci_client_session_get_app_config_uint8_handler);
}

enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_location(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	const struct cherry_fira_anchor_location *dl_tdoa_anchor_location)
{
	uint8_t location[FIRA_DL_TDOA_ANCHOR_LOCATION_SIZE_MAX];
	uint8_t length = 1;
	uint8_t *loc_x, *loc_y, *loc_z;
	uint64_t temp[3];
	uint16_t floor_val;
	uint32_t height_val;
	int ret;

	if (!dl_tdoa_anchor_location)
		return UCI_STATUS_INVALID_PARAM;
	memset(location, 0, FIRA_DL_TDOA_ANCHOR_LOCATION_SIZE_MAX);

	location[0] = dl_tdoa_anchor_location->type;

	switch (dl_tdoa_anchor_location->type) {
	case CHERRY_FIRA_ANCHOR_LOCATION_TYPE_NONE:
		break;
	case CHERRY_FIRA_ANCHOR_LOCATION_TYPE_WGS84:
		temp[0] =
			qhtole64(dl_tdoa_anchor_location->data.wgs84.latitude);
		temp[1] =
			qhtole64(dl_tdoa_anchor_location->data.wgs84.longitude);
		temp[2] =
			qhtole64(dl_tdoa_anchor_location->data.wgs84.altitude);
		loc_x = (uint8_t *)&temp[0];
		loc_y = (uint8_t *)&temp[1];
		loc_z = (uint8_t *)&temp[2];
		/* 33b for latitute / 33b for longitude / 30b for altitude. */
		location[1] = loc_x[0];
		location[2] = loc_x[1];
		location[3] = loc_x[2];
		location[4] = loc_x[3];
		location[5] = ((loc_x[4] << 7) & 0x80) |
			      ((loc_y[0] >> 1) & 0x7F);
		location[6] = ((loc_y[0] << 7) & 0x80) |
			      ((loc_y[1] >> 1) & 0x7F);
		location[7] = ((loc_y[1] << 7) & 0x80) |
			      ((loc_y[2] >> 1) & 0x7F);
		location[8] = ((loc_y[2] << 7) & 0x80) |
			      ((loc_y[3] >> 1) & 0x7F);
		location[9] = ((loc_y[3] << 7) & 0x80) |
			      ((loc_y[4] << 6) & 0x40) |
			      ((loc_z[0] >> 2) & 0x3F);
		location[10] = ((loc_z[0] << 6) & 0xC0) |
			       ((loc_z[1] >> 2) & 0x3F);
		location[11] = ((loc_z[1] << 6) & 0xC0) |
			       ((loc_z[2] >> 2) & 0x3F);
		location[12] = ((loc_z[2] << 6) & 0xC0) | (loc_z[3] & 0x3F);

		length = 13;
		break;

	case CHERRY_FIRA_ANCHOR_LOCATION_TYPE_REL:
		temp[0] = qhtole64(dl_tdoa_anchor_location->data.relative.x);
		temp[1] = qhtole64(dl_tdoa_anchor_location->data.relative.y);
		temp[2] = qhtole64(dl_tdoa_anchor_location->data.relative.z);
		loc_x = (uint8_t *)&temp[0];
		loc_y = (uint8_t *)&temp[1];
		loc_z = (uint8_t *)&temp[2];
		/* 28b for x coordinates / 28b for y coordinates / 24b z coordinates */
		location[1] = loc_x[0];
		location[2] = loc_x[1];
		location[3] = loc_x[2];
		location[4] = ((loc_x[3] << 4) & 0xF0) |
			      ((loc_y[0] >> 4) & 0x0F);
		location[5] = ((loc_y[0] << 4 & 0xF0)) |
			      ((loc_y[1] >> 4) & 0x0F);
		location[6] = ((loc_y[1] << 4 & 0xF0)) |
			      ((loc_y[2] >> 4) & 0x0F);
		location[7] = ((loc_y[2] << 4 & 0xF0)) | (loc_y[3] & 0x0F);
		location[8] = loc_z[0];
		location[9] = loc_z[1];
		location[10] = loc_z[2];
		length = 11;
		break;

	case CHERRY_FIRA_ANCHOR_LOCATION_TYPE_WGS84_Z_EXT:
		temp[0] = qhtole64(
			dl_tdoa_anchor_location->data.wgs84_z_ext.latitude);
		temp[1] = qhtole64(
			dl_tdoa_anchor_location->data.wgs84_z_ext.longitude);
		temp[2] = qhtole64(
			dl_tdoa_anchor_location->data.wgs84_z_ext.altitude);
		loc_x = (uint8_t *)&temp[0];
		loc_y = (uint8_t *)&temp[1];
		loc_z = (uint8_t *)&temp[2];
		/* 33b for latitute / 33b for longitude / 30b for altitude. */
		location[1] = loc_x[0];
		location[2] = loc_x[1];
		location[3] = loc_x[2];
		location[4] = loc_x[3];
		location[5] = ((loc_x[4] << 7) & 0x80) |
			      ((loc_y[0] >> 1) & 0x7F);
		location[6] = ((loc_y[0] << 7) & 0x80) |
			      ((loc_y[1] >> 1) & 0x7F);
		location[7] = ((loc_y[1] << 7) & 0x80) |
			      ((loc_y[2] >> 1) & 0x7F);
		location[8] = ((loc_y[2] << 7) & 0x80) |
			      ((loc_y[3] >> 1) & 0x7F);
		location[9] = ((loc_y[3] << 7) & 0x80) |
			      ((loc_y[4] << 6) & 0x40) |
			      ((loc_z[0] >> 2) & 0x3F);
		location[10] = ((loc_z[0] << 6) & 0xC0) |
			       ((loc_z[1] >> 2) & 0x3F);
		location[11] = ((loc_z[1] << 6) & 0xC0) |
			       ((loc_z[2] >> 2) & 0x3F);
		location[12] = ((loc_z[2] << 6) & 0xC0) | (loc_z[3] & 0x3F);

		floor_val = qhtole16(
			dl_tdoa_anchor_location->data.wgs84_z_ext.floor);
		location[13] = (floor_val >> 6) & 0xFF;
		location[14] |= (floor_val << 2) & 0xFC;
		location[14] &= ~0x6;
		location[14] |=
			dl_tdoa_anchor_location->data.wgs84_z_ext.moveable;
		height_val = qhtole32(
			dl_tdoa_anchor_location->data.wgs84_z_ext.height);
		location[15] = (height_val >> 16) & 0xFF;
		location[16] = (height_val >> 8) & 0xFF;
		location[17] = height_val & 0xFF;
		location[18] = dl_tdoa_anchor_location->data.wgs84_z_ext
				       .height_uncertainty;
		length = 19;
		break;

	case CHERRY_FIRA_ANCHOR_LOCATION_TYPE_REL_Z_EXT:
		temp[0] = qhtole64(
			dl_tdoa_anchor_location->data.relative_z_ext.x);
		temp[1] = qhtole64(
			dl_tdoa_anchor_location->data.relative_z_ext.y);
		temp[2] = qhtole64(
			dl_tdoa_anchor_location->data.relative_z_ext.z);
		loc_x = (uint8_t *)&temp[0];
		loc_y = (uint8_t *)&temp[1];
		loc_z = (uint8_t *)&temp[2];
		/* 28b for x coordinates / 28b for y coordinates / 24b z coordinates */
		location[1] = loc_x[0];
		location[2] = loc_x[1];
		location[3] = loc_x[2];
		location[4] = ((loc_x[3] << 4) & 0xF0) |
			      ((loc_y[0] >> 4) & 0x0F);
		location[5] = ((loc_y[0] << 4 & 0xF0)) |
			      ((loc_y[1] >> 4) & 0x0F);
		location[6] = ((loc_y[1] << 4 & 0xF0)) |
			      ((loc_y[2] >> 4) & 0x0F);
		location[7] = ((loc_y[2] << 4 & 0xF0)) | (loc_y[3] & 0x0F);
		location[8] = loc_z[0];
		location[9] = loc_z[1];
		location[10] = loc_z[2];

		floor_val = qhtole16(
			dl_tdoa_anchor_location->data.wgs84_z_ext.floor);
		location[11] = (floor_val >> 6) & 0xFF;
		location[12] |= (floor_val << 2) & 0xFC;
		location[12] &= ~0x6;
		location[12] |=
			dl_tdoa_anchor_location->data.wgs84_z_ext.moveable;
		height_val = qhtole32(
			dl_tdoa_anchor_location->data.wgs84_z_ext.height);
		location[13] = (height_val >> 16) & 0xFF;
		location[14] = (height_val >> 8) & 0xFF;
		location[15] = height_val & 0xFF;
		location[16] = dl_tdoa_anchor_location->data.wgs84_z_ext
				       .height_uncertainty;
		length = 17;
		break;

	default:
		return UCI_STATUS_INVALID_PARAM;
	}

	if (dl_tdoa_anchor_location->type ==
		    CHERRY_FIRA_ANCHOR_LOCATION_TYPE_NONE ||
	    dl_tdoa_anchor_location->type ==
		    CHERRY_FIRA_ANCHOR_LOCATION_TYPE_REL ||
	    dl_tdoa_anchor_location->type ==
		    CHERRY_FIRA_ANCHOR_LOCATION_TYPE_WGS84) {
		ret = cherry_uci_client_session_set_app_config_cmd_put(
			cmd, UCI_APPLICATION_PARAMETER_DL_TDOA_ANCHOR_LOCATION,
			location, length);
	} else {
		ret = cherry_uci_client_session_set_app_config_cmd_put(
			cmd,
			0x4e /* UCI_APPLICATION_PARAMETER_DL_TDOA_ANCHOR_LOCATION_V2 */,
			location, length);
	}

	return ret;
}

enum uci_status_code
cherry_uci_client_session_get_app_config_dl_tdoa_anchor_location_handler(
	struct uci_message_parser *parser, void *data, uint8_t param_len)
{
	struct dl_tdoa_anchor_location *location = data;
	uint8_t dl_tdoa_anchor_location_array[DL_TDOA_ANCHOR_LOCATION_MAX_SIZE];
	uint64_t temp[3] = { 0, 0, 0 };
	uint8_t *loc = (uint8_t *)&dl_tdoa_anchor_location_array[1];

	if (param_len != 1 && param_len != 11 && param_len != 13)
		return UCI_STATUS_INVALID_MESSAGE_SIZE;

	uci_message_get(parser, dl_tdoa_anchor_location_array, param_len);

	location->location_presence = dl_tdoa_anchor_location_array[0] & 0x01;
	location->coordinates_system =
		(dl_tdoa_anchor_location_array[0] & 0x02) >> 1;

	if (!location->location_presence)
		return UCI_STATUS_OK;

	switch (location->coordinates_system) {
	case FIRA_DT_LOCATION_COORD_WGS84: {
		uint8_t *latitude = (uint8_t *)&temp[0];
		uint8_t *longitude = (uint8_t *)&temp[1];
		uint8_t *altitude = (uint8_t *)&temp[2];

		/* 33b for latitute / 33b for longitude / 30b for altitude. */
		latitude[0] = loc[0];
		latitude[1] = loc[1];
		latitude[2] = loc[2];
		latitude[3] = loc[3];
		latitude[4] = (loc[4] >> 7) & 0x01;

		longitude[0] = ((loc[4] << 1) & 0xFE) | ((loc[5] >> 7) & 0x01);
		longitude[1] = ((loc[5] << 1) & 0xFE) | ((loc[6] >> 7) & 0x01);
		longitude[2] = ((loc[6] << 1) & 0xFE) | ((loc[7] >> 7) & 0x01);
		longitude[3] = ((loc[7] << 1) & 0xFE) | ((loc[8] >> 7) & 0x01);
		longitude[4] = (loc[8] >> 6) & 0x01;

		altitude[0] = ((loc[8] << 2) & 0xFC) | ((loc[9] >> 6) & 0x03);
		altitude[1] = ((loc[9] << 2) & 0xFC) | ((loc[10] >> 6) & 0x03);
		altitude[2] = ((loc[10] << 2) & 0xFC) | ((loc[11] >> 6) & 0x03);
		altitude[3] = loc[11] & 0x3F;
		break;
	}

	case FIRA_DT_LOCATION_COORD_RELATIVE: {
		uint8_t *relative_x = (uint8_t *)&temp[0];
		uint8_t *relative_y = (uint8_t *)&temp[1];
		uint8_t *relative_z = (uint8_t *)&temp[2];

		/* 28b for x coordinates / 28b for y coordinates / 24b z coordinates */
		relative_x[0] = loc[0];
		relative_x[1] = loc[1];
		relative_x[2] = loc[2];
		relative_x[3] = (loc[3] >> 4) & 0x0F;

		relative_y[0] = ((loc[3] << 4) & 0xF0) | ((loc[4] >> 4) & 0x0F);
		relative_y[1] = ((loc[4] << 4) & 0xF0) | ((loc[5] >> 4) & 0x0F);
		relative_y[2] = ((loc[5] << 4) & 0xF0) | ((loc[6] >> 4) & 0x0F);
		relative_y[3] = loc[6] & 0x0F;

		relative_z[0] = loc[7];
		relative_z[1] = loc[8];
		relative_z[2] = loc[9];
		break;
	}

	case FIRA_DT_LOCATION_COORD_INVALID:
		return UCI_STATUS_INVALID_PARAM;
	}

	location->location_x = qle64toh(temp[0]);
	location->location_y = qle64toh(temp[1]);
	location->location_z = qle64toh(temp[2]);

	return UCI_STATUS_OK;
}

enum uci_status_code
cherry_uci_client_session_get_app_config_dl_tdoa_anchor_location(
	struct cherry_session_context *context, uint32_t session_handle,
	struct dl_tdoa_anchor_location *dl_tdoa_anchor_location)
{
	return cherry_uci_client_session_get_app_config(
		context, session_handle,
		UCI_APPLICATION_PARAMETER_DL_TDOA_ANCHOR_LOCATION,
		dl_tdoa_anchor_location, 13,
		cherry_uci_client_session_get_app_config_dl_tdoa_anchor_location_handler);
}

enum uci_status_code
cherry_uci_client_session_get_count(struct cherry_session_context *context,
				    int *count)
{
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_COUNT);
	struct uci_message_builder builder;
	int ret;

	if (!context || !count) {
		return UCI_STATUS_INVALID_PARAM;
	}

	uci_message_builder_init(&builder, context->uci);
	context->cmd_data = count;
	/* Send message */
	uci_send_message(context->uci, mt_gid_oid, builder.message);
	/* Wait for response */
	ret = cherry_session_wait_rsp(context);

	return ret ? context->response_status : UCI_STATUS_UCI_MESSAGE_RETRY;
}

enum uci_status_code
cherry_uci_client_session_get_state(struct cherry_session_context *context,
				    uint32_t session_handle, int *state)
{
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND,
					     UCI_GID_SESSION_CONFIG,
					     UCI_OID_SESSION_GET_STATE);
	struct uci_message_builder builder;
	int ret;

	if (!context || !state) {
		return UCI_STATUS_INVALID_PARAM;
	}

	uci_message_builder_init(&builder, context->uci);
	uci_message_put_32bit(&builder, session_handle);
	context->cmd_data = state;
	/* Send message */
	uci_send_message(context->uci, mt_gid_oid, builder.message);
	/* Wait for response */
	ret = cherry_session_wait_rsp(context);

	return ret ? context->response_status : UCI_STATUS_UCI_MESSAGE_RETRY;
}

enum cherry_common_frame_status
cherry_session_frame_status_to_cherry_format(enum fira_status code)
{
	switch (code) {
	case FIRA_STATUS_OK:
	case FIRA_STATUS_OK_NEGATIVE_DISTANCE_REPORT:
		return CHERRY_COMMON_FRAME_STATUS_OK;
	case FIRA_STATUS_RANGING_TX_FAILED:
		return CHERRY_COMMON_FRAME_STATUS_TX_FAILED;
	case FIRA_STATUS_RANGING_RX_TIMEOUT:
		return CHERRY_COMMON_FRAME_STATUS_RX_TIMEOUT;
	case FIRA_STATUS_RANGING_RX_PHY_DEC_FAILED:
		return CHERRY_COMMON_FRAME_STATUS_RX_PHY_DEC_FAILED;
	case FIRA_STATUS_RANGING_RX_PHY_TOA_FAILED:
		return CHERRY_COMMON_FRAME_STATUS_RX_PHY_TOA_FAILED;
	case FIRA_STATUS_RANGING_RX_PHY_STS_FAILED:
		return CHERRY_COMMON_FRAME_STATUS_RX_PHY_STS_FAILED;
	case FIRA_STATUS_RANGING_RX_MAC_DEC_FAILED:
		return CHERRY_COMMON_FRAME_STATUS_RX_MAC_DEC_FAILED;
	case FIRA_STATUS_RANGING_RX_MAC_IE_DEC_FAILED:
		return CHERRY_COMMON_FRAME_STATUS_RX_MAC_IE_DEC_FAILED;
	case FIRA_STATUS_RANGING_RX_MAC_IE_MISSING:
		return CHERRY_COMMON_FRAME_STATUS_RX_MAC_IE_MISSING;
	default:
		break;
	}
	return CHERRY_COMMON_FRAME_STATUS_UNKNOWN;
}
