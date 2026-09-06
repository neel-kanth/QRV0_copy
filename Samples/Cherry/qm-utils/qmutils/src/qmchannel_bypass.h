/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <stddef.h>

#include "qmutils/qmchannel.h"

struct qm3x;

/* Copied from qm3x_transport.h */
enum qm3x_transport_msg_type {
	QM3X_TRANSPORT_MSG_RESERVED_HSSPI,
	QM3X_TRANSPORT_MSG_BOOTLOADER,
	QM3X_TRANSPORT_MSG_UCI,
	QM3X_TRANSPORT_MSG_COREDUMP,
	QM3X_TRANSPORT_MSG_LOG,
	QM3X_TRANSPORT_MSG_QTRACE,
	QM3X_TRANSPORT_MSG_AWAKE,
	QM3X_TRANSPORT_MSG_MAX
};

/* Copied from qm3x_bypass.h */
enum qm3x_bypass_events {
	QM3X_BYPASS_IRQ,
	QM3X_BYPASS_NOTIFICATION,
	QM3X_BYPASS_RESPONSE,
	QM3X_BYPASS_MAX
};
enum qm3x_bypass_actions {
	QM3X_BYPASS_ACTION_RESET,
	QM3X_BYPASS_ACTION_FWUPD,
	QM3X_BYPASS_ACTION_POWER,
	QM3X_BYPASS_ACTION_MSG_TYPE,
	QM3X_BYPASS_ACTION_MAX
};
typedef int (*qm3x_bypass_listener_cb)(void *data,
				       enum qm3x_bypass_events event);
struct qm3x_bypass_channel;
typedef struct qm3x_bypass_channel *qm3x_bypass_handle;
int qm3x_bypass_bound(qm3x_bypass_handle hnd,
		      enum qm3x_transport_msg_type *out);
qm3x_bypass_handle qm3x_bypass_open(struct qm3x *qm35,
				    qm3x_bypass_listener_cb cb,
				    void *priv_data);
int qm3x_bypass_queue_check(qm3x_bypass_handle hnd);
int qm3x_bypass_close(qm3x_bypass_handle hnd);
int qm3x_bypass_send(qm3x_bypass_handle hnd, void *buffer, size_t len);
int qm3x_bypass_recv(qm3x_bypass_handle hnd, void *buffer, size_t len,
		     enum qm3x_transport_msg_type *type, int *flags);
int qm3x_bypass_control(qm3x_bypass_handle hnd, enum qm3x_bypass_actions action,
			long *param);

struct qmchannel {
	enum qmchannel_type type;
	qmchannel_callback cb;
	void *cb_data;
	char *buffer;
	size_t buffer_size;
	qm3x_bypass_handle handle;
};
