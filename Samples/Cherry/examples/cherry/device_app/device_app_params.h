/*
 * This file contains parameters' structure and define for device_app sample.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <cherry/cherry.h>
#include <stdbool.h>

#define DEVICE_APP_NAME "cherry-device-app"

#ifdef CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT
#define DEVICE_APP_USAGE_HELP                                                   \
	"\nUsage: " DEVICE_APP_NAME " [-l N] [-d device] [-r] [-R]\n"           \
	"\nOptions:\n"                                                          \
	"\t-l N\t\tset cherry's log level (default is 2 for warning)\n"         \
	"\t-d device\tset the device path (UCI or TTY), default is /dev/uci0\n" \
	"\t-r\t\tdo a soft reset of the UWBS\n"                                 \
	"\t-R\t\tdo a hard reset of the UWBS\n"                                 \
	"\n"                                                                    \
	"\nExample command line:\n"                                             \
	"\tActivate UCI Logs:\tcherry-device-app -l 4\n"
#endif

struct app_device_parameter {
	bool reset_soft;
	bool reset_hard;
	const char *device;
	enum cherry_log_level log_level;
};

bool get_runtime_device_app_param(int argc, char *argv[],
				  struct app_device_parameter *params);
