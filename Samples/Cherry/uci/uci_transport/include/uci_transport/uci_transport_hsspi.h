/*
 * Header file for uci transport over HSSPI
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#ifndef UCI_TRANSPORT_HSSPI_H
#define UCI_TRANSPORT_HSSPI_H

#include <uci/uci.h>

/**
 * uci_transport_hsspi_create() - Create a hsspi transport object.
 *
 * Return: Transport object or null on error.
 */
struct uci_transport *uci_transport_hsspi_create(void);

/**
 * uci_transport_hsspi_destroy() - Destroy a hsspi transport object.
 * @transport: Transport to destroy.
 */
void uci_transport_hsspi_destroy(struct uci_transport *transport);

/**
 * struct uci_transport_hsspi - UCI generic transport channel for HSSPI Async driver.
 */

struct uci_transport_hsspi {
	/**
	 * @uci: UCI context used for transport.
	 */
	struct uci *uci;
	/**
	 * @base: Basic transport we inherit from, must be first.
	 */
	struct uci_transport base;
};

#endif // UCI_TRANSPORT_HSSPI_H
