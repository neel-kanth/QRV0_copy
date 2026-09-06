/*
 * Header file for uci transport on chardev
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#ifndef UCI_TRANSPORT_CHARDEV_H
#define UCI_TRANSPORT_CHARDEV_H

#include "uci/uci.h"

/**
 * uci_transport_chardev_create() - Create a character device transport object.
 * @path: Path to character device.
 * @fd_out: File decriptor for the character device.
 *
 * Return: Transport object or null on error.
 */
struct uci_transport *uci_transport_chardev_create(const char *path,
						   int *fd_out);

/**
 * uci_transport_chardev_destroy() - Destroy a character device transport object.
 * @transport: Transport to destroy.
 */
void uci_transport_chardev_destroy(struct uci_transport *transport);

/**
 * uci_transport_chardev_read() - Read a packet from the transport.
 * @transport: Transport to read from.
 *
 * Return: Number of bytes read on success, or a negative error.
 */
int uci_transport_chardev_read(struct uci_transport *transport);

#endif // UCI_TRANSPORT_CHARDEV_H
