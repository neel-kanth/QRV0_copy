/*
 * Implementation for uart uci transport.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#define _GNU_SOURCE 1

#include <cherry/cherry_proxy.h>
#include <uci_transport/uci_transport_uart.h>
extern unsigned int uci_log_level;
#define LOG_TAG "qorvo.uwb.uci_transport"
#define QLOG_CURRENT_LEVEL uci_log_level
#include <qlog.h>
#include <qmalloc.h>
#include <qtils.h>
#include <stdint.h>
#include <uci/uci.h>

static void uci_transport_uart_attach(struct uci_transport *tr,
				      struct uci *uci);
static void uci_transport_uart_detach(struct uci_transport *tr);
static void uci_transport_uart_packet_send_ready(struct uci_transport *tr);
static void uci_transport_uart_send_raw(struct uci_transport *tr,
					struct uci_blk *p);

static struct uci_transport_ops g_uci_transport_uart_ops = {
	.attach = uci_transport_uart_attach,
	.detach = uci_transport_uart_detach,
	.packet_send_ready = uci_transport_uart_packet_send_ready,
	.packet_send_raw = uci_transport_uart_send_raw,
};

static struct uci_transport g_uci_transport_uart = {
	.ops = &g_uci_transport_uart_ops,
};

/* Zephyr specific. */
#ifdef CONFIG_UCI_ZEPHYR
#include <shell_utils.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

void uci_transport_uart_callback(const struct device *dev, void *user_data)
{
	struct uci_transport_uart *transport_uart =
		(struct uci_transport_uart *)user_data;
	int ret;

	uart_irq_update(dev);
	if (uart_irq_rx_ready(dev)) {
		/* On some platform (nrf52840), in uart interrupt driven, only one byte is read each time. */
		do {
			ret = uart_fifo_read(
				dev,
				&transport_uart->uci_pkt
					 ->data[transport_uart->uart_data_len],
				1);
			/* Error or no data. */
			if (ret <= 0) {
				return;
			}
			transport_uart->uart_data_len += ret;
			if (transport_uart->uart_data_len ==
			    UCI_PACKET_HEADER_SIZE) {
				if (*(uint32_t *)transport_uart->uci_pkt->data ==
				    0xFFFFFFFF) {
					/* The magic 0xFFFFFFFF is received, set the exit flag and process it. */
					transport_uart->exit = true;
					k_work_submit(
						&transport_uart
							 ->work_uart_packet_received);
				} else {
					/* The uci packet header is fully received. */
					transport_uart->uci_pkt->len =
						UCI_PACKET_HEADER_SIZE +
						uci_packet_hdr_get_payload_size(
							transport_uart->uci_pkt
								->data);
				}
			}
			if (transport_uart->uart_data_len ==
			    transport_uart->uci_pkt->len) {
				/* The entire uci packet is received, send it via the work queue. */
				transport_uart->uci_pkt_ready =
					transport_uart->uci_pkt;
				k_work_submit(
					&transport_uart
						 ->work_uart_packet_received);
				/* Pre-allocate a new uci packet for the next packet . */
				transport_uart->uci_pkt = uci_packet_recv_alloc(
					transport_uart->uci,
					UCI_MAX_PACKET_SIZE);
				if (transport_uart->uci_pkt == NULL) {
					QLOGE("uci_packet_recv_alloc error in %s",
					      __func__);
					return;
				}
				transport_uart->uart_data_len = 0;
			}
		} while (true);
	}
}

static int
uci_transport_uart_cherry_proxy_stop(struct uci_transport_uart *transport_uart)
{
	int ret;

	/* Stop and destroy the cherry proxy. */
	ret = cherry_proxy_stop(transport_uart->cherry_proxy_context);
	if (ret != CHERRY_ERR_NONE) {
		QLOGE("cherry_proxy_stop has failed !!! (err = %d)", ret);
		return -EINVAL;
	}
	cherry_proxy_destroy(transport_uart->cherry_proxy_context);
	/* Reconfigure the shell uart and restart the shell. */
	ret = shell_uart_configure(1000000, true);
	shell_start_and_uart_release();
	QLOGI("The uci bridge is stopped and the shell is restarted on the zephyr_shell_uart");
	return 0;
}

static void uci_transport_uart_packet_received_zephyr(struct k_work *work)
{
	struct uci_transport_uart *transport_uart = qparent_of(
		work, struct uci_transport_uart, work_uart_packet_received);

	if (transport_uart->exit) {
		/* Exit if requested. */
		uci_transport_uart_cherry_proxy_stop(transport_uart);
	} else {
		/* Handle this UCI message. */
		uci_packet_recv(transport_uart->uci,
				transport_uart->uci_pkt_ready);
	}
}

static int uci_transport_uart_write_zephyr(struct uci_transport *tr, void *buf,
					   size_t len)
{
	struct uci_transport_uart *transport_uart =
		qparent_of(tr, struct uci_transport_uart, base);
	for (int i = 0; i < len; i++) {
		uart_poll_out(transport_uart->uart, ((unsigned char *)buf)[i]);
	}
	return 0;
}

#endif /* CONFIG_UCI_ZEPHYR */

static int uci_transport_uart_write(struct uci_transport *tr, void *buf,
				    size_t len)
{
#ifdef CONFIG_UCI_ZEPHYR
	return uci_transport_uart_write_zephyr(tr, buf, len);
#endif /* CONFIG_UCI_ZEPHYR */
	/* Function not implemented on other OS than Zephyr. */
	return -ENOSYS; /* Or ENOTSUP ? */
}

struct uci_transport *
uci_transport_uart_create(struct cherry_proxy *cherry_proxy_context)
{
	struct uci_transport_uart *transport_uart = NULL;
	int ret;

	/* Allocate resources */
	transport_uart = qcalloc(1, sizeof(*transport_uart));
	if (!transport_uart) {
		return NULL;
	}
	transport_uart->base = g_uci_transport_uart;
	transport_uart->cherry_proxy_context = cherry_proxy_context;
#ifdef CONFIG_UCI_ZEPHYR
	k_work_init(&transport_uart->work_uart_packet_received,
		    uci_transport_uart_packet_received_zephyr);
	transport_uart->uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
	if (!device_is_ready(transport_uart->uart)) {
		QLOGE("Uart device %s is not ready",
		      transport_uart->uart->name);
		qfree(transport_uart);
		return NULL;
	}
	ret = shell_uart_configure(115200, false);
	if (ret) {
		QLOGE("Could not configure uart device %s: %d",
		      transport_uart->uart->name, ret);
		qfree(transport_uart);
		return NULL;
	}
#endif /* CONFIG_UCI_ZEPHYR */
	return &transport_uart->base;
}

void uci_transport_uart_destroy(struct uci_transport *tr)
{
	struct uci_transport_uart *transport_uart =
		qparent_of(tr, struct uci_transport_uart, base);

	if (transport_uart->uci_pkt) {
		QLOGW("Transport destroyed without being detached!");
		uci_packet_recv_free_all(transport_uart->uci,
					 transport_uart->uci_pkt);
		transport_uart->uci_pkt = NULL;
	}
	qfree(transport_uart);
}

static void uci_transport_uart_attach(struct uci_transport *tr, struct uci *uci)
{
	struct uci_transport_uart *transport_uart =
		qparent_of(tr, struct uci_transport_uart, base);
	int ret;

	transport_uart->uci = uci;
	/* Pre-allocate an UCI packet. */
	transport_uart->uci_pkt =
		uci_packet_recv_alloc(transport_uart->uci, UCI_MAX_PACKET_SIZE);
	if (transport_uart->uci_pkt == NULL) {
		QLOGE("uci_packet_recv_alloc error in %s", __func__);
		return;
	}
	transport_uart->uart_data_len = 0;
	/* Connect uart to user_uart_callback. */
	ret = uart_irq_callback_user_data_set(transport_uart->uart,
					      uci_transport_uart_callback,
					      transport_uart);
	if (ret != 0) {
		QLOGE("Failed to configure the irq user callback %d", ret);
	}
	/* Disable uart tx irq: uart tx use the blocking poll api uart_poll_out(). */
	uart_irq_tx_disable(transport_uart->uart);
	uart_irq_rx_enable(transport_uart->uart);
}

static void uci_transport_uart_detach(struct uci_transport *tr)
{
	struct uci_transport_uart *transport_uart =
		qparent_of(tr, struct uci_transport_uart, base);
	if (!transport_uart->uci) {
		QLOGW("Transport detached without being attached!");
		return;
	}
	if (transport_uart->uci_pkt) {
		uci_packet_recv_free_all(transport_uart->uci,
					 transport_uart->uci_pkt);
		transport_uart->uci_pkt = NULL;
	}
	transport_uart->uci = NULL;
}

static void uci_transport_uart_packet_send_ready(struct uci_transport *tr)
{
	struct uci_transport_uart *transport_uart =
		qparent_of(tr, struct uci_transport_uart, base);
	struct uci_blk *p;
	int r = 0;

	while ((p = uci_packet_send_get_ready(transport_uart->uci))) {
		r = uci_transport_uart_write(tr, p->data, p->len);
		uci_packet_send_done(transport_uart->uci, p, r < 0 ? r : 0);
	}
}

static void uci_transport_uart_send_raw(struct uci_transport *tr,
					struct uci_blk *p)
{
	uci_transport_uart_write(tr, p->data, p->len);
}
