/*
 * Implementation for calibration client functions
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "cherry_calib_client.h"

#define LOG_TAG "cherry_uci_client"
#include "cherry_log.h"

#include <qerr.h>
#include <qmalloc.h>
#include <qsemaphore.h>
#include <qtils.h>
#include <stdio.h>
#include <uci/uci_message.h>
#include <uci/uci_spec_mcps.h>
#include <uci/uci_unit_converter.h>

#ifndef CALIBRATION_NAME_MAX_LEN
#define CALIBRATION_NAME_MAX_LEN 64
#endif

struct cherry_calib_context {
	/* private: internal use only */
	struct uci *uci;
	void *cmd_data;
	uint8_t response_status;
	struct qsemaphore *sem_response;
};

static inline int cherry_calib_wait_rsp(struct cherry_calib_context *context)
{
	int r = qsemaphore_take(context->sem_response, 1000);

	/* Error or timeout. */
	if (r)
		return 0;

	/* Completed in time. */
	return 1;
}

static inline void cherry_calib_set_rsp(struct cherry_calib_context *context)
{
	qsemaphore_give(context->sem_response);
}

struct cherry_uci_client_uwbs_config_set_cmd {
	struct cherry_calib_context *context;
	struct uci_message_builder builder;
	/* session_handle is a void pointer as not aligned. Use memcpy to update it. */
	uint16_t *nb_key;
};

struct cherry_uci_client_uwbs_config_set_cmd *
cherry_uci_client_uwbs_config_set_cmd_create(
	struct cherry_calib_context *context)
{
	struct cherry_uci_client_uwbs_config_set_cmd *cmd;
	struct uci_message_builder *builder;

	if (!context) {
		return NULL;
	}

	cmd = qmalloc(sizeof(*cmd));
	if (!cmd) {
		return NULL;
	}

	cmd->context = context;
	builder = &cmd->builder;
	uci_message_builder_init(builder, context->uci);
	cmd->nb_key = uci_message_reserve_16bit(builder, 0);
	if (!cmd->nb_key)
		goto failed;

	return cmd;

failed:
	qfree(cmd);
	return NULL;
}

static void cherry_uci_client_uwbs_config_set_cmd_destroy(
	struct cherry_uci_client_uwbs_config_set_cmd *cmd)
{
	if (!cmd)
		return;

	uci_blk_free_all(cmd->context->uci, cmd->builder.message);
	qfree(cmd);
}

enum uci_status_code cherry_uci_client_uwbs_config_set_cmd_send(
	struct cherry_uci_client_uwbs_config_set_cmd *cmd)
{
	const uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND, UCI_GID_QORVO_MAC,
			       UCI_OID_QORVO_MAC_SET_CALIBRATIONS);
	int ret;
	struct cherry_calib_context *context;

	if (!cmd)
		return UCI_STATUS_INVALID_PARAM;

	/* No configuration set, just abort the command and fake success return code. */
	if (*cmd->nb_key == 0) {
		cherry_uci_client_uwbs_config_set_cmd_abort(cmd);
		return UCI_STATUS_OK;
	}

	context = cmd->context;

	/* Send message */
	uci_send_message(context->uci, mt_gid_oid, cmd->builder.message);
	cmd->builder.message = NULL;
	/* Wait for response */
	ret = cherry_calib_wait_rsp(context);

	cherry_uci_client_uwbs_config_set_cmd_destroy(cmd);

	return ret ? context->response_status : UCI_STATUS_UCI_MESSAGE_RETRY;
}

enum uci_status_code cherry_uci_client_uwbs_config_set_cmd_put(
	struct cherry_uci_client_uwbs_config_set_cmd *cmd, const char *keyname,
	const uint8_t *value, uint8_t value_size)
{
	struct uci_message_builder *builder;
	int ret;
	uint8_t keyname_size;

	if (!cmd || !keyname || !value || value_size == 0)
		return UCI_STATUS_INVALID_PARAM;

	builder = &cmd->builder;
	keyname_size = strlen(keyname);

	ret = uci_message_put_8bit(builder, keyname_size);
	if (!ret)
		uci_message_put(builder, (const uint8_t *)keyname,
				keyname_size);
	if (!ret)
		uci_message_put_8bit(builder, value_size);
	if (!ret)
		uci_message_put(builder, value, value_size);

	if (ret)
		return UCI_STATUS_FAILED;

	++(*cmd->nb_key);
	return UCI_STATUS_OK;
}

void cherry_uci_client_uwbs_config_set_cmd_abort(
	struct cherry_uci_client_uwbs_config_set_cmd *cmd)
{
	cherry_uci_client_uwbs_config_set_cmd_destroy(cmd);
}

enum uci_status_code
cherry_uci_client_calib_set_key(struct cherry_calib_context *context,
				const char *key, const char *value,
				size_t value_size)
{
	enum uci_status_code err;
	struct cherry_uci_client_uwbs_config_set_cmd *cmd;

	cmd = cherry_uci_client_uwbs_config_set_cmd_create(context);
	if (!cmd)
		return UCI_STATUS_INVALID_PARAM;

	err = cherry_uci_client_uwbs_config_set_cmd_put(
		cmd, key, (const uint8_t *)value, value_size);
	if (err != UCI_STATUS_OK)
		goto failed;

	return cherry_uci_client_uwbs_config_set_cmd_send(cmd);

failed:
	cherry_uci_client_uwbs_config_set_cmd_abort(cmd);
	return err;
}

static enum qerr
cherry_uci_client_calib_set_key_handler(struct uci *uci, uint16_t mt_gid_oid,
					const struct uci_blk *payload,
					void *user_data)
{
	struct cherry_calib_context *context =
		(struct cherry_calib_context *)user_data;
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(payload);

	if (uci_message_remaining(&parser) < 1) {
		context->response_status = UCI_STATUS_SYNTAX_ERROR;
	} else {
		context->response_status = uci_message_get_8bit(&parser);
	}
	cherry_calib_set_rsp(context);

	return QERR_SUCCESS;
}

enum uci_status_code
cherry_uci_client_calib_get_key(struct cherry_calib_context *context,
				const char **keys, const uint16_t n_keys,
				struct cherry_calib_cb *cb)
{
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND, UCI_GID_QORVO_MAC,
			       UCI_OID_QORVO_MAC_GET_CALIBRATIONS);
	struct uci_message_builder builder;
	int ret;
	uint16_t idx;

	if (!context)
		return UCI_STATUS_INVALID_PARAM;

	uci_message_builder_init(&builder, context->uci);
	uci_message_put_16bit(&builder, n_keys);
	for (idx = 0; idx < n_keys; idx++) {
		uci_message_put_string(&builder, keys[idx], strlen(keys[idx]));
	}
	context->cmd_data = cb;
	uci_send_message(context->uci, mt_gid_oid, builder.message);

	ret = cherry_calib_wait_rsp(context);

	return ret ? context->response_status : UCI_STATUS_UCI_MESSAGE_RETRY;
}

static enum uci_status_code parse_key(struct uci_message_parser *parser,
				      struct cherry_calib_cb *calib_cb)
{
	enum uci_status_code ret = UCI_STATUS_OK;
	uint8_t key_status;
	uint8_t key_name_size;
	char *key_name;
	uint8_t data_len;
	void *data;

	/* Get size of key's name. */
	if (!uci_message_get_8bit_no_assert(parser, &key_name_size)) {
		ret = UCI_STATUS_SYNTAX_ERROR;
		goto end;
	}

	key_name = calib_cb->alloc(calib_cb->user_data, key_name_size + 1);
	if (!key_name) {
		ret = UCI_STATUS_INVALID_PARAM;
		goto end;
	}
	/* Get the key's name. */
	if (!uci_message_get(parser, (uint8_t *)key_name, key_name_size)) {
		ret = UCI_STATUS_INVALID_PARAM;
		goto end;
	}
	*(uint8_t *)(key_name + key_name_size) = '\0';

	/* Get the key's status. */
	if (!uci_message_get_8bit_no_assert(parser, &key_status)) {
		ret = UCI_STATUS_SYNTAX_ERROR;
		goto end;
	}

	if (key_status) {
		ret = key_status;
		goto end;
	}

	/* Get the key's value size. */
	if (!uci_message_get_8bit_no_assert(parser, &data_len)) {
		ret = UCI_STATUS_SYNTAX_ERROR;
		goto end;
	}

	data = calib_cb->alloc(calib_cb->user_data, data_len);
	if (!data) {
		ret = UCI_STATUS_INVALID_PARAM;
		goto end;
	}
	if (!uci_message_get(parser, (uint8_t *)data, data_len)) {
		ret = UCI_STATUS_INVALID_RANGE;
		goto end;
	}

	/* Notify a new keys. */
	if (!calib_cb->key_notif(calib_cb->user_data, key_name, data, data_len))
		ret = UCI_STATUS_SYNTAX_ERROR;

end:
	return ret;
}

static enum qerr
cherry_uci_client_calib_get_key_handler(struct uci *uci, uint16_t mt_gid_oid,
					const struct uci_blk *payload,
					void *user_data)
{
	struct cherry_calib_context *context =
		(struct cherry_calib_context *)user_data;
	struct cherry_calib_cb *calib_cb = context->cmd_data;
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(payload);
	uint16_t number_key;
	uint16_t idx;

	/* Get the response status. */
	if (!uci_message_get_8bit_no_assert(
		    &parser, (uint8_t *)&context->response_status)) {
		context->response_status = UCI_STATUS_SYNTAX_ERROR;
		goto end;
	}

	if (context->response_status != UCI_STATUS_OK) {
		goto end;
	}

	/* Get the number of keys. */
	if (!uci_message_get_16bit_no_assert(&parser, &number_key)) {
		context->response_status = UCI_STATUS_SYNTAX_ERROR;
		goto end;
	}

	for (idx = 0; idx < number_key; idx++) {
		if ((context->response_status = parse_key(&parser, calib_cb)) !=
		    UCI_STATUS_OK)
			goto end;
	}

	/* If data remaining we consider the response false. */
	if (uci_message_remaining(&parser)) {
		context->response_status = UCI_STATUS_SYNTAX_ERROR;
		goto end;
	}

end:
	cherry_calib_set_rsp(context);

	return QERR_SUCCESS;
}

/* Handlers in table need to have their OID's ordered ascending. */
static struct uci_message_handler uci_rsp_session_config_handlers[] = {
	{
		.mt_gid_oid = UCI_MT_GID_OID(
			UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_QORVO_MAC,
			UCI_OID_QORVO_MAC_SET_CALIBRATIONS),
		.handler = cherry_uci_client_calib_set_key_handler,
	},
	{
		.mt_gid_oid = UCI_MT_GID_OID(
			UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_QORVO_MAC,
			UCI_OID_QORVO_MAC_GET_CALIBRATIONS),
		.handler = cherry_uci_client_calib_get_key_handler,
	},
};

static struct uci_message_handlers uci_rsp_session_config_handlers_list = {
	.next = NULL,
	.handlers = uci_rsp_session_config_handlers,
	.n_handlers = qarray_size(uci_rsp_session_config_handlers),
	.user_data = NULL,
};

enum qerr cherry_uci_client_calib_open(struct cherry_calib_context **context,
				       struct uci *uci)
{
	*context = (struct cherry_calib_context *)qmalloc(
		sizeof(struct cherry_calib_context));
	if (!context)
		return QERR_EINVAL;
	(*context)->uci = uci;

	(*context)->sem_response = qsemaphore_init(0, 1);

	/* setup handlers */
	uci_rsp_session_config_handlers_list.user_data = (*context);
	uci_message_handlers_register((*context)->uci,
				      &uci_rsp_session_config_handlers_list);

	return QERR_SUCCESS;
}

void cherry_uci_client_calib_close(struct cherry_calib_context *context)
{
	if (!context)
		return;

	qsemaphore_deinit(context->sem_response);

	uci_rsp_session_config_handlers_list.user_data = NULL;
	uci_message_handlers_unregister(context->uci,
					&uci_rsp_session_config_handlers_list);
	qfree(context);
}
