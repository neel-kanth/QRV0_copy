/*
 * This file contains parameters' parsing for radar_app sample.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "radar_app_params.h"

#include <unistd.h>
#include <util_convert.h>
#include <util_log.h>
#ifdef CONFIG_CHERRY_EXAMPLES_ZEPHYR
#include <getopt.h>
#endif

#ifdef CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT
#ifndef CONFIG_CHERRY_EXAMPLES_ZEPHYR
#ifndef CONFIG_CHERRY_CALIB_FOLDER
#define OPTSTR "hl:d:b:s:n:B:S:o:i:f:"
#else
#define OPTSTR "hl:d:b:s:n:B:S:o:i:f:Y:Z:"
#endif
#else
#define OPTSTR "hl:d:b:s:n:B:S:o:i:"
#endif

static void radar_app_usage(void)
{
	QLOGD("%s", RADAR_APP_USAGE_HELP);
}

#endif

bool get_runtime_radar_app_param(int argc, char *argv[],
				 struct app_radar_parameter *params)
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
		case 'b':
			if (!util_convert_arg_to_uint32(
				    optarg, &params->burst_period_ms)) {
				QLOGE("Invalid value for the burst period ms.");
				return -1;
			}
			break;
		case 's':
			if (!util_convert_arg_to_uint16(
				    optarg, &params->sweep_period_rstu)) {
				QLOGE("Invalid value for the sweep period RSTU.");
				return -1;
			}
			break;
		case 'n':
			if (!util_convert_arg_to_uint16(
				    optarg, &params->number_of_bursts)) {
				QLOGE("Invalid value for the number of bursts.");
				return -1;
			}
			break;
		case 'B':
			if (!util_convert_arg_to_uint8(
				    optarg, &params->sweeps_per_burst)) {
				QLOGE("Invalid value for the sweep per burst.");
				return -1;
			}
			break;
		case 'S':
			if (!util_convert_arg_to_uint8(
				    optarg, &params->samples_per_sweep)) {
				QLOGE("Invalid value for the samples per sweep.");
				return -1;
			}
			break;
		case 'o':
			if (!util_convert_arg_to_int16(optarg,
						       &params->sweep_offset)) {
				QLOGE("Invalid value for the sweep offset.");
				return -1;
			}
			break;
		case 'i':
			if (!util_convert_arg_to_uint8(
				    optarg, &params->tx_profile_idx)) {
				QLOGE("Invalid value for the profile idx.");
				return -1;
			}
			break;
#ifndef CONFIG_CHERRY_EXAMPLES_ZEPHYR
		case 'f':
			params->file = optarg;
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
#endif

		case 'h':
		default:
			radar_app_usage();
			return false;
		}
#endif
	return true;
}
