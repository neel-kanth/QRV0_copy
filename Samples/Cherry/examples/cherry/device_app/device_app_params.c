/*
 * This file contains parameters' parsing for device_app sample.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "device_app_params.h"

#include <unistd.h>
#include <util_convert.h>
#include <util_log.h>

#ifdef CONFIG_CHERRY_EXAMPLES_ZEPHYR
#include <getopt.h>
#endif

#ifdef CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT
#define OPTSTR "hl:d:rR"

static void device_app_usage(void)
{
	QLOGI("%s", DEVICE_APP_USAGE_HELP);
}
#endif

bool get_runtime_device_app_param(int argc, char *argv[],
				  struct app_device_parameter *params)
{
#ifdef CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT
	int opt;

#ifdef CONFIG_CHERRY_EXAMPLES_ZEPHYR
	/* On zephyr, getopt must be reseted between multiple calls. */
	getopt_init();
#endif

	while ((opt = getopt(argc, argv, OPTSTR)) != -1)
		switch (opt) {
		case 'l':
			if (!util_convert_arg_to_log_level(
				    optarg, &params->log_level)) {
				QLOGE("Invalid log level.");
				return false;
			}
			break;
		case 'd':
			params->device = optarg;
			break;
		case 'r':
			params->reset_soft = true;
			break;
		case 'R':
			params->reset_hard = true;
			break;
		case 'h':
		default:
			device_app_usage();
			return false;
		}
#endif
	return true;
}
