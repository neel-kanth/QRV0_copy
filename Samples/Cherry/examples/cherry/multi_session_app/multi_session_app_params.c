/*
 * This file contains parameters' parsing for multi_session_app sample.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "multi_session_app_params.h"

#include <unistd.h>
#include <util_convert.h>
#include <util_dump.h>
#include <util_log.h>
#ifdef CONFIG_CHERRY_EXAMPLES_ZEPHYR
#include <getopt.h>
#endif

#ifdef CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT
#ifdef CONFIG_CHERRY_CALIB_FOLDER
#define OPTSTR "Cs:n:i:hl:d:Y:Z:"
#else
#define OPTSTR "Cs:n:i:hl:d:"
#endif
#endif

#define MAX_SESSIONS_NR 8
#define CONTROLLER_MAC_ADDRESS 10
#define CONTROLEE_MAC_ADDRESS 11

#ifdef CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT
static void multi_session_app_usage(void)
{
	QLOGD("%s", MULTI_SESSION_APP_USAGE_HELP);
}
#endif

bool get_runtime_multi_session_app_param(
	int argc, char *argv[], struct app_multi_session_parameter *params)
{
#ifdef CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT
	int opt;

#ifdef CONFIG_CHERRY_EXAMPLES_ZEPHYR
	/* On zephyr, getopt must be reseted between multiple calls. */
	getopt_init();
#endif

	while ((opt = getopt(argc, argv, OPTSTR)) != -1)
		switch (opt) {
		case 'C':
			params->is_controller = false;
			break;
		case 's':
			if (!util_convert_arg_to_uint8(
				    optarg, &params->nr_of_sessions)) {
				QLOGE("Invalid number for the number of sessions to create.");
				return false;
			}
			if (params->nr_of_sessions < 1 ||
			    params->nr_of_sessions > MAX_SESSIONS_NR) {
				QLOGE("number of sessions has to be between 1 and %d",
				      MAX_SESSIONS_NR);
				return false;
			}
			break;
		case 'n':
			if (!util_convert_arg_to_uint16(
				    optarg, &params->max_nb_measurements)) {
				QLOGE("Invalid number for the number of measurement.");
				return false;
			}
			break;
		case 'i':
			if (!util_convert_arg_to_int32(optarg,
						       &params->interval_ms)) {
				QLOGE("Invalid number for the ranging interval.");
				return false;
			}
			break;
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
			multi_session_app_usage();
			return false;
		}
#endif
	return true;
}
