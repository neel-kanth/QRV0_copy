/*
 * This file contains parameters' parsing for get_calib_app sample.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "get_calib_app_params.h"

#include <unistd.h>
#include <util_convert.h>
#include <util_log.h>
#ifdef CONFIG_CHERRY_EXAMPLES_ZEPHYR
#include <getopt.h>
#endif

#ifdef CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT
#ifdef CONFIG_CHERRY_CALIB_FOLDER
#define OPTSTR "hl:d:aY:Z:"
#else
#define OPTSTR "hl:d:a"
#endif
#endif

#ifdef CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT
static void get_calib_app_usage(void)
{
	QLOGD("%s", GET_CALIB_APP_USAGE_HELP);
}
#endif

bool get_runtime_get_calib_app_param(int argc, char *argv[],
				     struct app_get_calib_parameter *params)
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
		case 'a':
			params->all_config = true;
			break;

#ifdef CONFIG_CHERRY_CALIB_FOLDER
		case 'Y': {
			params->country_code = optarg;
			break;
		}
		case 'Z': {
			params->config_path = optarg;
			break;
		}
#endif

		case 'h':
		default:
			get_calib_app_usage();
			return false;
		}
#endif
	return true;
}
