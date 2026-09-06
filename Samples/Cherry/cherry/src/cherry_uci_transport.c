/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "cherry_uci_transport.h"

#include "cherry_log.h"

#include <errno.h>
#include <qmalloc.h>
#include <qthread.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define POLL_TIMEOUT 500 /* ms */
#define MAX_EVENTS 1

#ifndef CONFIG_USE_QMUTILS

#include "uci_transport/uci_transport_chardev.h"

static int client_reset(int fd)
{
	unsigned int state;
	int rc = ioctl(fd, QM35_CTRL_RESET, &state);
	if (rc < 0)
		QLOGE("reset ioctl error: %s", strerror(errno));
	return rc;
}

#define uci_transport_chardev_reset(ctx) client_reset((ctx)->uci_fd)

#else

#include "uci_transport/uci_transport_qmutils.h"

/* Call qmutils functions instead of chardev. */
#define uci_transport_chardev_create(a, b) uci_transport_qmutils_create(b)
#define uci_transport_chardev_destroy(tr) uci_transport_qmutils_destroy(tr)
#define uci_transport_chardev_read(tr) uci_transport_qmutils_read(tr)
#define uci_transport_chardev_reset(ctx) \
	uci_transport_qmutils_reset((ctx)->transport)

#endif

static void uci_read_thread_hdl(void *arg)
{
	int epoll_fd, nfds, rc;
	struct cherry_uci_transport *ctx;
	struct epoll_event ev, events[MAX_EVENTS];
	ssize_t nread;
	ctx = (struct cherry_uci_transport *)arg;

	epoll_fd = epoll_create1(0);
	if (epoll_fd < 0) {
		QLOGE("Failed epoll_creat1, errno = %d", errno);
		return;
	}

	ev.events = EPOLLIN;
	ev.data.fd = ctx->uci_fd;

	if ((rc = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ctx->uci_fd, &ev)) < 0) {
		QLOGE("Failed epoll_ctl, rc = %d - errno %d", rc, errno);
		goto close_epollfd;
	}

	while (ctx->reading) {
		/* If the epoll_wait() call is interrupted by a signal, retry. */
		do {
			nfds = epoll_wait(epoll_fd, events, MAX_EVENTS,
					  POLL_TIMEOUT);
		} while (nfds == -1 && errno == EINTR);

		if (nfds == -1) {
			QLOGE("%s: epoll_wait() error: %s.", __func__,
			      strerror(errno));
		}

		for (int i = 0; i < nfds; i++) {
			if (events[i].data.fd == ctx->uci_fd) {
				if (!ctx->serial)
					nread = uci_transport_chardev_read(
						ctx->transport);
				else
					nread = uci_transport_serial_read(
						ctx->transport);
				if (nread < 0 && nread != -EAGAIN)
					QLOGE("uci_transport_chardev_read rc = %d",
					      nread);
			}
		}
	}
close_epollfd:
	close(epoll_fd);
}

enum cherry_err cherry_init_transport(struct cherry_uci_transport *ctx,
				      struct uci *uci, const char *device)
{
	ctx->uci = uci;

	/* Initialization of the UCI transport char dev */
	if (strstr(device, "uci")) {
		ctx->serial = false;
		ctx->transport =
			uci_transport_chardev_create(device, &ctx->uci_fd);
	} else {
		ctx->serial = true;
		ctx->transport = uci_transport_serial_create(device, B115200,
							     &ctx->uci_fd);
	}

	if (!ctx->transport) {
		QLOGE("%s: Failed to create UCI Transport Char Dev. Verify %s exists ",
		      __func__, device);
		goto uci_err;
	}
	/* Attach the UCI client to the UCI transport char dev */
	if (uci_transport_attach(uci, ctx->transport) != QERR_SUCCESS) {
		QLOGE("%s: Failed uci_transport_attach", __func__);
		goto chardev_destroy;
	}

	ctx->reading = true;

	if ((ctx->reader_thread = qthread_create(
		     uci_read_thread_hdl, ctx, "Cherry uci transport thread",
		     NULL, 0, QTHREAD_PRIORITY_NORMAL)) == NULL) {
		QLOGE("%s: qthread_create failed.", __func__);
		goto transport_detach;
	}
	return CHERRY_ERR_NONE;

transport_detach:
	uci_transport_detach(uci);
chardev_destroy:
	if (!ctx->serial)
		uci_transport_chardev_destroy(ctx->transport);
	else
		uci_transport_serial_destroy(ctx->transport);
uci_err:

	return CHERRY_ERR_INTERNAL;
}

int cherry_client_reset(struct cherry_uci_transport *ctx)
{
	if (ctx->serial)
		return -ENOTSUP;

	/* Reset the UCI client */
	return uci_transport_chardev_reset(ctx);
}

void cherry_de_init_transport(struct cherry_uci_transport *ctx)
{
	/* Stop the pthread */
	if (ctx->reader_thread) {
		ctx->reading = false;
		qthread_join(ctx->reader_thread);
		qthread_delete(ctx->reader_thread);
	}

	/* Detach the UCI client from the UCI transport char dev*/
	uci_transport_detach(ctx->uci);

	/* Suppresion of the UCI transport char dev */
	if (ctx->transport) {
		if (!ctx->serial)
			uci_transport_chardev_destroy(ctx->transport);
		else
			uci_transport_serial_destroy(ctx->transport);
	}

	return;
}
