/*
 * Implementation for uci transport on chardev
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#define _GNU_SOURCE 1

#include "uci_transport/uci_transport_chardev.h"

#include "uci_transport/uci_transport_fd.h"

#include <fcntl.h>
#include <qtils.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <uci/uci.h>
#include <unistd.h>

struct uci_transport *uci_transport_chardev_create(const char *path,
						   int *fd_out)
{
	struct uci_transport *transport = NULL;
	int fd;

	fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0) {
		return NULL;
	}

	/* Allocate resources */
	transport = uci_transport_fd_create(fd);
	if (!transport)
		return NULL;

	*fd_out = fd;
	return transport;
}

void uci_transport_chardev_destroy(struct uci_transport *tr)
{
	struct uci_transport_fd *s =
		qparent_of(tr, struct uci_transport_fd, base);

	close(s->fd);
	uci_transport_fd_destroy(tr);
}

int uci_transport_chardev_read(struct uci_transport *tr)
{
	return uci_transport_fd_read(tr);
}
