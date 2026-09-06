/*
 * This file contains parameters' structure and define for get_calib_app sample.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <cherry/cherry.h>
#include <stdbool.h>
#include <stdint.h>
#include <util_common_param.h>

#define GET_CALIB_APP_NAME "cherry-get_calib-app"

#ifndef CONFIG_CHERRY_CALIB_FOLDER
#define CALIB_APP_USAGE_HELP_COMMAND_EXT ""
#define CALIB_APP_USAGE_HELP_EXT ""
#else
#define CALIB_APP_USAGE_HELP_COMMAND_EXT \
	" [-Y country_code] [-Z configuration_path]"
#define CALIB_APP_USAGE_HELP_EXT                                                           \
	"\t-Y CODE\t\tSelect the country code. Configuration path needds to be set too\n " \
	"\t-Z PATH\t\tSelect the path of the config folder\n "
#endif

#ifdef CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT
#define GET_CALIB_APP_USAGE_HELP                                                      \
	"\nUsage: " GET_CALIB_APP_NAME                                                \
	" [-l N] [-d device] [-a]" CALIB_APP_USAGE_HELP_COMMAND_EXT "\n"              \
	"\nOptions:\n"                                                                \
	"\t-l N\t\tset cherry's log level (default is 2 for warning)\n"               \
	"\t-d device\tset the device path (UCI or TTY) (default is /dev/uci0)\n"      \
	"\t-a\t\tget all keys without setting them before\n" CALIB_APP_USAGE_HELP_EXT \
	"\n"                                                                          \
	"\nExample command line:\n"                                                   \
	"\tSetting keys and dumping them:\t\tcherry-get-calib-app\n"                  \
	"\tDumping current keys:\t\t\tcherry-get-calib-app -a\n"
#endif

struct app_get_calib_parameter {
	const char *device;
	enum cherry_log_level log_level;
	bool all_config;
#ifdef CONFIG_CHERRY_CALIB_FOLDER
	const char *config_path;
	const char *country_code;
#endif
};

bool get_runtime_get_calib_app_param(int argc, char *argv[],
				     struct app_get_calib_parameter *params);
