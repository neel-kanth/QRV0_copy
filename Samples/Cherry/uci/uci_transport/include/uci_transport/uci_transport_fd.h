/*
 * Header file for uci transport on fd
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#ifndef UCI_TRANSPORT_FD_H
#define UCI_TRANSPORT_FD_H

#include "uci/uci.h"

/**
 * uci_transport_fd_create() - Create a fd transport object.
 * @fd: File decriptor for the device.
 *
 * Return: Transport object or null on error.
 */
struct uci_transport *uci_transport_fd_create(int fd);

/**
 * uci_transport_fd_destroy() - Destroy a fd transport object.
 * @transport: Transport to destroy.
 */
void uci_transport_fd_destroy(struct uci_transport *transport);

/**
 * uci_transport_fd_read() - Read a packet from the transport.
 * @transport: Transport to read from.
 *
 * Return: Number of bytes read on success, or a negative error.
 */
int uci_transport_fd_read(struct uci_transport *transport);

/**
 * struct uci_transport_fd - UCI generic transport channel for file descriptor.
 */
struct uci_transport_fd {
	/**
	 * @uci: UCI context used for transport.
	 */
	struct uci *uci;
	/**
	 * @base: Basic transport we inherit from, must be first.
	 */
	struct uci_transport base;
	/**
	 * @fd: File decriptor for the device.
	 */
	int fd;
	/**
	 * @epollfd_wr: Epoll file decriptor to write on the device.
	 */
	int epollfd_wr;
	/**
	 * @epollfd_rd: Epoll file decriptor to read on the device.
	 */
	int epollfd_rd;
};

#endif // UCI_TRANSPORT_FD_H
