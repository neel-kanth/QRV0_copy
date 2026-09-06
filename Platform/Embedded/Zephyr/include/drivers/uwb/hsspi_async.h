/**
 * @file      hsspi_helpers.h
 *
 * @brief     Asynchronous HSSPI API
 *
 * @author    Qorvo Paris Applications
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */

#pragma once

#include <drivers/uwb/bypass.h>
#include <stdint.h>
#include <zephyr/device.h>

struct hsspi_driver_data;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Setup field used by HSSPI helper functions.
 *
 * This function is called at then of the device initialisation to setup
 * fields required by the helper functions.
 *
 * @param data Pointer to device data to setup.
 * @returns 0 in case of success.
 * @returns errno error code in case of failure.
 */
int hsspi_helpers_init(struct hsspi_driver_data *data);

/**
 * @brief Synchronously sends the tx_buffer content using HSSPI protocol.
 *
 * This function blocks. The tx_buffer is internally queued to be sent by the
 * Zephyr system worqueue. When sent, the result is returned.
 *
 * @param ul Upper Layer protocol value
 * @param tx_buffer Pointer to the buffer to send. It shall be previously
 * allocated on the HEAP. The ownership of the pointer is given to the HSSPI ASYNC
 * system which will free it once sent.
 * @param len Lenght of the tx_buffer.
 * @returns 0 in case of success.
 * @returns errno error code in case of failure.
 */
int hsspi_sync_write(enum qm3x_transport_msg_type ul, uint8_t *tx_buffer,
		     uint16_t len);

#ifdef __cplusplus
}
#endif
