/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <stddef.h>

/* Values coming from uci_transport library of the firmware. */
#define UCI_PACKET_HEADER_SIZE 4
#define UCI_MAX_CONTROL_PAYLOAD_SIZE 255
#define UCI_MAX_PAYLOAD_SIZE (UCI_MAX_CONTROL_PAYLOAD_SIZE)
#define UCI_MAX_PACKET_SIZE (UCI_PACKET_HEADER_SIZE + UCI_MAX_PAYLOAD_SIZE)

#define CD_CMD_LEN 1
#define COREDUMP_HSSPI_BLOCK_LENGTH 128
#define COREDUMP_MAX_PACKET_SIZE (CD_CMD_LEN + COREDUMP_HSSPI_BLOCK_LENGTH)

#define LOG_BACKEND_HSSPI_BUF_SIZE 256
#define LOG_MAX_PACKET_SIZE LOG_BACKEND_HSSPI_BUF_SIZE

#define QTRACE_ALLOC_SIZE 256
#define QTRACE_MAX_PACKET_SIZE QTRACE_ALLOC_SIZE

enum qmchannel_type {
	QMCHANNEL_TYPE_RESERVED_HSSPI,
	QMCHANNEL_TYPE_BOOTLOADER,
	QMCHANNEL_TYPE_UCI,
	QMCHANNEL_TYPE_COREDUMP,
	QMCHANNEL_TYPE_LOG,
	QMCHANNEL_TYPE_QTRACE,
	QMCHANNEL_TYPE_MAX
};

struct qmchannel;
typedef struct qmchannel *qmhandle;

typedef int (*qmchannel_callback)(void *cb_data, void *buf, size_t len);

#ifdef __cplusplus
extern "C" {
#endif

int qmchannel_init(qmhandle hnd, enum qmchannel_type type,
		   qmchannel_callback cb, void *cb_data);
int qmchannel_close(qmhandle hnd);
int qmchannel_getfd(qmhandle hnd);
int qmchannel_getpollevent(qmhandle hnd);
int qmchannel_setbuf(qmhandle hnd, void *buf, size_t len);
int qmchannel_read(qmhandle hnd);
int qmchannel_write(qmhandle hnd, void *buf, size_t len);
int qmchannel_ioctl(qmhandle hnd, unsigned long request, char *argp);

void *qmchannel_allocbuf(size_t size);
int qmchannel_freebuf(void *buf);

#ifdef QMCHANNEL_USE_BYPASS
/* Embedded channel require this function called before any other. */
struct qm3x;
void qmchannel_setup(struct qm3x *qm);
#endif

#ifdef __cplusplus
}
#endif
