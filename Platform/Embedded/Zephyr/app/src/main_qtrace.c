/**
 * @file      main_qtrace.c
 *
 * @brief     Example for mcu host to read FW qtrace
 *
 * @author    Qorvo Paris Applications
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include "qmutils/qmutils.h"
#include <qmalloc.h>

#include "SEGGER_RTT.h" /* To output qtrace on JLink RTT. */

/* RTT Channel used for sending QTRACE to Host. */
#define QTRACE_RTT_CHANNEL 1

LOG_MODULE_REGISTER(main_qtrace, CONFIG_APP_LOG_LEVEL);

/* qtrace command options. */
#define OPTSTR "se"

/* qtrace context definition. */
struct qtrace_context {
	/* Context handle. */
	qmhandle qtrace_hnd;

	/* SEGGER RTT params. */
	uint8_t *qtrace_buffer_up;
	uint8_t *qtrace_buffer_down;
};

/* static variables. */
static struct qtrace_context *qtrace_ctx = NULL;

/* static functions prototypes. */
static int qtrace_read_callback(void *cb_data, void *buf, size_t len);
static int rtt_channel_init(void);
static void free_context_memory(void);
static int qtrace_start(void);

static int qtrace_read_callback(void *cb_data, void *buf, size_t len)
{
	int ret = 0;

	/* Send qtrace on JLink RTT. */
	ret = SEGGER_RTT_Write(QTRACE_RTT_CHANNEL, buf, len);
	if (ret != len) {
		/* Buffer is full. */
		ret = -EIO;
	}

	return ret;
}

static int rtt_channel_init(void)
{
	int ret = 0;

	/* Context exists ? */
	if (!qtrace_ctx) {
		ret = -EINVAL;
		LOG_ERR("Missing context: %s", strerror(-ret));
		return ret;
	}
	/* Allocate RTT upload and download buffers. */
	qtrace_ctx->qtrace_buffer_up = qcalloc(BUFFER_SIZE_UP, sizeof(uint8_t));
	if (!qtrace_ctx->qtrace_buffer_up) {
		return -ENOMEM;
	}
	qtrace_ctx->qtrace_buffer_down =
	    qcalloc(BUFFER_SIZE_DOWN, sizeof(uint8_t));
	if (!qtrace_ctx->qtrace_buffer_down) {
		return -ENOMEM;
	}
	/* Initialize JLink RTT link. */
	SEGGER_RTT_Init();
	ret = SEGGER_RTT_ConfigUpBuffer(QTRACE_RTT_CHANNEL, "QTRACEOUT",
					qtrace_ctx->qtrace_buffer_up,
					BUFFER_SIZE_UP,
					SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL);
	if (ret < 0) {
		ret = -EIO;
		LOG_ERR("Failed to config RTT upload buffer: %s.",
			strerror(-ret));
		return ret;
	}
	ret = SEGGER_RTT_ConfigDownBuffer(QTRACE_RTT_CHANNEL, "QTRACEOUT",
					  qtrace_ctx->qtrace_buffer_down,
					  BUFFER_SIZE_DOWN,
					  SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL);
	if (ret < 0) {
		ret = -EIO;
		LOG_ERR("Failed to config RTT download buffer: %s.",
			strerror(-ret));
		return ret;
	}
	return 0;
}

static void free_context_memory(void)
{
	if (qtrace_ctx) {
		if (qtrace_ctx->qtrace_buffer_up) {
			qfree(qtrace_ctx->qtrace_buffer_up);
		}
		if (qtrace_ctx->qtrace_buffer_down) {
			qfree(qtrace_ctx->qtrace_buffer_down);
		}
		qfree(qtrace_ctx);
		qtrace_ctx = NULL;
	}
}

static int qtrace_start(void)
{
	int ret;

	/* Context already allocated ? */
	if (qtrace_ctx) {
		LOG_INF("QTRACE already started.");
		return -ENXIO;
	}
	/* Allocate new context. */
	qtrace_ctx = qcalloc(1, sizeof(struct qtrace_context));
	if (!qtrace_ctx) {
		return -ENOMEM;
	}
	/* Initialize qtrace channel. */
	qtrace_ctx->qtrace_hnd =
	    qmu_qtrace_init(&qtrace_read_callback, "qtrace");
	if (!qtrace_ctx->qtrace_hnd) {
		ret = -EIO;
		LOG_ERR("qmu_qtrace_init: %s.", strerror(-ret));
		goto err;
	}
	/* Init RTT channel connection with host. */
	ret = rtt_channel_init();
	if (ret < 0) {
		/* Close already opened qtrace channel. */
		if (qmu_qtrace_destroy(qtrace_ctx->qtrace_hnd) < 0) {
			LOG_ERR("qmu_qtrace_destroy error.");
		}
		LOG_ERR("Failed to initialize RTT channel: %s.",
			strerror(-ret));
		goto err;
	}
	return 0;
err:
	free_context_memory();
	return ret;
}

static int qtrace_stop(void)
{
	int ret = 0;

	/* Stop qtrace channel. */
	if (!qtrace_ctx) {
		LOG_INF("qtrace channel not initialized.");
		return -ENXIO;
	}
	if (qtrace_ctx->qtrace_hnd) {
		/* Close qtrace channel. */
		ret = qmu_qtrace_destroy(qtrace_ctx->qtrace_hnd);
		if (ret < 0) {
			LOG_ERR("qmu_qtrace_destroy: %s.", strerror(-ret));
			ret = -ENXIO;
		}
	}
	/* Free context in all cases */
	free_context_memory();

	return ret;
}

int main_qtrace(int argc, char *argv[])
{
	int ret = 0;
	int opt = 0;
	bool start_req = false;
	bool end_req = false;

	/* Needed for Zephyr. */
	getopt_init();

	/* Manage options. */
	while ((opt = getopt(argc, argv, OPTSTR)) != -1) {
		switch (opt) {
		case 's': /* Start qtrace. */
			start_req = true;
			break;
		case 'e': /* End qtrace. */
			end_req = true;
			break;
		default: {
			LOG_ERR("Invalid argument '-%c'\n", opt);
			ret = -EINVAL;
			goto exit;
		}
		}
	}
	/* Both options cannot be present at same time. */
	if (start_req && end_req) {
		/* Invalid call. */
		LOG_ERR(
		    "Both options '-s' and '-e' cannot be used at the same time.");
		ret = -EINVAL;
	} else {
		if (start_req)
			ret = qtrace_start();
		else if (end_req)
			ret = qtrace_stop();
	}

exit:
	return ret;
}
