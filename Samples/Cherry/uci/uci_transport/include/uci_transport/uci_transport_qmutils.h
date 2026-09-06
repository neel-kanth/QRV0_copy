/*
 * Header file for uci transport using qm-utils library
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#ifndef UCI_TRANSPORT_QMUTILS_H
#define UCI_TRANSPORT_QMUTILS_H

#include "qmutils/qmchannel.h"
#include "uci/uci.h"

/**
 * uci_transport_qmutils_create() - Create a fd transport object.
 * @fd: File decriptor for the device.
 *
 * Return: Transport object or null on error.
 */
struct uci_transport *uci_transport_qmutils_create(int *fdout);

/**
 * uci_transport_qmutils_destroy() - Destroy a fd transport object.
 * @transport: Transport to destroy.
 */
void uci_transport_qmutils_destroy(struct uci_transport *transport);

/**
 * uci_transport_qmutils_read() - Read a packet from the transport.
 * @transport: Transport to read from.
 *
 * Return: Number of bytes read on success, or a negative error.
 */
int uci_transport_qmutils_read(struct uci_transport *transport);

/**
 * uci_transport_qmutils_reset() - Read a packet from the transport.
 * @transport: Transport to read from.
 *
 * Return: Number of bytes read on success, or a negative error.
 */
int uci_transport_qmutils_reset(struct uci_transport *transport);

/**
 * struct uci_transport_qmutils - UCI generic transport channel for qm-utils library.
 */
struct uci_transport_qmutils {
	/**
	 * @uci: UCI context used for transport.
	 */
	struct uci *uci;
	/**
	 * @base: Basic transport we inherit from, must be first.
	 */
	struct uci_transport base;
	/**
	 * @uci_channel: Opaque UCI channel.
	 */
	qmhandle uci_channel;
	/**
	 * @uci_pkt: Pre-allocated packet for zero-copy.
	 */
	struct uci_blk *uci_pkt;
};

#endif // UCI_TRANSPORT_QMUTILS_H
