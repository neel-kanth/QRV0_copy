/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#ifndef CHERRY_UCI_TRANSPORT_H
#define CHERRY_UCI_TRANSPORT_H

#include <cherry/cherry.h>
#ifndef CONFIG_CHERRY_ZEPHYR
#include <uci_transport/uci_transport_chardev.h>
#include <uci_transport/uci_transport_chardev_ioctl.h>
#include <uci_transport/uci_transport_serial.h>
#else
#include <uci_transport/uci_transport_hsspi.h>
#endif

/**
 * struct cherry_uci_transport - Cherry UCI transport structure.
 * It hold the Cherry uci transport context.
 * @uci: UCI instance.
 * @reader_thread: Pointer to the reader thread instance.
 * @uci_fd: UCI file descriptor.
 * @transport: UCI transport instance.
 * @reading: Boolean to indicate the thread to leave.
 * @serial: Boolean to indicate if the transport is serial or not.
 */
struct cherry_uci_transport {
	struct uci *uci;
	struct qthread *reader_thread;
	int uci_fd;
	struct uci_transport *transport;
	bool reading;
	bool serial;
};

/**
 * cherry_init_transport() - Create the UCI transport for a cherry context.
 * @ctx: Cherry uci transport context.
 * @device: Device path.
 *
 * Returns:
 *  - &CHERRY_ERR_NONE if succeed
 *  - &CHERRY_ERR_INTERNAL for other errors
 */
enum cherry_err cherry_init_transport(struct cherry_uci_transport *ctx,
				      struct uci *uci, const char *device);

/**
 * cherry_de_init_transport() - Send a command to the Cherry thread.
 * @ctx: Cherry uci transport context.
 */
void cherry_de_init_transport(struct cherry_uci_transport *ctx);

/**
 * cherry_client_reset() - Reset the UCI client.
 * @ctx: Cherry uci transport context.
 *
 * Return: 0 on success and -1 on fail.
 */
int cherry_client_reset(struct cherry_uci_transport *ctx);

#endif /* CHERRY_UCI_TRANSPORT_H */
