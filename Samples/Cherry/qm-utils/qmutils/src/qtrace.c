/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */
#include <errno.h>

#include "qmchannel_priv.h"
#include "qmutils/qmutils.h"

enum qmu_qtrace_commands_id {
	QTRACE_SET_LOG_LEVEL = 1,
};

/**
 * qmu_qtrace_set_level() - Set qtrace module dump level.
 * @hnd: The qmchannel handle.
 * @module_id: Module for which to set dump level.
 * @level: Dump level to set for given module id.
 *
 * Return: Zero on success else a negative error.
 */
int qmu_qtrace_set_level(qmhandle hnd, int module_id, int level)
{
	int ret;

	/* Check that handle is QTRACE type */
	if (hnd->type == QMCHANNEL_TYPE_QTRACE) {
		/* Set qtrace level for given module id */
		ret = qmchannel_write(hnd,
				      (char[]){ QTRACE_SET_LOG_LEVEL,
						(uint8_t)module_id,
						(uint8_t)level },
				      3);
	} else {
		/* Invalid channel type */
		ret = -EINVAL;
	}

	return ret >= 0 ? 0 : ret;
}

/**
 * qmu_qtrace_init() - Initialize and return a new Qtrace channel.
 * @cb: Callback.
 * @cb_data: Callback data.
 *
 * Return: Qtrace channel qmhandle on success, else NULL.
 */
qmhandle qmu_qtrace_init(qmchannel_callback cb, void *cb_data)
{
	return common_init(QMCHANNEL_TYPE_QTRACE,
			   sizeof(struct common_channel_data),
			   common_handle_data, cb, cb_data);
}

/**
 * qmu_qtrace_start() - Start the Qtrace channel read thread.
 * @hnd: qmchannel handle.
 *
 * Return: 0 on success, else a negative error code.
 */
int qmu_qtrace_start(qmhandle hnd)
{
	/* Check that handle is QTRACE type */
	if (hnd->type == QMCHANNEL_TYPE_QTRACE) {
		return common_start(hnd, common_thread);
	} else {
		/* Invalid channel type */
		return -EINVAL;
	}
}

/**
 * qmu_qtrace_destroy() - Stop the Qtrace channel read thread and destroy the instance.
 * @hnd: qmchannel handle.
 *
 * Return: 0 on success, else a negative error code.
 */
int qmu_qtrace_destroy(qmhandle hnd)
{
	/* Check that handle is QTRACE type */
	if (hnd->type == QMCHANNEL_TYPE_QTRACE) {
		return common_destroy(hnd);
	} else {
		/* Invalid channel type */
		return -EINVAL;
	}
}
