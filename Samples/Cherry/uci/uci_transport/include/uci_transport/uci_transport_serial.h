/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include "uci/uci.h"

#include <termios.h>

/**
 * uci_transport_serial_create() - Create a serial transport object.
 * @path: Path to serial character device.
 * @baud: Baud rate of the serial port.
 * @fd: File decriptor for the serial device.
 *
 * Return: Transport object or null on error.
 */
struct uci_transport *uci_transport_serial_create(const char *path,
						  speed_t baud, int *fd);

/**
 * uci_transport_serial_destroy() - Destroy a serial transport object.
 * @transport: Transport to destroy.
 */
void uci_transport_serial_destroy(struct uci_transport *transport);

/**
 * uci_transport_serial_read() - Read a packet from the transport.
 * @transport: Transport to read from.
 *
 * Return: Number of bytes read on success, or a negative error.
 */
int uci_transport_serial_read(struct uci_transport *transport);
