/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */
#include <errno.h>
#include <string.h>

#include "qmchannel_priv.h"
#include "qmutils/qmutils.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

#ifdef USE_MOCK
int qmchannel_write_mock(qmhandle hnd, void *buf, size_t len);
#define qmchannel_write(h, b, l) qmchannel_write_mock(h, b, l)
#endif

enum coredump_cmd_id {
	COREDUMP_HEADER_NTF,
	COREDUMP_BODY_NTF,
	COREDUMP_RCV_STATUS,
	COREDUMP_FORCE_CMD,
};

enum coredump_status { COREDUMP_RCV_NACK, COREDUMP_RCV_ACK };

struct __attribute__((packed)) coredump_hdr {
	uint32_t size;
	uint16_t csum;
};

/**
 * struct coredump_channel - Coredump channel private data.
 * @base: Common channel private data.
 * @end_cb: End callback.
 * @coredump_size: Size of the coredump, sent by the firmware.
 * @coredump_csum: Checksum of the coredump, sent by the firmware.
 * @csum_received: Checksum, calculated from the received data.
 * @coredump_remain: Remaining coredump data to receive.
 * @coredump_buf: Coredump buffer.
 * @coredump_buf_size: Size of the coredump buffer.
 * @coredump_buf_ptr: Pointer to the current position in the coredump buffer.
 * @coredump_buf_remain: Remaining space in the coredump buffer.
 * @set_buf: Buffer set by the application.
 * @set_buf_size: Size of the buffer set by the application.
 */
struct coredump_channel {
	struct common_channel_data base;
	qmu_coredump_callback end_cb;

	uint32_t coredump_size;
	uint16_t coredump_csum;

	uint16_t csum_received;
	size_t coredump_remain;

	char *coredump_buf;
	size_t coredump_buf_size;
	char *coredump_buf_ptr;
	size_t coredump_buf_remain;

	char *set_buf;
	size_t set_buf_size;
};

/**
 * qmu_coredump_prepare_buf() - Prepare the coredump buffer to receive data.
 * @ch: Coredump channel.
 *
 * Return: 0 on success, else a negative error code.
 */
static int qmu_coredump_prepare_buf(struct coredump_channel *ch)
{
	if (ch->set_buf) {
		ch->coredump_buf = ch->set_buf;
		ch->coredump_buf_size = ch->set_buf_size;
	} else {
		ch->coredump_buf_size = ch->set_buf_size ? ch->set_buf_size :
							   ch->coredump_size;
		ch->coredump_buf = qmchannel_allocbuf(ch->coredump_buf_size);
		if (!ch->coredump_buf)
			return -ENOMEM;
	}
	ch->coredump_buf_ptr = ch->coredump_buf;
	ch->coredump_buf_remain = ch->coredump_buf_size;

	return 0;
}

/**
 * qmu_coredump_send_status() - Send status at the end of the reception of a coredump.
 * @ch: Coredump channel.
 * @status: True if the coredump has been received successfully.
 *
 * Return: 0 on success, else a negative error code.
 */
static int qmu_coredump_send_status(struct coredump_channel *ch, bool status)
{
	int ret, ret2;

	/* Send status to the application. */
	ret = ch->end_cb(ch->base.app_cb_data, status);

	/* Send status to the firmware. */
	ret2 = qmchannel_write(
		&ch->base.ll,
		((char[]){ COREDUMP_RCV_STATUS,
			   status ? COREDUMP_RCV_ACK : COREDUMP_RCV_NACK }),
		2);

	if (ret)
		return ret;
	if (ret2 < 0)
		return ret2;
	return 0;
}

/**
 * qmu_coredump_csum() - Calculate checksum of a buffer.
 * @buf: Buffer to use.
 * @len: Length of the buffer.
 *
 * Return: Checksum.
 */
static uint16_t qmu_coredump_csum(char *buf, size_t len)
{
	uint16_t csum = 0;
	char *end = buf + len;

	while (buf < end)
		csum += *buf++;
	return csum;
}

/**
 * qmu_coredump_handle_header() - Process a COREDUMP_HEADER_NTF message.
 * @cb_data: Callback data.
 * @buf: Buffer to use.
 * @len: Length of the buffer.
 *
 * Return: 0 on success, else a negative error code.
 */
static int qmu_coredump_handle_header(void *cb_data, void *buf, size_t len)
{
	struct coredump_channel *ch = cb_data;
	struct coredump_hdr *hdr = buf;
	int ret = 0;

	ch->coredump_size = hdr->size;
	ch->coredump_csum = hdr->csum;
	ch->csum_received = 0;
	ch->coredump_remain = ch->coredump_size;

	ret = qmu_coredump_prepare_buf(ch);
	if (ret)
		qmu_coredump_send_status(ch, false);

	return ret;
}

/**
 * qmu_coredump_handle_body() - Process a COREDUMP_BODY_NTF message.
 * @cb_data: Callback data.
 * @buf: Buffer to use.
 * @len: Length of the buffer.
 *
 * This function MUST free the coredump buffer if it decides not to pass it to
 * the application, except when it has been set using qmu_coredump_setbuf().
 *
 * Return: 0 on success, else a negative error code.
 */
static int qmu_coredump_handle_body(void *cb_data, void *buf, size_t len)
{
	struct coredump_channel *ch = cb_data;
	int ret = 0;

	if (len > ch->coredump_remain) {
		if (!ch->set_buf)
			qmchannel_freebuf(ch->coredump_buf);
		qmu_coredump_send_status(ch, false);
		ret = -EMSGSIZE;
		goto error;
	}

	while (len) {
		size_t copy_len = min(len, ch->coredump_buf_remain);

		memcpy(ch->coredump_buf_ptr, buf, copy_len);
		ch->coredump_buf_ptr += copy_len;
		ch->coredump_buf_remain -= copy_len;
		ch->csum_received += qmu_coredump_csum(buf, copy_len);
		ch->coredump_remain -= copy_len;
		len -= copy_len;
		buf = (char *)buf + copy_len;

		/* When the coredump buffer becomes full, pass it to the
		 * application. */
		if (!ch->coredump_buf_remain) {
			ret = ch->base.app_cb(ch->base.app_cb_data,
					      ch->coredump_buf,
					      ch->coredump_buf_size);
			if (ret)
				goto error;

			/* If coredump data remains, prepare a new buffer, else
			 * reset the buffer pointer to avoid sending the same
			 * data twice. */
			if (ch->coredump_remain) {
				ret = qmu_coredump_prepare_buf(ch);
				if (ret)
					goto error;
			} else {
				ch->coredump_buf_ptr = ch->coredump_buf;
			}
		}
	}

	/* Coredump has been fully received. */
	if (!ch->coredump_remain) {
		int remaining = ch->coredump_buf_ptr - ch->coredump_buf;
		bool status = ch->csum_received == ch->coredump_csum;

		if (remaining) {
			/* If the checksum is correct, send the remaining data
			 * to the application. Otherwise, free the buffer. */
			if (status)
				ret = ch->base.app_cb(ch->base.app_cb_data,
						      ch->coredump_buf,
						      remaining);
			else if (!ch->set_buf)
				qmchannel_freebuf(ch->coredump_buf);
		}

		qmu_coredump_send_status(ch, status);
	}

error:
	return ret;
}

/**
 * qmu_coredump_handle_data() - Callback for the qmchannel_read() function.
 * @cb_data: Callback data.
 * @buf: Buffer to use.
 * @len: Length of the buffer.
 *
 * This function MUST free the received channel buffer, except when it has been
 * set using qmchannel_setbuf().
 *
 * Return: 0 on success, else a negative error code.
 */
static int qmu_coredump_handle_data(void *cb_data, void *buf, size_t len)
{
	struct coredump_channel *ch = cb_data;
	int data_len = len - CD_CMD_LEN;
	char *data = (char *)buf + CD_CMD_LEN;
	int ret;

	switch (*(char *)buf) {
	case COREDUMP_HEADER_NTF:
		ret = qmu_coredump_handle_header(ch, data, data_len);
		break;
	case COREDUMP_BODY_NTF:
		ret = qmu_coredump_handle_body(ch, data, data_len);
		break;
	default:
		ret = -EBADMSG;
	}

	if (!ch->base.ll.buffer)
		qmchannel_freebuf(buf);
	return ret;
}

/**
 * qmu_coredump_init() - Initialize and return a new coredump channel.
 * @read_cb: Read callback.
 * @end_cb: End callback.
 * @cb_data: Callback data.
 *
 * Return: Coredump channel qmhandle on success, else NULL.
 */
qmhandle qmu_coredump_init(qmchannel_callback read_cb,
			   qmu_coredump_callback end_cb, void *cb_data)
{
	qmhandle hnd;

	if (!read_cb || !end_cb)
		return NULL;

	hnd = common_init(QMCHANNEL_TYPE_COREDUMP,
			  sizeof(struct coredump_channel),
			  qmu_coredump_handle_data, read_cb, cb_data);

	if (hnd) {
		struct coredump_channel *ch;

		ch = container_of(hnd, struct coredump_channel, base.ll);
		ch->end_cb = end_cb;
	}

	return hnd;
}

/**
 * qmu_coredump_setbuf() - Set the buffer to be used by the coredump channel.
 * @hnd: qmchannel handle.
 * @buf: Buffer to use.
 * @len: Length of the buffer.
 *
 * When @buf is not NULL, set the app-provided buffer and its size. @len cannot
 * be 0 in this case.
 *
 * When @buf is NULL, @len represents the size of the buffer that will be
 * allocated using qmu_coredump_prepare_buf(), defaulting to the total coredump
 * size.
 *
 * Return: 0 on success, else a negative error code.
 */
int qmu_coredump_setbuf(qmhandle hnd, void *buf, size_t len)
{
	struct coredump_channel *ch;

	if (!hnd || (buf && !len))
		return -EINVAL;
	if (len && len < COREDUMP_HSSPI_BLOCK_LENGTH)
		return -EMSGSIZE;

	ch = container_of(hnd, struct coredump_channel, base.ll);
	ch->set_buf = buf;
	ch->set_buf_size = len;

	return 0;
}

/**
 * qmu_coredump_start() - Start the coredump channel read thread.
 * @hnd: qmchannel handle.
 *
 * Return: 0 on success, else a negative error code.
 */
int qmu_coredump_start(qmhandle hnd)
{
	return common_start(hnd, common_thread);
}

/**
 * qmu_coredump_destroy() - Stop the coredump channel read thread and destroy the instance.
 * @hnd: qmchannel handle.
 *
 * Return: 0 on success, else a negative error code.
 */
int qmu_coredump_destroy(qmhandle hnd)
{
	return common_destroy(hnd);
}

/**
 * qmu_coredump_force() - Force the generation of a coredump by the firmware.
 * @hnd: qmchannel handle.
 *
 * Return: 0 on success, else a negative error code.
 */
int qmu_coredump_force(qmhandle hnd)
{
	int ret = qmchannel_write(hnd, (char[]){ COREDUMP_FORCE_CMD }, 1);

	return ret >= 0 ? 0 : ret;
}
