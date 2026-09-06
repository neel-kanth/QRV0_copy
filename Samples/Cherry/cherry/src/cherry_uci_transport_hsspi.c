/*
 * Cherry UCI transport over HSSPI.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "cherry_log.h"
#include "cherry_uci_transport.h"

enum cherry_err cherry_init_transport(struct cherry_uci_transport *ctx,
				      struct uci *uci, const char *device)
{
	/* Initialization of the UCI transport HSSPI. */
	ctx->transport = uci_transport_hsspi_create();
	if (!ctx->transport) {
		QLOGE("%s: Failed to create UCI transport HSSPI ", __func__,
		      device);
		goto uci_err;
	}
	/* Attach the UCI client to the UCI transport HSSPI. */
	if (uci_transport_attach(uci, ctx->transport)) {
		QLOGE("%s: Failed uci_transport_attach", __func__);
		goto hsspi_destroy;
	}
	ctx->serial = false;
	ctx->reader_thread = NULL;
	ctx->reading = true;
	ctx->uci = uci;
	return CHERRY_ERR_NONE;

hsspi_destroy:
	uci_transport_hsspi_destroy(ctx->transport);
uci_err:
	return CHERRY_ERR_INTERNAL;
}

int cherry_client_reset(struct cherry_uci_transport *ctx)
{
	void reset_uwbs(void);
	reset_uwbs();
	return 0;
}

void cherry_de_init_transport(struct cherry_uci_transport *ctx)
{
	/* Detach the UCI client from the UCI transport HSSPI. */
	uci_transport_detach(ctx->uci);
	/* Delete the UCI transport HSSPI. */
	if (ctx->transport) {
		uci_transport_hsspi_destroy(ctx->transport);
	}
	return;
}
