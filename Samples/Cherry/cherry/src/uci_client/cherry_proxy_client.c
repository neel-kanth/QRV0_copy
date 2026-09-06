/*
 * Implemention of uci core client functions
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "cherry_proxy_client.h"

#include "uci/uci.h"

#include <qmalloc.h>
#include <qsemaphore.h>
#include <qtils.h>
#include <uci/uci_message.h>

struct cherry_proxy_context {
	cherry_uci_client_proxy_boot_cb_t boot_cb;
	cherry_uci_client_proxy_data_cb_t data_cb;
	void *user_data;
	struct uci uci;
	bool bridge;
};

static enum qerr uci_data_handler(struct uci *uci, uint16_t mt_gid_oid,
				  const struct uci_blk *payload,
				  void *user_data)
{
	struct uci_blk *block_available = (struct uci_blk *)payload;
	struct cherry_proxy_context *context = user_data;

	if (context->data_cb) {
		while (block_available) {
			context->data_cb(block_available->data,
					 block_available->len,
					 context->user_data);
			block_available = block_available->next;
		}
	}
	return QERR_SUCCESS;
}

static enum qerr uci_boot_handler(struct uci *uci, uint16_t mt_gid_oid,
				  const struct uci_blk *payload,
				  void *user_data)
{
	struct cherry_proxy_context *context = user_data;
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(payload);
	enum uci_qorvo_boot_reason reason;

	if (uci_message_remaining(&parser) < 1) {
		reason = UCI_QORVO_BOOT_REASON_FATAL_ERROR_RESET;
	} else {
		reason = uci_message_get_8bit(&parser);
	}

	if (context->boot_cb)
		context->boot_cb(reason, context->user_data);

	return QERR_SUCCESS;
}

/* Handlers for Bridge. */

static const struct uci_message_handler uci_qorvo_cmd_handlers_bridge[] = {
	{
		.mt_gid_oid = UCI_MT_GID_OID(
			UCI_MESSAGE_TYPE_COMMAND, UCI_GID_QORVO_EXT2,
			UCI_OID_QORVO_CORE_GET_DEVICE_STATS),
		.handler = uci_data_handler,
	},
};

static struct uci_message_handlers uci_rsp_qorvo_handlers_list_bridge = {
	.next = NULL,
	.handlers = uci_qorvo_cmd_handlers_bridge,
	.n_handlers = qarray_size(uci_qorvo_cmd_handlers_bridge),
	.user_data = NULL,
};

/* Handlers for UCI. */

static const struct uci_message_handler uci_qorvo_cmd_handlers_uci[] = {
	{
		.mt_gid_oid = UCI_MT_GID_OID(
			UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_QORVO_EXT2,
			UCI_OID_QORVO_CORE_GET_DEVICE_STATS),
		.handler = uci_data_handler,
	},
	{
		.mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_NOTIFICATION,
					     UCI_GID_QORVO_EXT2,
					     UCI_OID_QORVO_CORE_DEVICE_BOOT),
		.handler = uci_boot_handler,
	},
	{
		.mt_gid_oid = UCI_MT_GID_OID(
			UCI_MESSAGE_TYPE_NOTIFICATION, UCI_GID_QORVO_EXT2,
			UCI_OID_QORVO_FIRA_RANGE_DIAGNOSTICS),
		.handler = uci_data_handler,
	},
};

static struct uci_message_handlers uci_rsp_qorvo_handlers_list_uci = {
	.next = NULL,
	.handlers = uci_qorvo_cmd_handlers_uci,
	.n_handlers = qarray_size(uci_qorvo_cmd_handlers_uci),
	.user_data = NULL,
};

static struct uci_generic_handler uci_generic_handler_bridge = {
	.handler = uci_data_handler,
	.user_data = NULL,
};

static struct uci_generic_handler uci_generic_handler_uci = {
	.handler = uci_data_handler,
	.user_data = NULL,
};

static struct uci_blk *simple_acquire(struct uci_allocator *allocator,
				      size_t size_hint, uint8_t flags_hint)
{
	struct uci_blk *p;

	p = (struct uci_blk *)qmalloc(sizeof(*p) + UCI_MAX_PACKET_SIZE);
	if (p) {
		p->data = (uint8_t *)&p[1];
		p->size = UCI_MAX_PACKET_SIZE;
	}
	return p;
}

static void simple_release(struct uci_allocator *allocator,
			   struct uci_blk *packet)
{
	qfree(packet);
}

static struct uci_allocator_ops simple_allocator_ops = {
	.alloc = simple_acquire,
	.free = simple_release,
};

static struct uci_allocator simple_allocator = { .ops = &simple_allocator_ops };

enum qerr cherry_uci_client_proxy_open(
	struct cherry_proxy_context **context, struct uci_transport *tr,
	void *user_data, bool bridge, cherry_uci_client_proxy_boot_cb_t boot_cb,
	cherry_uci_client_proxy_data_cb_t data_cb)
{
	struct cherry_proxy_context *ctx;

	if ((!boot_cb && !bridge) || !data_cb || !tr)
		return QERR_EINVAL;

	*context = ctx = (struct cherry_proxy_context *)qmalloc(
		sizeof(struct cherry_proxy_context));
	if (!*context)
		return QERR_EINVAL;

	/* Initialization of UCI client for proxy. */
	if (uci_init(&ctx->uci, &simple_allocator, true) != QERR_SUCCESS) {
		goto ctx_destroy;
	}
	/* Attach the UCI client to the UCI transport char dev. */
	if (uci_transport_attach(&ctx->uci, tr) != QERR_SUCCESS) {
		goto uci_destroy;
	}

	(*context)->user_data = user_data;
	(*context)->bridge = bridge;
	(*context)->boot_cb = boot_cb;
	(*context)->data_cb = data_cb;

	if (bridge) {
		/* Setup qorvo handlers. */
		uci_rsp_qorvo_handlers_list_bridge.user_data = (*context);
		uci_message_handlers_register(
			&ctx->uci, &uci_rsp_qorvo_handlers_list_bridge);
		/* Setup generic handler. */
		uci_generic_handler_bridge.user_data = (*context);
		uci_message_generic_handler_register(
			&ctx->uci, &uci_generic_handler_bridge);
	} else {
		/* Setup qorvo handlers. */
		uci_rsp_qorvo_handlers_list_uci.user_data = (*context);
		uci_message_handlers_register(&ctx->uci,
					      &uci_rsp_qorvo_handlers_list_uci);
		/* Setup generic handler. */
		uci_generic_handler_uci.user_data = (*context);
		uci_message_generic_handler_register(&ctx->uci,
						     &uci_generic_handler_uci);
	}

	return QERR_SUCCESS;

uci_destroy:
	uci_uninit(&ctx->uci);
ctx_destroy:
	qfree(ctx);
	*context = NULL;
	return QERR_EINVAL;
}

void cherry_uci_client_proxy_close(struct cherry_proxy_context *context)
{
	if (!context)
		return;

	context->user_data = NULL;

	if (context->bridge) {
		uci_rsp_qorvo_handlers_list_bridge.user_data = NULL;
		uci_message_handlers_unregister(
			&context->uci, &uci_rsp_qorvo_handlers_list_bridge);
		uci_generic_handler_bridge.user_data = NULL;
		uci_message_generic_handler_unregister(&context->uci);
	} else {
		uci_rsp_qorvo_handlers_list_uci.user_data = NULL;
		uci_message_handlers_unregister(
			&context->uci, &uci_rsp_qorvo_handlers_list_uci);
		uci_generic_handler_uci.user_data = NULL;
		uci_message_generic_handler_unregister(&context->uci);
	}

	/* Detach the UCI client from the UCI transport. */
	uci_transport_detach(&context->uci);
	/* And uninit UCI client.*/
	uci_uninit(&context->uci);

	qfree(context);
}

enum uci_status_code
cherry_uci_client_proxy_send_data(struct cherry_proxy_context *context,
				  const uint8_t *data, uint16_t len)
{
	struct uci_blk blk;

	blk.data = (uint8_t *)data;
	blk.len = len;
	blk.flags = UCI_BLK_FLAGS_HEADER_RESERVED;

	uci_send_message_raw(&context->uci, &blk);

	return UCI_STATUS_OK;
}

struct uci *cherry_uci_client_proxy_get(struct cherry_proxy_context *context)
{
	return &context->uci;
}
