/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */
#include <errno.h>

#include "qmchannel_priv.h"
#include "qmutils/qmutils.h"

/* LOG definitions */

enum log_commands_id {
	LOG_TRACE_NTF = 0,
	SET_LOG_LEVEL = 1,
	GET_LOG_LEVEL = 2,
	GET_LOG_SOURCES = 3,
};

/**
 * qmu_log_init() - Initialize and return a new log channel.
 * @cb: Callback.
 * @cb_data: Callback data.
 *
 * Return: Log channel qmhandle on success, else NULL.
 */
qmhandle qmu_log_init(qmchannel_callback cb, void *cb_data)
{
	return common_init(QMCHANNEL_TYPE_LOG,
			   sizeof(struct common_channel_data),
			   common_handle_data, cb, cb_data);
}

/**
 * qmu_log_start() - Start the log channel read thread.
 * @hnd: qmchannel handle.
 *
 * Return: 0 on success, else a negative error code.
 */
int qmu_log_start(qmhandle hnd)
{
	/* Check that handle is LOG type */
	if (hnd->type == QMCHANNEL_TYPE_LOG) {
		return common_start(hnd, common_thread);
	} else {
		/* Invalid channel type */
		return -EINVAL;
	}
}

/**
 * qmu_log_destroy() - Stop the log channel read thread and destroy the instance.
 * @hnd: qmchannel handle.
 *
 * Return: 0 on success, else a negative error code.
 */
int qmu_log_destroy(qmhandle hnd)
{
	/* Check that handle is LOG type */
	if (hnd->type == QMCHANNEL_TYPE_LOG) {
		return common_destroy(hnd);
	} else {
		/* Invalid channel type */
		return -EINVAL;
	}
}

/**
 * qmu_log_get_sources() - Get all log module ids and names.
 * @hnd: qmchannel handle.
 *
 * Return: 0 on success, else a negative error code.
 */
int qmu_log_get_sources(qmhandle hnd)
{
	int ret;

	if (hnd->type == QMCHANNEL_TYPE_LOG) {
		/* Get log module ids */
		ret = qmchannel_write(hnd, (char[]){ GET_LOG_SOURCES }, 1);

	} else {
		/* Invalid channel type */
		ret = -EINVAL;
	}

	return ret >= 0 ? 0 : ret;
}

/**
 * qmu_log_get_level() - Get the current module log level.
 * @hnd: qmchannel handle.
 * @module_id: Module for which to get log level.
 *
 * Return: 0 on success, else a negative error code.
 */
int qmu_log_get_level(qmhandle hnd, int module_id)
{
	int ret;

	if (hnd->type == QMCHANNEL_TYPE_LOG) {
		/* Get log level for given module id */
		ret = qmchannel_write(hnd,
				      (char[]){ GET_LOG_LEVEL, 0x00, 0x01, 0x00,
						(uint8_t)module_id },
				      5);

	} else {
		/* Invalid channel type */
		ret = -EINVAL;
	}

	return ret >= 0 ? 0 : ret;
}

/**
 * qmu_log_set_level() - Set module log level.
 * @hnd: qmchannel handle.
 * @module_id: Module for which to set log level.
 * @level: Log level to set for given module id.
 *
 * Return: 0 on success, else a negative error code.
 */
int qmu_log_set_level(qmhandle hnd, int module_id, int level)
{
	int ret;

	/* Check that handle is LOG type */
	if (hnd->type == QMCHANNEL_TYPE_LOG) {
		/* Set log level for given module id */
		ret = qmchannel_write(hnd,
				      (char[]){ SET_LOG_LEVEL, 0x00, 0x02, 0x00,
						(uint8_t)module_id,
						(uint8_t)level },
				      6);
	} else {
		/* Invalid channel type */
		ret = -EINVAL;
	}

	return ret >= 0 ? 0 : ret;
}
