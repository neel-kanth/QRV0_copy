/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "qmchannel_priv.h"
#include "qmutils/qmutils.h"

/**
 * qmu_hsspi_init() - Initialize and return a new HSSPI channel.
 * @cb: Callback.
 * @cb_data: Callback data.
 *
 * Return: HSSPI channel qmhandle on success, else NULL.
 */
qmhandle qmu_hsspi_init(qmchannel_callback cb, void *cb_data)
{
	return common_init(QMCHANNEL_TYPE_RESERVED_HSSPI,
			   sizeof(struct common_channel_data),
			   common_handle_data, cb, cb_data);
}

/**
 * qmu_hsspi_start() - Start the HSSPI channel read thread.
 * @hnd: qmchannel handle.
 *
 * Return: 0 on success, else a negative error code.
 */
int qmu_hsspi_start(qmhandle hnd)
{
	return common_start(hnd, common_thread);
}

/**
 * qmu_hsspi_destroy() - Stop the HSSPI channel read thread and destroy the instance.
 * @hnd: qmchannel handle.
 *
 * Return: 0 on success, else a negative error code.
 */
int qmu_hsspi_destroy(qmhandle hnd)
{
	return common_destroy(hnd);
}
