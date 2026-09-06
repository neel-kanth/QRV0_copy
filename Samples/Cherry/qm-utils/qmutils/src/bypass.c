/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */
#include <errno.h>
#include <qmalloc.h>
#include <stdlib.h>
#include <string.h>

#include "qm35_uci_dev_ioctl.h"
#include "qmchannel_bypass.h"
#include "qmutils/qmchannel.h"

#define DEFAULT_BUF_SIZE \
	1024 /* Should be large enough to hold any UCI message. */

static struct qm3x *device_instance;

static int qmchannel_bypass_listener(void *data, enum qm3x_bypass_events event)
{
	qmhandle hnd = (qmhandle)data;

	while (qmchannel_read(hnd) >= 0)
		;
	return 0;
}

/**
 * qmchannel_setup() - Initialise library to use the given device instance.
 * @device: Device instance to use.
 *
 * Store specified device instance in library private device_instance variable
 * to use it when channel are initialised.
 */
void qmchannel_setup(struct qm3x *device)
{
	device_instance = device;
}

/**
 * qmchannel_init() - Initialize the given qmhandle for the specified channel type.
 * @hnd: qmchannel handle.
 * @type: Channel type.
 * @cb: Callback.
 * @cb_data: Callback data.
 *
 * Return: 0 on success, else a negative error code.
 */
int qmchannel_init(qmhandle hnd, enum qmchannel_type type,
		   qmchannel_callback cb, void *cb_data)
{
	long param = (long)type;
	int ret;

	if (!hnd || !cb || !device_instance)
		return -EINVAL;

	/* Create a new bypass channel on selected device. */
	hnd->handle = qm3x_bypass_open(device_instance,
				       qmchannel_bypass_listener, hnd);
	if (!hnd->handle)
		return -ENOMEM;

	/* Set the channel type. */
	ret = qm3x_bypass_control(hnd->handle, QM3X_BYPASS_ACTION_MSG_TYPE,
				  &param);
	if (ret < 0)
		goto type_error;

	/* Save data */
	hnd->type = type;
	hnd->cb = cb;
	hnd->cb_data = cb_data;
	return 0;

type_error:
	qmchannel_close(hnd);
	return ret;
}

/**
 * qmchannel_close() - Close the given qmhandle.
 * @hnd: qmchannel handle.
 *
 * Return: 0 on success, else a negative error code.
 */
int qmchannel_close(qmhandle hnd)
{
	qm3x_bypass_handle bph;

	if (!hnd)
		return -EINVAL;

	bph = hnd->handle;
	hnd->handle = NULL;

	return qm3x_bypass_close(bph);
}

/**
 * qmchannel_getfd() - Get the file descriptor associated with the given qmhandle.
 * @hnd: qmchannel handle.
 *
 * Not supported for embedded implementation.
 *
 * Return: -ENOTSUP error.
 */
int qmchannel_getfd(qmhandle hnd)
{
	return -ENOTSUP;
}

/**
 * qmchannel_getpollevent() - Get the poll event for the given qmhandle file descriptor.
 * @hnd: qmchannel handle.
 *
 * Not supported for embedded implementation.
 *
 * Return: -ENOTSUP error.
 */
int qmchannel_getpollevent(qmhandle hnd)
{
	return -ENOTSUP;
}

/**
 * qmchannel_setbuf() - Set the buffer to be used by qmchannel_read().
 * @hnd: qmchannel handle.
 * @buf: Buffer to use.
 * @len: Length of the buffer.
 *
 * When @buf is not NULL, set the app-provided buffer and its size. @len cannot
 * be 0 in this case.
 *
 * When @buf is NULL, @len represents the size of the buffer that will be
 * allocated using qmchannel_allocbuf(), defaulting to %DEFAULT_BUF_SIZE.
 *
 * Return: 0 on success, else a negative error code.
 */
int qmchannel_setbuf(qmhandle hnd, void *buf, size_t len)
{
	if (!hnd || (buf && !len))
		return -EINVAL;

	switch (hnd->type) {
	case QMCHANNEL_TYPE_RESERVED_HSSPI:
		break;
	case QMCHANNEL_TYPE_COREDUMP:
		if (len && len < COREDUMP_MAX_PACKET_SIZE)
			return -EMSGSIZE;
		break;
	case QMCHANNEL_TYPE_LOG:
		if (len && len < LOG_MAX_PACKET_SIZE)
			return -EMSGSIZE;
		break;
	case QMCHANNEL_TYPE_QTRACE:
		if (len && len < QTRACE_MAX_PACKET_SIZE)
			return -EMSGSIZE;
		break;
	default:
		if (len && len < UCI_MAX_PACKET_SIZE)
			return -EMSGSIZE;
	}

	hnd->buffer = buf;
	hnd->buffer_size = len;
	return 0;
}

/**
 * qmchannel_read() - Read from the given qmhandle file descriptor.
 * @hnd: qmchannel handle.
 *
 * There is no need for the application to call this function because it is
 * already called by the installed qmchannel_bypass_listener() event callback.
 *
 * Return: callback return value on success, else a negative error code.
 */
int qmchannel_read(qmhandle hnd)
{
	int buf_len, read_len;
	void *buf;
	enum qm3x_transport_msg_type type;
	int flags;

	if (!hnd)
		return -EINVAL;

	if (hnd->buffer) {
		buf = hnd->buffer;
		buf_len = hnd->buffer_size;
	} else {
		buf_len = hnd->buffer_size ? hnd->buffer_size :
					     DEFAULT_BUF_SIZE;
		buf = qmchannel_allocbuf(buf_len);
		if (!buf)
			return -ENOMEM;
	}

	read_len = qm3x_bypass_recv(hnd->handle, buf, buf_len, &type, &flags);
	if (read_len < 0) {
		if (!hnd->buffer)
			qmchannel_freebuf(buf);
		return read_len;
	}

	return hnd->cb(hnd->cb_data, buf, read_len);
}

/**
 * qmchannel_write() - Write to the given qmhandle file descriptor.
 * @hnd: qmchannel handle.
 * @buf: Buffer to use.
 * @len: Length of the buffer.
 *
 * Return: number of bytes written on success, else a negative error code.
 */
int qmchannel_write(qmhandle hnd, void *buf, size_t len)
{
	if (!hnd)
		return -EINVAL;

	return qm3x_bypass_send(hnd->handle, buf, len);
}

/**
 * qmchannel_ioctl() - Send ioctl request to the given qmhandle file descriptor.
 * @hnd: qmchannel handle.
 * @request: Request code.
 * @argp: Pointer argument.
 *
 * Return: ioctl return value on success, else a negative error code.
 */
int qmchannel_ioctl(qmhandle hnd, unsigned long request, char *argp)
{
	struct qm35_fwupload_params ext_params;
	long param = 0;
	int ret;

	if (!hnd)
		return -EINVAL;

	switch (request) {
	case QM35_CTRL_RESET_EXT:
		if (!argp)
			return -EINVAL;
		param = *(unsigned int *)argp;
		/* fall-through */
	case QM35_CTRL_RESET:
		ret = qm3x_bypass_control(hnd->handle, QM3X_BYPASS_ACTION_RESET,
					  &param);
		break;
	case QM35_CTRL_FW_UPLOAD:
		ret = qm3x_bypass_control(hnd->handle, QM3X_BYPASS_ACTION_FWUPD,
					  NULL);
		break;
	case QM35_CTRL_FW_UPLOAD_EXT:
		if (!argp)
			return -EINVAL;
		memcpy(&ext_params, argp, sizeof(ext_params));
		ext_params.fw_name[QM35_FIRMWARE_FILENAME_SIZE - 1] = '\0';
		ret = qm3x_bypass_control(hnd->handle, QM3X_BYPASS_ACTION_FWUPD,
					  (long *)&ext_params);
		break;
	case QM35_CTRL_POWER:
		if (!argp)
			return -EINVAL;
		param = *(unsigned int *)argp;
		ret = qm3x_bypass_control(hnd->handle, QM3X_BYPASS_ACTION_POWER,
					  &param);
		break;
	default:
		return -ENOTSUP;
	}
	return ret;
}

/**
 * qmchannel_allocbuf() - Allocate a buffer.
 * @size: Size to allocate.
 *
 * Declared as weak so the library client can redefine it.
 *
 * Return: buffer address on success, else 0.
 */
void *__attribute__((weak)) qmchannel_allocbuf(size_t size)
{
	return qmalloc(size);
}

/**
 * qmchannel_freebuf() - Free given allocated buffer.
 * @buf: Buffer to free.
 *
 * Declared as weak so the library client can redefine it.
 *
 * Return: 0 on success, else a negative error code.
 */
int __attribute__((weak)) qmchannel_freebuf(void *buf)
{
	qfree(buf);
	return 0;
}
