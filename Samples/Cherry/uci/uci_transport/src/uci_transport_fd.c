/*
 * Implementation of uci transport on fd
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#define _GNU_SOURCE 1
#define MAX_EVENTS 1
#define TIMEOUT_MS 100

#include "uci_transport/uci_transport_fd.h"

#include "uci/uci_message.h"

#include <errno.h>
#include <qlog.h>
#include <qtils.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <time.h>
#include <uci/uci.h>
#include <unistd.h>

static void fd_attach(struct uci_transport *tr, struct uci *uci);
static void fd_detach(struct uci_transport *tr);
static void fd_packet_send_ready(struct uci_transport *tr);
static void fd_send_raw(struct uci_transport *tr, struct uci_blk *p);

static struct uci_transport_ops g_transport_fd_ops = {
	.attach = fd_attach,
	.detach = fd_detach,
	.packet_send_ready = fd_packet_send_ready,
	.packet_send_raw = fd_send_raw,
};

static struct uci_transport g_transport_fd = {
	.ops = &g_transport_fd_ops,
};

static int create_epoll_ctx(int fd, uint32_t events)
{
	struct epoll_event ev;
	int epollfd;

	if ((epollfd = epoll_create1(0)) == -1) {
		QLOGE("epoll_create1() failed with error : %d", errno);
		return -1;
	}

	ev.events = events;
	ev.data.fd = fd;

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
		QLOGE("epoll_ctl() failed with error : %d", errno);
		close(epollfd);
		return -1;
	}

	return epollfd;
}

static struct timeval abs_timeout_create(struct timeval *abs_tv, int timeout_ms)
{
	struct timespec current_ts;
	struct timeval current_tv, timeout_tv;

	timeout_tv.tv_usec = (timeout_ms % 1000) * 1000;
	timeout_tv.tv_sec = timeout_ms / 1000;
	clock_gettime(CLOCK_MONOTONIC, &current_ts);
	TIMESPEC_TO_TIMEVAL(&current_tv, &current_ts);
	timeradd(&current_tv, &timeout_tv, abs_tv);
	return timeout_tv;
}

static void abs_timeout_update(struct timeval *abs_tv,
			       struct timeval *timeout_tv)
{
	struct timespec current_ts;
	struct timeval current_tv;

	clock_gettime(CLOCK_MONOTONIC, &current_ts);
	TIMESPEC_TO_TIMEVAL(&current_tv, &current_ts);
	timersub(abs_tv, &current_tv, timeout_tv);
}

int fd_read(struct uci_transport_fd *s, uint8_t *data, uint16_t len,
	    int timeout_ms)
{
	struct epoll_event event;
	int n, nfds;
	struct timeval timeout_tv, abs_tv;

	timeout_tv = abs_timeout_create(&abs_tv, timeout_ms);
	while (len) {
		abs_timeout_update(&abs_tv, &timeout_tv);
		do {
			nfds = epoll_wait(s->epollfd_rd, &event, MAX_EVENTS,
					  timeout_tv.tv_sec * 1000 +
						  timeout_tv.tv_usec / 1000);
		} while (nfds == -1 && errno == EINTR);
		if (nfds == -1) {
			QLOGE("%s: epoll_wait() error: %s.", __func__,
			      strerror(errno));
			goto error;
		} else if (nfds == 0) {
			QLOGE("%s: epoll_wait() reached timeout for read",
			      __func__);
			break;
		}

		n = read(s->fd, data, len);
		if (n == 0 || (n == -1 && errno == EINTR))
			break;
		if (n > 0) {
			len -= n;
			data += n;
		}
	}
error:
	return len ? -1 : 0;
}

int fd_write(struct uci_transport_fd *s, struct uci_blk *p, int timeout_ms)
{
	struct epoll_event event;
	int n, nfds;
	struct timeval timeout_tv, abs_tv;
	const uint8_t *data = p->data;
	uint16_t len = p->len;

	timeout_tv = abs_timeout_create(&abs_tv, timeout_ms);
	while (len) {
		abs_timeout_update(&abs_tv, &timeout_tv);
		do {
			nfds = epoll_wait(s->epollfd_wr, &event, MAX_EVENTS,
					  timeout_tv.tv_sec * 1000 +
						  timeout_tv.tv_usec / 1000);
		} while (nfds == -1 && errno == EINTR);

		if (nfds == -1) {
			QLOGE("epoll_wait() failed with error : %d", errno);
			goto error;
		} else if (nfds == 0) {
			QLOGE("epoll_wait() reached timeout for write");
			break;
		}

		n = write(s->fd, data, len);
		if (n == 0 || (n == -1 && errno == EINTR))
			break;
		if (n > 0) {
			len -= n;
			data += n;
		}
	}
error:
	return len ? -1 : 0;
}

void fd_send_raw(struct uci_transport *tr, struct uci_blk *p)
{
	struct uci_transport_fd *s =
		qparent_of(tr, struct uci_transport_fd, base);

	fd_write(s, p, TIMEOUT_MS);
}

static void fd_attach(struct uci_transport *tr, struct uci *uci)
{
	struct uci_transport_fd *s =
		qparent_of(tr, struct uci_transport_fd, base);

	s->uci = uci;
}

static void fd_detach(struct uci_transport *tr)
{
	struct uci_transport_fd *s =
		qparent_of(tr, struct uci_transport_fd, base);

	s->uci = NULL;
}

static void fd_packet_send_ready(struct uci_transport *tr)
{
	struct uci_transport_fd *s =
		qparent_of(tr, struct uci_transport_fd, base);
	struct uci_blk *p;
	int r = 0;

	while ((p = uci_packet_send_get_ready(s->uci))) {
		r = fd_write(s, p, TIMEOUT_MS);
		uci_packet_send_done(s->uci, p, r);
	}
}

struct uci_transport *uci_transport_fd_create(int fd)
{
	struct uci_transport_fd *transport = NULL;

	/* Allocate resources */
	transport = calloc(1, sizeof(*transport));
	if (!transport) {
		return NULL;
	}

	transport->base = g_transport_fd;
	transport->fd = fd;

	if ((transport->epollfd_wr =
		     create_epoll_ctx(transport->fd, EPOLLOUT)) == -1)
		QLOGD("%s : Epoll fd for writing creation failed.");
	if ((transport->epollfd_rd =
		     create_epoll_ctx(transport->fd, EPOLLIN)) == -1)
		QLOGD("%s : Epoll fd for writing creation failed.");

	return &transport->base;
}

void uci_transport_fd_destroy(struct uci_transport *tr)
{
	struct uci_transport_fd *s =
		qparent_of(tr, struct uci_transport_fd, base);
	close(s->epollfd_rd);
	close(s->epollfd_wr);

	free(s);
}

int uci_transport_fd_read(struct uci_transport *tr)
{
	struct uci_transport_fd *s =
		qparent_of(tr, struct uci_transport_fd, base);
	ssize_t n = 0;
	uint16_t payload_len = 0;

	/* Alloc new packet */
	struct uci_blk *p = uci_packet_recv_alloc(s->uci, UCI_MAX_PACKET_SIZE);
	if (!p)
		return -ENOMEM;
	if (p->size < UCI_MAX_PACKET_SIZE) {
		n = -ENOMEM;
		goto err;
	}

	/* Read header.
	 * If the read() call is interrupted by a signal, retry.
	 */
	if (fd_read(s, p->data, UCI_PACKET_HEADER_SIZE, TIMEOUT_MS)) {
		n = -EBADMSG;
		goto err;
	}

	/* Read payload.
	 * If the read() call is interrupted by a signal, retry.
	 */
	payload_len = uci_packet_hdr_get_payload_size(p->data);
	if (payload_len) {
		if (fd_read(s, p->data + UCI_PACKET_HEADER_SIZE, payload_len,
			    TIMEOUT_MS)) {
			QLOGD("Could not read the full payload length");
			n = -EBADMSG;
			goto err;
		} else
			n = payload_len;
	}

	p->len = UCI_PACKET_HEADER_SIZE + payload_len;

	/* Give packet to uci */
	uci_packet_recv(s->uci, p);
	return n;

err:
	uci_packet_recv_free_all(s->uci, p);
	return n;
}
