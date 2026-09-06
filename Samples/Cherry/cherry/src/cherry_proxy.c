/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "cherry/cherry_proxy.h"

#include "cherry/cherry.h"
#include "cherry_calib_client.h"
#include "cherry_log.h"
#include "cherry_priv.h"
#include "cherry_proxy_client.h"
#include "uci/uci.h"

#include <errno.h>
#include <qmalloc.h>
#include <qsemaphore.h>
#include <qthread.h>
#include <qtime.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef CONFIG_CHERRY_ZEPHYR
#include "uci_transport/uci_transport_serial.h"

#include <signal.h>
#include <sys/epoll.h>
#endif

#if defined(CONFIG_USE_QMUTILS) || defined(CONFIG_CHERRY_ZEPHYR)
#include "uci_transport/uci_transport_qmutils.h"
/* Call qmutils functions instead of chardev. */
#define uci_transport_chardev_create(a, b) uci_transport_qmutils_create(b)
#define uci_transport_chardev_destroy(tr) uci_transport_qmutils_destroy(tr)
#define uci_transport_chardev_read(tr) uci_transport_qmutils_read(tr)
#else
#include "uci_transport/uci_transport_chardev.h"
#endif
#ifdef CONFIG_CHERRY_ZEPHYR
#include "uci_transport/uci_transport_uart.h"
/* Call uart transport function instead of serial. */
#define uci_transport_serial_destroy(tr) uci_transport_uart_destroy(tr)
#endif

#define POLL_TIMEOUT 10 /* ms */
#define MAX_EVENTS 2
#define MAX_ERRORS 5

#ifndef CONFIG_CHERRY_PROXY_STACK_SIZE
#define CONFIG_CHERRY_PROXY_STACK_SIZE 4096
#endif
QTHREAD_STACK_DEFINE(cherry_proxy_stack, CONFIG_CHERRY_PROXY_STACK_SIZE);

/**
 * struct cherry_proxy_transport - Cherry proxy transport structure.
 * It holds the Cherry transport and associated proxy_ctx.
 */
struct cherry_proxy_transport {
	struct cherry_proxy_context *proxy_ctx;
	struct uci_transport *tr;
	int fd;
};

struct cherry_set_calib_params {
	bool reload;
	const struct cherry_calib *calib;
};

struct cherry_proxy {
	struct cherry_proxy_transport vcom, device;
	const struct cherry_calib *calib;
	struct qthread *proxy_thread;
	struct qthread *reader_thread;
	enum cherry_core_state_change_reason boot_reason;
	bool reading;
	bool stop;
	bool reboot;
	bool aborted;
};

/* Proxy thread to reload automatically the calibration after reboot. */
static void proxy_thread_hdl(void *arg);

enum cherry_err cherry_proxy_set_log_level(struct cherry_proxy *ctx,
					   enum cherry_log_level level,
					   enum cherry_log_module module)
{
	if (module == CHERRY_LOG_MODULE_ALL) {
		cherry_log_level = level;
		uci_set_log_level(level);
	} else
		return CHERRY_ERR_INVALID_PARAMETER;

	return CHERRY_ERR_NONE;
}

#ifndef CONFIG_CHERRY_ZEPHYR
static void proxy_read_thread_hdl(void *arg)
{
	struct cherry_proxy *ctx = (struct cherry_proxy *)arg;
	int epoll_fd, nfds, rc;
	struct epoll_event ev, events[MAX_EVENTS];
	ssize_t nread;
	unsigned errors = 0;

	epoll_fd = epoll_create1(0);
	if (epoll_fd < 0) {
		QLOGE("Failed epoll_creat1, errno = %d.", errno);
		return;
	}

	ev.events = EPOLLIN;
	ev.data.fd = ctx->device.fd;
	if ((rc = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ctx->device.fd, &ev)) <
	    0) {
		QLOGE("Failed epoll_ctl uci, rc = %d - errno %d.", rc, errno);
		goto close_epollfd;
	}

	ev.events = EPOLLIN;
	ev.data.fd = ctx->vcom.fd;
	if ((rc = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ctx->vcom.fd, &ev)) < 0) {
		QLOGE("Failed epoll_ctl vcom, rc = %d - errno %d.", rc, errno);
		goto close_epollfd;
	}

	while (ctx->reading) {
		/* If the epoll_wait() call is interrupted by a signal, retry. */
		nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, POLL_TIMEOUT);
		if (nfds == -1 && errno == EINTR)
			continue;
		if (nfds == -1) {
			QLOGE("%s: epoll_wait() error: %s.", __func__,
			      strerror(errno));
			continue;
		}

		for (int i = 0; i < nfds; i++) {
			if (events[i].data.fd == ctx->device.fd) {
				nread = uci_transport_chardev_read(
					ctx->device.tr);
				if (nread < 0 && nread != -EAGAIN) {
					QLOGE("%s: uci_transport_chardev_read rc = %d.",
					      __func__, nread);
					errors++;
				} else {
					errors = 0;
				}
			}
			if (events[i].data.fd == ctx->vcom.fd) {
				nread = uci_transport_serial_read(ctx->vcom.tr);
				if (nread < 0 && nread != -EAGAIN) {
					QLOGE("%s: uci_transport_serial_read rc = %d.",
					      __func__, nread);
					errors++;
				} else {
					errors = 0;
				}
			}
		}
		if (errors == MAX_ERRORS) {
			/* Too many error in a raw, send signal to kill ourself properly. */
			ctx->aborted = true;
			kill(0, SIGTERM);
		}
	}
close_epollfd:
	close(epoll_fd);
}
#endif /* CONFIG_CHERRY_ZEPHYR */

enum cherry_err cherry_proxy_init_transport(struct cherry_proxy *ctx,
					    const char *device,
					    const char *vcom,
					    const speed_t baud_rate)
{
	/* Initialization of the UCI transport char dev. */
	ctx->device.tr = uci_transport_chardev_create(device, &ctx->device.fd);
	if (!ctx->device.tr) {
		QLOGE("%s: Failed to create UCI Transport Char Dev. Verify %s exists.",
		      __func__, device);
		goto uci_err;
	}

	/* Initialization of the Proxy transport. */
	ctx->vcom.tr =
#ifndef CONFIG_CHERRY_ZEPHYR
		uci_transport_serial_create(vcom, baud_rate, &ctx->vcom.fd);
#else
		uci_transport_uart_create(ctx);
#endif
	if (!ctx->vcom.tr) {
		QLOGE("%s: Failed to create Proxy Transport. Verify %s exists.",
		      __func__, vcom);
		goto uci_transport_destroy;
	}

	return CHERRY_ERR_NONE;

uci_transport_destroy:
	uci_transport_chardev_destroy(ctx->device.tr);
uci_err:
	return CHERRY_ERR_INTERNAL;
}

void cherry_proxy_de_init_transport(struct cherry_proxy *ctx)
{
	/* Suppression of the UCI transport char dev. */
	if (ctx->device.tr) {
		uci_transport_chardev_destroy(ctx->device.tr);
		ctx->device.tr = NULL;
	}
	/* Suppression of the Proxy transport. */
	if (ctx->vcom.tr) {
		uci_transport_serial_destroy(ctx->vcom.tr);
		ctx->vcom.tr = NULL;
	}
}

/* Boot notification. */
static void cherry_proxy_boot_cb(const enum uci_qorvo_boot_reason reason,
				 void *user_data)
{
	struct cherry_proxy *ctx = (struct cherry_proxy *)user_data;

	switch (reason) {
	case UCI_QORVO_BOOT_REASON_UNKNOWN:
		ctx->boot_reason = CHERRY_CORE_STATE_CHANGE_BOOT_REASON_UNKNOWN;
		QLOGD("%s received unknown UCI Boot reason: UNKNOWN", __func__);
		break;
	case UCI_QORVO_BOOT_REASON_FATAL_ERROR_RESET:
		ctx->boot_reason = CHERRY_CORE_STATE_CHANGE_BOOT_REASON_FATAL;
		QLOGD("%s received unknown UCI Boot reason: FATAL", __func__);
		break;
	default:
		QLOGW("%s received unknown UCI Boot reason: %d", __func__,
		      reason);
		break;
	}

	ctx->reboot = true;

	return;
}

/* All others notifications/responses. */
static void cherry_proxy_data_uci_cb(uint8_t *data_notif,
				     uint16_t data_notif_sz, void *user_data)
{
	struct cherry_proxy *ctx = (struct cherry_proxy *)user_data;
	enum uci_status_code ret;

	if ((ret = cherry_uci_client_proxy_send_data(
		     ctx->vcom.proxy_ctx, data_notif, data_notif_sz)) !=
	    UCI_STATUS_OK)
		QLOGE("%s: error sending data %d.", __func__, ret);
}

static void cherry_proxy_data_vcom_cb(uint8_t *data_notif,
				      uint16_t data_notif_sz, void *user_data)
{
	struct cherry_proxy *ctx = (struct cherry_proxy *)user_data;
	enum uci_status_code ret;

	if ((ret = cherry_uci_client_proxy_send_data(
		     ctx->device.proxy_ctx, data_notif, data_notif_sz)) !=
	    UCI_STATUS_OK)
		QLOGE("%s: error sending data %d.", __func__, ret);
}

struct cherry_proxy *cherry_proxy_create(const char *device, const char *vcom,
					 const speed_t baud_rate)
{
	struct cherry_proxy *ctx;
	enum cherry_err ret;
	enum qerr r;

	if (!device) {
		QLOGE("%s: UCI device is null.", __func__);
		return NULL;
	}

	if (!vcom) {
		QLOGE("%s: VCOM device is null.", __func__);
		return NULL;
	}

	ctx = (struct cherry_proxy *)qcalloc(1, sizeof(struct cherry_proxy));
	if (!ctx) {
		QLOGE("%s: Unable to allocate memory.", __func__);
		return NULL;
	}

	ctx->boot_reason = CHERRY_CORE_STATE_CHANGE_ACTIVITY;
	ctx->calib = NULL;
	ctx->stop = false;
	ctx->reboot = false;
	ctx->aborted = false;

	/* Initialize UCI and VCOM transport layers. */
	ret = cherry_proxy_init_transport(ctx, device, vcom, baud_rate);
	if (ret != CHERRY_ERR_NONE) {
		QLOGE("%s: cherry_proxy_init_transport failed.", __func__);
		goto error_free;
	}

	/* Initialize the UCI proxy client. */
	if ((r = cherry_uci_client_proxy_open(
		     &ctx->device.proxy_ctx, ctx->device.tr, ctx, false,
		     cherry_proxy_boot_cb, cherry_proxy_data_uci_cb))) {
		QLOGE("%s: cherry_uci_client_proxy_open failed with error %d.",
		      __func__, r);
		goto error_dev_proxy;
	}

	/* Initialize the proxy client. */
	if ((r = cherry_uci_client_proxy_open(&ctx->vcom.proxy_ctx,
					      ctx->vcom.tr, ctx, true, NULL,
					      cherry_proxy_data_vcom_cb))) {
		QLOGE("%s: cherry_uci_client_proxy_open failed with error %d.",
		      __func__, r);
		goto error_vcom_proxy;
	}

	return ctx;

error_vcom_proxy:
	cherry_uci_client_proxy_close(ctx->device.proxy_ctx);
error_dev_proxy:
	cherry_proxy_de_init_transport(ctx);
error_free:
	qfree(ctx);
	return NULL;
}

void cherry_proxy_destroy(struct cherry_proxy *ctx)
{
	if (!ctx)
		return;
	cherry_uci_client_proxy_close(ctx->device.proxy_ctx);
	cherry_uci_client_proxy_close(ctx->vcom.proxy_ctx);
	cherry_proxy_de_init_transport(ctx);
	qfree(ctx);
}

enum cherry_err cherry_proxy_start(struct cherry_proxy *ctx,
				   const struct cherry_calib *calib)
{
	ctx->calib = calib;

#ifndef CONFIG_CHERRY_ZEPHYR
	/* Start reading thread. */
	ctx->reading = true;
	if ((ctx->reader_thread = qthread_create(
		     proxy_read_thread_hdl, ctx, "UCI transport thread", NULL,
		     0, QTHREAD_PRIORITY_NORMAL)) == NULL) {
		QLOGE("%s: qthread_create failed.", __func__);
		ctx->reading = false;
		goto error_reader_thread;
	}
#endif
	if ((ctx->proxy_thread = qthread_create(
		     proxy_thread_hdl, ctx, "Proxy thread", cherry_proxy_stack,
		     CONFIG_CHERRY_PROXY_STACK_SIZE,
		     QTHREAD_PRIORITY_NORMAL)) == NULL) {
		QLOGE("%s: qthread_create failed.", __func__);
		goto error_proxy_thread;
	}

	return CHERRY_ERR_NONE;

error_proxy_thread:
#ifndef CONFIG_CHERRY_ZEPHYR
	qthread_join(ctx->reader_thread);
	qthread_delete(ctx->reader_thread);
	ctx->reader_thread = NULL;
error_reader_thread:
#endif
	return CHERRY_ERR_INTERNAL;
}

enum cherry_err cherry_proxy_stop(struct cherry_proxy *ctx)
{
	if (!ctx)
		return CHERRY_ERR_INVALID_PARAMETER;

		/* Stop the threads. */
#ifndef CONFIG_CHERRY_ZEPHYR
	if (ctx->reader_thread) {
		ctx->reading = false;
		qthread_join(ctx->reader_thread);
		qthread_delete(ctx->reader_thread);
		ctx->reader_thread = NULL;
	}
#endif
	if (ctx->proxy_thread) {
		ctx->stop = true;
		qthread_join(ctx->proxy_thread);
		qthread_delete(ctx->proxy_thread);
		ctx->proxy_thread = NULL;
	}
	return ctx->aborted ? CHERRY_ERR_INTERNAL : CHERRY_ERR_NONE;
}

static void proxy_thread_hdl(void *arg)
{
	struct cherry_proxy *ctx = (struct cherry_proxy *)arg;
	enum cherry_err err;

	/* Create local cherry_uci_transport to call cherry_send_calib(). */
	struct cherry_uci_transport cut;
	cut.transport = ctx->device.tr;
	cut.uci = cherry_uci_client_proxy_get(ctx->device.proxy_ctx);

	if (ctx->calib) {
		err = cherry_send_calib(ctx->calib, &cut);
		if (err != CHERRY_ERR_NONE)
			QLOGE("%s: cherry_send_calib fails with error %d.",
			      __func__, err);
	}

	do {
		if (ctx->reboot) {
			if (ctx->calib) {
				err = cherry_send_calib(ctx->calib, &cut);
				if (err != CHERRY_ERR_NONE)
					QLOGE("%s: cherry_send_calib fails  after reboot with error %d.",
					      __func__, err);
			}
			ctx->reboot = false;
		}
		qtime_msleep(100);
	} while (!ctx->stop);
}
