/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "drivers/uwb/bypass.h"

#include <errno.h>
#include <stdlib.h>
#include <zephyr/device.h>

#include "drivers/uwb/hsspi.h"
#include "drivers/uwb/hsspi_helpers.h"
#include "qassert.h"
#include "qmalloc.h"
#include "qmutex.h"
#include "qmutils/qmchannel.h"
#include "qlog.h"
#include "qtime.h"

struct qm3x_bypass_channel {
	struct qm3x *device;
	struct qmutex *lock; /* protect *buffer* fields */
	enum qm3x_transport_msg_type type;
	qm3x_bypass_listener_cb cb;
	void *cb_data;
	char *read_buffer;
	size_t buffer_len;
	size_t buffer_offset;
};

int qm3x_bypass_bound(qm3x_bypass_handle hnd, enum qm3x_transport_msg_type *out)
{
	if (hnd && hnd->type != QM3X_TRANSPORT_MSG_MAX) {
		if (out)
			*out = hnd->type;
		return true;
	}
	return false;
}

qm3x_bypass_handle qm3x_bypass_open(struct qm3x *qm35,
				    qm3x_bypass_listener_cb cb, void *priv_data)
{
	struct qm3x_bypass_channel *new;
	new = qmalloc(sizeof(*new));
	if (!new)
		return NULL;
	new->lock = qmutex_init();
	new->device = qm35;
	new->type = QM3X_TRANSPORT_MSG_MAX; /* unbound channel */
	new->cb = cb;
	new->cb_data = priv_data;
	new->read_buffer = NULL;
	return new;
}

static int qm3x_bypass_set_type(qm3x_bypass_handle hnd,
				enum qm3x_transport_msg_type type)
{
	struct qm3x *qm3x = hnd->device;
	enum qm3x_transport_msg_type cur;
	int rc = 0;

	if (!hnd || type > QM3X_TRANSPORT_MSG_MAX)
		return -EINVAL;
	if (type == hnd->type)
		return 0;
	qmutex_lock(qm3x->lock, QOSAL_WAIT_FOREVER);
	if (qm3x_bypass_bound(hnd, &cur)) {
		/* Self remove from device's handles table. */
		QASSERT(qm3x->handles[cur] == hnd);
		qm3x->handles[cur] = NULL;
		hnd->type = QM3X_TRANSPORT_MSG_MAX;
	}
	if (type == QM3X_TRANSPORT_MSG_MAX)
		goto unlock;
	if (qm3x->handles[type]) {
		rc = -EEXIST;
	} else {
		qm3x->handles[type] = hnd;
	}
unlock:
	if (!rc)
		/* Save new expected type. */
		hnd->type = type;
	qmutex_unlock(qm3x->lock);
	return rc;
}

int qm3x_bypass_queue_check(qm3x_bypass_handle hnd)
{
	int ret;
	if (!hnd)
		return -EINVAL;
	qmutex_lock(hnd->lock, QOSAL_WAIT_FOREVER);
	ret = hnd->read_buffer != NULL;
	qmutex_unlock(hnd->lock);
	return ret;
}

int qm3x_bypass_close(qm3x_bypass_handle hnd)
{
	if (!hnd)
		return -EINVAL;
	/* Ensure this bypass handle callback isn't called anymore. */
	qm3x_bypass_set_type(hnd, QM3X_TRANSPORT_MSG_MAX);
	/* Cleanup bypass handle and ensure no one reading the buffer. */
	qmutex_lock(hnd->lock, QOSAL_WAIT_FOREVER);
	if (hnd->read_buffer) {
		unsigned unread = hnd->buffer_len - hnd->buffer_offset;
		QLOGW("Channel closed with %u bytes unread!", unread);
		/* Received buffer was allocated by malloc() in hsspi_helpers.c. */
		free(hnd->read_buffer);
		hnd->read_buffer = NULL;
	}
	qmutex_unlock(hnd->lock);
	qmutex_deinit(hnd->lock);
	qfree(hnd);
	return 0;
}

int qm3x_bypass_send(qm3x_bypass_handle hnd, void *buffer, size_t len)
{
	int ret;

	if (!hnd)
		return -EINVAL;
	/* Use synchronous write as we assume this function is only called from
	 * cherry thread when a command is sent using qmutils uci transport. */
	ret = hsspi_sync_write(hnd->type, (uint8_t *)buffer, len);
	return ret;
}

int qm3x_bypass_recv(qm3x_bypass_handle hnd, void *buffer, size_t len,
		     enum qm3x_transport_msg_type *type, int *flags)
{
	int ret = -EAGAIN;
	size_t remain;

	qmutex_lock(hnd->lock, QOSAL_WAIT_FOREVER);
	if (!hnd->read_buffer)
		goto unlock;

	remain = hnd->buffer_len - hnd->buffer_offset;
	if (len > remain)
		len = remain;

	memcpy(buffer, hnd->read_buffer + hnd->buffer_offset, len);
	*type = hnd->type;
	if (flags)
		*flags = 0;

	hnd->buffer_offset += len;
	ret = len;

	if (hnd->buffer_offset == hnd->buffer_len) {
		free(hnd->read_buffer);
		hnd->read_buffer = NULL;
	}

unlock:
	qmutex_unlock(hnd->lock);
	return ret;
}

int qm3x_bypass_control(qm3x_bypass_handle hnd, enum qm3x_bypass_actions action,
			long *param)
{
	struct qm3x *qm3x;
	const struct device *dev;
	enum qm3x_transport_msg_type cur, new;
	int rc = -EINVAL;

	if (!hnd)
		goto error;
	qm3x = hnd->device;

	/* Now perform the requested action (outside critical section) */
	switch (action) {
	case QM3X_BYPASS_ACTION_RESET:
		if (!param)
			break;
		dev = CONTAINER_OF(qm3x, struct hsspi_driver_data, qminstance)
			  ->dev;
		hsspi_reset(dev, *param);
		rc = 0;
		break;
	case QM3X_BYPASS_ACTION_MSG_TYPE:
		if (!qm3x_bypass_bound(hnd, &cur))
			cur = QM3X_TRANSPORT_MSG_MAX;
		if (param) {
			new = *param;
			rc = qm3x_bypass_set_type(hnd, new);
			if (!rc)
				rc = cur;
		} else {
			rc = cur;
		}
		break;
	case QM3X_BYPASS_ACTION_FWUPD:
		/* FW flashing is already handled by the application. No need
		 * for this action until FW flashing use the qm-utils library. */
		rc = -ENOTSUP;
		break;
	case QM3X_BYPASS_ACTION_POWER:
		/* Manual power management not supported by the application. */
		rc = -ENOTSUP;
		break;
	default:
		QLOGW("Unsupported action %d\n", action);
		rc = -EINVAL;
	}
error:
	return rc;
}

int qm3x_bypass_event(struct qm3x *qm, enum qm3x_transport_msg_type type,
		      void *buffer, size_t len)
{
	qm3x_bypass_handle hnd;

	if (type >= QM3X_TRANSPORT_MSG_MAX)
		goto nochan;

	qmutex_lock(qm->lock, QOSAL_WAIT_FOREVER);
	hnd = qm->handles[(int)type];
	qmutex_unlock(qm->lock);
	if (!hnd)
		goto nochan;

	qmutex_lock(hnd->lock, QOSAL_WAIT_FOREVER);
	if (hnd->read_buffer) {
		QLOGW("Previous buffer not read! Remain %d bytes!",
		      hnd->buffer_len - hnd->buffer_offset);
		free(hnd->read_buffer);
	}
	hnd->read_buffer = buffer;
	hnd->buffer_len = len;
	hnd->buffer_offset = 0;
	qmutex_unlock(hnd->lock);

	return hnd->cb(hnd->cb_data, QM3X_BYPASS_IRQ);

nochan:
	free(buffer);
	return -ENOENT;
}

int qm3x_bypass_init(struct qm3x *qminstance)
{
	memset(qminstance, 0, sizeof(*qminstance));
	qminstance->lock = qmutex_init();
	if (!qminstance->lock)
		return -ENOMEM;
	qmchannel_setup(qminstance);
	return 0;
}
