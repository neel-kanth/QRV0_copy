/*
 * Example to trigger QM35 firmware upload
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "uci_transport/uci_transport_chardev_ioctl.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	int fd;
	unsigned int arg = 0;
	int ret = 0;

	if (argc < 2 || argc > 3) {
		printf("Usage: %s <uci_device> [firmware_filename]\n", argv[0]);
		ret = -EINVAL;
		goto exit;
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		ret = errno;
		perror("Error opening device");
		goto exit;
	}

	if (argc <= 2) {
		ret = ioctl(fd, QM35_CTRL_FW_UPLOAD, &arg);
	} else {
		struct qm35_fwupload_params ext_arg;
		strncpy(ext_arg.fw_name, argv[2], QM35_FIRMWARE_FILENAME_SIZE);
		ext_arg.fw_name[QM35_FIRMWARE_FILENAME_SIZE - 1] = '\0';
		ret = ioctl(fd, QM35_CTRL_FW_UPLOAD_EXT, &ext_arg);
	}

	if (ret < 0) {
		ret = errno;
		perror("Error sending firmware upload request");
		goto exit_close;
	}

	printf("Firmware upload request returned %d\n", ret);

exit_close:
	close(fd);
exit:
	return ret;
}
