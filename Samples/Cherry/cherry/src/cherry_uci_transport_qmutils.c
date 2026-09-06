/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "cherry_log.h"
#include "cherry_uci_transport.h"
#include "uci_transport/uci_transport_qmutils.h"

enum cherry_err cherry_init_transport(struct cherry_uci_transport *ctx,
				      struct uci *uci, const char *device)
{
	/* Initialization of the UCI transport based on qmutils channels. */
	ctx->transport = uci_transport_qmutils_create(&ctx->uci_fd);
	if (!ctx->transport) {
		QLOGE("%s: Failed to create UCI Transport Char Dev. Verify %s exists ",
		      __func__, device);
		goto uci_err;
	}
	/* Attach the UCI client to the UCI transport. */
	if (uci_transport_attach(uci, ctx->transport) != QERR_SUCCESS) {
		QLOGE("%s: Failed uci_transport_attach", __func__);
		goto transport_destroy;
	}

	ctx->uci = uci;
	ctx->serial = false;
	ctx->reading = true;
	/* No thread required on Zephyr using the qmutils based  transport. The
	 * bypass API will automatically call the registered channel callback.
	 * From either interrupt handler or a thread woken-up by an interrupt.
	 */
	ctx->reader_thread = NULL;
	return CHERRY_ERR_NONE;

transport_destroy:
	uci_transport_qmutils_destroy(ctx->transport);
uci_err:
	return CHERRY_ERR_INTERNAL;
}

int cherry_client_reset(struct cherry_uci_transport *ctx)
{
	/* Reset the UCI client. */
	return uci_transport_qmutils_reset(ctx->transport);
}

void cherry_de_init_transport(struct cherry_uci_transport *ctx)
{
	/* Detach the UCI client from the UCI transport. */
	uci_transport_detach(ctx->uci);
	/* Destroy of the UCI transport based on qmutils channels. */
	if (ctx->transport) {
		uci_transport_qmutils_destroy(ctx->transport);
	}
	return;
}
