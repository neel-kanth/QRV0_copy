/*
 * Implementation of UCI transport over HSSPI.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "uci/uci_message.h"

#include <drivers/uwb/hsspi_async.h>
#include <errno.h>
#include <qlog.h>
#include <qtils.h>
#include <stdlib.h>
#include <uci/uci.h>
#include <uci_transport/uci_transport_hsspi.h>
#include <unistd.h>

static void hsspi_attach(struct uci_transport *tr, struct uci *uci);
static void hsspi_detach(struct uci_transport *tr);
static void hsspi_packet_send_ready(struct uci_transport *tr);
static void hsspi_send_raw(struct uci_transport *tr, struct uci_blk *p);
static void uci_transport_hsspi_read_cb(void *user_data, uint8_t *rx_buffer,
					uint16_t len);

static struct uci_transport_ops g_transport_hsspi_ops = {
	.attach = hsspi_attach,
	.detach = hsspi_detach,
	.packet_send_ready = hsspi_packet_send_ready,
	.packet_send_raw = hsspi_send_raw,
};

static struct uci_transport g_transport_hsspi = {
	.ops = &g_transport_hsspi_ops,
};

void hsspi_send_raw(struct uci_transport *tr, struct uci_blk *p)
{
	//TODO In a first step, copy the buffer. Further implementation would to provide
	// an allocator/deallocator to the HSSPI async, per UL.
	uint8_t *tx_buffer = (uint8_t *)malloc(p->len);
	memcpy(tx_buffer, p->data, p->len);
	hsspi_async_write(UL_VALUE_UCI, tx_buffer, p->len);
}

static void hsspi_attach(struct uci_transport *tr, struct uci *uci)
{
	struct uci_transport_hsspi *s =
		qparent_of(tr, struct uci_transport_hsspi, base);
	s->uci = uci;
}

static void hsspi_detach(struct uci_transport *tr)
{
	struct uci_transport_hsspi *s =
		qparent_of(tr, struct uci_transport_hsspi, base);
	s->uci = NULL;
}

static void hsspi_packet_send_ready(struct uci_transport *tr)
{
	struct uci_transport_hsspi *s =
		qparent_of(tr, struct uci_transport_hsspi, base);
	struct uci_blk *p;
	int r = 0;

	while ((p = uci_packet_send_get_ready(s->uci))) {
		//TODO In a first step, copy the buffer. Further implementation would to provide
		// an allocator/deallocator to the HSSPI async, per UL.
		uint8_t *tx_buffer = (uint8_t *)malloc(p->len);
		memcpy(tx_buffer, p->data, p->len);
		r = hsspi_async_write(UL_VALUE_UCI, tx_buffer, p->len);
		uci_packet_send_done(s->uci, p, r);
	}
}

struct uci_transport *uci_transport_hsspi_create(void)
{
	struct uci_transport_hsspi *transport = NULL;

	/* Allocate resources */
	transport = calloc(1, sizeof(*transport));
	if (!transport) {
		return NULL;
	}

	transport->base = g_transport_hsspi;
	hsspi_read_callback_user_data_set(
		UL_VALUE_UCI, uci_transport_hsspi_read_cb, transport);
	return &transport->base;
}

void uci_transport_hsspi_destroy(struct uci_transport *tr)
{
	struct uci_transport_hsspi *s =
		qparent_of(tr, struct uci_transport_hsspi, base);

	hsspi_read_callback_user_data_unset(UL_VALUE_UCI);
	free(s);
}

static void uci_transport_hsspi_read_cb(void *user_data, uint8_t *rx_buffer,
					uint16_t len)
{
	struct uci_transport_hsspi *s = (struct uci_transport_hsspi *)user_data;
	// TODO: here packet globbing is not supported. We expect the UWBS to send one and
	// only one UCI packet per HSSPI transaction.
	/* Alloc new packet */
	struct uci_blk *p = uci_packet_recv_alloc(s->uci, UCI_MAX_PACKET_SIZE);
	if (!p) {
		free(rx_buffer);
		return;
	}
	if (p->size < len) {
		goto err;
	}
	p->len = len;
	memcpy(p->data, rx_buffer, len);
	free(rx_buffer);
	/* Give packet to uci */
	uci_packet_recv(s->uci, p);
	return;

err:
	uci_packet_recv_free_all(s->uci, p);
	free(rx_buffer);
	return;
}
