/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#define _GNU_SOURCE 1

#include "uci_transport/uci_transport_serial.h"

#include "uci_transport/uci_transport_fd.h"

#include <errno.h>
#include <fcntl.h>
#include <qlog.h>
#include <qtils.h>
#include <qtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <uci/uci.h>
#include <unistd.h>

static void stty(int fd, speed_t baud)
{
	struct termios settings;

	int rc = tcgetattr(fd, &settings);
	if (rc < 0) {
		fprintf(stderr, "Cannot read current settings\n");
		return;
	}

	cfmakeraw(&settings);

	rc = cfsetspeed(&settings, baud);
	if (rc < 0)
		QLOGE("%s", strerror(errno));

	rc = tcsetattr(fd, TCSANOW, &settings);
	if (rc < 0)
		QLOGE("%s", strerror(errno));
}

struct uci_transport *uci_transport_serial_create(const char *path,
						  speed_t baud, int *fd_out)
{
	struct uci_transport *transport = NULL;
	int fd;

	fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0) {
		return NULL;
	}

	/* Check that input is from a tty. */
	if (!isatty(fd)) {
		close(fd);
		return NULL;
	}

	stty(fd, baud);

	/* Allocate resources */
	transport = uci_transport_fd_create(fd);
	if (!transport)
		return NULL;

	*fd_out = fd;
	return transport;
}

void uci_transport_serial_destroy(struct uci_transport *tr)
{
	struct uci_transport_fd *s =
		qparent_of(tr, struct uci_transport_fd, base);

	close(s->fd);
	uci_transport_fd_destroy(tr);
}

int uci_transport_serial_read(struct uci_transport *tr)
{
	return uci_transport_fd_read(tr);
}
