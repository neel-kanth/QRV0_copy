/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <qmalloc.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "qm35_uci_dev_ioctl.h"
#include "qmchannel_multichan.h"
#include "qmutils/qmchannel.h"

#ifndef UCI_DEV_PATH
#define UCI_DEV_PATH "/dev/uci0"
#endif
#define DEFAULT_BUF_SIZE \
	4096 /* Should be large enough to hold any UCI message. */

#ifdef USE_MOCK
int open_mock(const char *pathname, int flags);
#define open(p, f) open_mock(p, f)
int close_mock(int fd);
#define close(f) close_mock(f)
ssize_t read_mock(int fd, void *buf, size_t count);
#define read(f, b, c) read_mock(f, b, c)
ssize_t write_mock(int fd, const void *buf, size_t count);
#define write(f, b, c) write_mock(f, b, c)
int ioctl_mock(int fd, unsigned long request, void *arg);
#define ioctl(f, r, a) ioctl_mock(f, r, a)
#endif

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
	unsigned int param;
	int ret;

	if (!hnd || !cb)
		return -EINVAL;

	/* Open the device. */
	ret = open(UCI_DEV_PATH, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (ret < 0) {
		ret = -errno;
		goto open_error;
	}
	hnd->fd = ret;

	/* Set the channel type. */
	ret = qmchannel_ioctl(hnd, QM35_CTRL_GET_TYPE, (char *)&param);
	if (ret < 0)
		goto comm_error;
	if (param != type) {
		param = type;
		ret = qmchannel_ioctl(hnd, QM35_CTRL_SET_TYPE, (char *)&param);
		if (ret < 0)
			goto comm_error;
	}

	hnd->type = type;
	hnd->cb = cb;
	hnd->cb_data = cb_data;
	return 0;

comm_error:
	qmchannel_close(hnd);
open_error:
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
	int ret;

	if (!hnd)
		return -EINVAL;

	ret = close(hnd->fd);
	return !ret ? ret : -errno;
}

/**
 * qmchannel_getfd() - Get the file descriptor associated with the given qmhandle.
 * @hnd: qmchannel handle.
 *
 * Return: File descriptor.
 */
int qmchannel_getfd(qmhandle hnd)
{
	if (!hnd)
		return -EINVAL;

	return hnd->fd;
}

/**
 * qmchannel_getpollevent() - Get the poll event for the given qmhandle file descriptor.
 * @hnd: qmchannel handle.
 *
 * Return: Poll event.
 */
int qmchannel_getpollevent(qmhandle hnd)
{
	return POLLIN;
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
 * Return: callback return value on success, else a negative error code.
 */
int qmchannel_read(qmhandle hnd)
{
	int buf_len, read_len;
	void *buf;

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

	read_len = read(hnd->fd, buf, buf_len);
	if (read_len < 0) {
		if (!hnd->buffer)
			qmchannel_freebuf(buf);
		return -errno;
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
	int ret;

	if (!hnd)
		return -EINVAL;

	ret = write(hnd->fd, buf, len);
	return ret >= 0 ? ret : -errno;
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
	int ret;

	if (!hnd)
		return -EINVAL;

	ret = ioctl(hnd->fd, request, argp);
	return ret >= 0 ? ret : -errno;
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
