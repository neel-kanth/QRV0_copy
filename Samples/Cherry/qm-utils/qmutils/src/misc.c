/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */
#include <stdio.h>
#include <string.h>

#include "qm35_uci_dev_ioctl.h"
#include "qmutils/qmutils.h"

/**
 * qmu_reset() - Force a QM chip reset.
 * @hnd: qmchannel handle.
 * @bootrom: Reset to bootrom command mode.
 *
 * Return: 0 on success, else a negative error code.
 */
int qmu_reset(qmhandle hnd, bool bootrom)
{
	unsigned int bootrom_value;

	bootrom_value = bootrom;

	return qmchannel_ioctl(hnd, QM35_CTRL_RESET_EXT,
			       (char *)&bootrom_value);
}

/**
 * qmu_fwupdate() - Force a QM chip FW update.
 * @hnd: qmchannel handle.
 * @name: Firmware name. Optional: it can be set to NULL.
 *
 * Return: 0 on success, else a negative error code.
 */
int qmu_fwupdate(qmhandle hnd, char *name)
{
	const char *const warning =
		"Warning! Firmware name length (including null end char) "
		"exceed max size (%d bytes): truncated.";
	int ret;

	if (name == NULL) {
		unsigned int state;
		char *ptr_state = (char *)&state;

		/* No filename */
		ret = qmchannel_ioctl(hnd, QM35_CTRL_FW_UPLOAD, ptr_state);
	} else {
		struct qm35_fwupload_params params = { 0 };

		/* Warn user if name size exceed max allowed size (truncated name) */
		if ((strlen(name) + 1) > QM35_FIRMWARE_FILENAME_SIZE) {
			fprintf(stderr, warning, QM35_FIRMWARE_FILENAME_SIZE);
		}
		strncpy(params.fw_name, name, QM35_FIRMWARE_FILENAME_SIZE - 1);

		/* Call driver IOCTL with given fw filename */
		ret = qmchannel_ioctl(hnd, QM35_CTRL_FW_UPLOAD_EXT,
				      (char *)&params);
	}

	return ret >= 0 ? 0 : ret;
}

/**
 * qmu_disable_irq() - Disable IRQ line.
 * @hnd: qmchannel handle.
 *
 * Return: 0 on success, else a negative error code.
 */
int qmu_disable_irq(qmhandle hnd)
{
	unsigned int enabled = 0;

	return qmchannel_ioctl(hnd, QM35_CTRL_IRQ, (char *)&enabled);
}

/**
 * qmu_enable_irq() - Enable IRQ line.
 * @hnd: qmchannel handle.
 *
 * Return: 0 on success, else a negative error code.
 */
int qmu_enable_irq(qmhandle hnd)
{
	unsigned int enabled = 1;

	return qmchannel_ioctl(hnd, QM35_CTRL_IRQ, (char *)&enabled);
}

/**
 * qmu_wait_for_irq_line() - Wait for IRQ line to be asserted.
 * @hnd: qmchannel handle.
 * @timeout_ms: Timeout in milliseconds.
 *
 * Return: 0 on success, else a negative error code.
 */
int qmu_wait_for_irq_line(qmhandle hnd, int timeout_ms)
{
	unsigned int timeout = timeout_ms;

	return qmchannel_ioctl(hnd, QM35_CTRL_WAIT_IRQ, (char *)&timeout);
}

/**
 * qmu_raw_transfer() - Perform raw SPI transfer.
 * @hnd: qmchannel handle.
 * @tx_buf: Buffer to send.
 * @rx_buf: Buffer to receive.
 * @len: Length of the transfer.
 *
 * Return: 0 on success, else a negative error code.
 */
int qmu_raw_transfer(qmhandle hnd, void *tx_buf, void *rx_buf, size_t len)
{
	struct qm35_spi_transfer_params params = { 0 };

	params.tx_buf = tx_buf;
	params.rx_buf = rx_buf;
	params.len = len;

	return qmchannel_ioctl(hnd, QM35_CTRL_SPI_TRANSFER, (char *)&params);
}
