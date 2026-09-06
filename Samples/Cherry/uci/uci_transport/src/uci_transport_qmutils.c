/*
 * Implementation for uci transport using qm-utils library
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#define _GNU_SOURCE 1

#include "uci_transport/uci_transport_qmutils.h"

extern unsigned int uci_log_level;
#define LOG_TAG "qorvo.uwb.uci_transport"
#define QLOG_CURRENT_LEVEL uci_log_level
#include <qlog.h>
#include <qmutils/qmchannel.h>
#include <qmutils/qmutils.h>
#include <qtils.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <uci/uci.h>

/* forward declarations */
static void qmu_attach(struct uci_transport *tr, struct uci *uci);
static void qmu_detach(struct uci_transport *tr);
static void qmu_packet_send_ready(struct uci_transport *tr);
static void qmu_send_raw(struct uci_transport *tr, struct uci_blk *p);

static int qmu_uci_channel_callback(void *cb_data, uint8_t *buf, size_t len);

static struct uci_transport_ops g_transport_qmutils_ops = {
	.attach = qmu_attach,
	.detach = qmu_detach,
	.packet_send_ready = qmu_packet_send_ready,
	.packet_send_raw = qmu_send_raw,
};

static struct uci_transport g_transport_qmutils = {
	.ops = &g_transport_qmutils_ops,
};

struct uci_transport *uci_transport_qmutils_create(int *fd_out)
{
	struct uci_transport_qmutils *transport = NULL;
	int ret;

	/* Allocate resources */
	transport = calloc(1, sizeof(*transport));
	if (!transport) {
		return NULL;
	}

	transport->base = g_transport_qmutils;
	transport->uci_channel = qmu_uci_init(
		(qmchannel_callback)qmu_uci_channel_callback, transport);
	if (!transport->uci_channel) {
		free(transport);
		return NULL;
	}
	/* The returned fd (for opened /dev/uci0) support POLL_RD event. */
	ret = qmchannel_getfd(transport->uci_channel);
	if (ret >= 0)
		*fd_out = ret;
	return &transport->base;
}

void uci_transport_qmutils_destroy(struct uci_transport *tr)
{
	struct uci_transport_qmutils *s =
		qparent_of(tr, struct uci_transport_qmutils, base);

	if (s->uci_pkt) {
		QLOGW("Transport destroyed without being detached!");
		uci_packet_recv_free_all(s->uci, s->uci_pkt);
		s->uci_pkt = NULL;
	}
	qmu_uci_destroy(s->uci_channel);
	free(s);
}

int uci_transport_qmutils_read(struct uci_transport *tr)
{
	struct uci_transport_qmutils *s =
		qparent_of(tr, struct uci_transport_qmutils, base);
	/* Since this function is called only when POLL_RD event occurs, the
	 * following call will never block and should not fail (except if
	 * channel is closed or signal is received). */
	return qmchannel_read(s->uci_channel);
}

int uci_transport_qmutils_reset(struct uci_transport *tr)
{
	struct uci_transport_qmutils *s =
		qparent_of(tr, struct uci_transport_qmutils, base);
	return qmu_reset(s->uci_channel, false);
}

static void qmu_attach(struct uci_transport *tr, struct uci *uci)
{
	struct uci_transport_qmutils *s =
		qparent_of(tr, struct uci_transport_qmutils, base);
	struct uci_blk *p;

	s->uci = uci;
	p = uci_packet_recv_alloc(s->uci, UCI_MAX_PACKET_SIZE);
	if (p) {
		qmchannel_setbuf(s->uci_channel, p->data, p->size);
		s->uci_pkt = p;
	}
}

static void qmu_detach(struct uci_transport *tr)
{
	struct uci_transport_qmutils *s =
		qparent_of(tr, struct uci_transport_qmutils, base);
	if (!s->uci) {
		QLOGW("Transport detached without being attached!");
		return;
	}
	if (s->uci_pkt) {
		qmchannel_setbuf(s->uci_channel, NULL, 0);
		uci_packet_recv_free_all(s->uci, s->uci_pkt);
		s->uci_pkt = NULL;
	}
	s->uci = NULL;
}

static void qmu_packet_send_ready(struct uci_transport *tr)
{
	struct uci_transport_qmutils *s =
		qparent_of(tr, struct uci_transport_qmutils, base);
	struct uci_blk *p;
	int r = 0;

	while ((p = uci_packet_send_get_ready(s->uci))) {
		r = qmchannel_write(s->uci_channel, p->data, p->len);
		/* The status given to uci_packet_send_done() shall be 0 if
		 * there is no error. As qmchannel_write() returns the number
		 * of written bytes or a negative error code, we need to
		 * translate it.
		 */
		uci_packet_send_done(s->uci, p, r < 0 ? r : 0);
	}
}

static void qmu_send_raw(struct uci_transport *tr, struct uci_blk *p)
{
	struct uci_transport_qmutils *s =
		qparent_of(tr, struct uci_transport_qmutils, base);

	qmchannel_write(s->uci_channel, p->data, p->len);
}

static int qmu_uci_channel_callback(void *cb_data, uint8_t *buf, size_t len)
{
	struct uci_transport_qmutils *s =
		(struct uci_transport_qmutils *)cb_data;

	/* The qmchannel_read() calls this callback with a full HSSPI packet
	 * which may contains multiple UCI packets. */

	while (len >= UCI_PACKET_HEADER_SIZE) {
		size_t uci_msg_len = UCI_PACKET_HEADER_SIZE +
				     uci_packet_hdr_get_payload_size(buf);
		struct uci_blk *p = s->uci_pkt;

		/* UCI message is truncated! */
		if (len < uci_msg_len)
			break;

		/* Allocate a new uci_blk if needed. */
		if (!p || uci_msg_len < len) {
			/* Something received while no pre-allocated buffer!
			 * Or buffer contains multiple UCI packets. */
			p = uci_packet_recv_alloc(s->uci, uci_msg_len);
			if (!p)
				break;
			memcpy(p->data, buf, uci_msg_len);
		}
		buf += uci_msg_len;
		len -= uci_msg_len;

		/* Handle this UCI message. */
		p->len = uci_msg_len;
		uci_packet_recv(s->uci, p);

		if (p == s->uci_pkt) {
			/* Pre-allocated packet was sent to UCI,
			 * create and setup a new one. */
			p = uci_packet_recv_alloc(s->uci, UCI_MAX_PACKET_SIZE);
			qmchannel_setbuf(s->uci_channel, p->data, p->size);
			s->uci_pkt = p;
		}
	}
	if (len) {
		QLOGW("Truncated UCI packet (%d bytes remain)", len);
	}
	return 0;
}
