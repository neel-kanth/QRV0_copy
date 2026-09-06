/*
 * This file contains parameters' structure and define for multi_session_app sample.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <cherry/cherry.h>
#include <stdbool.h>
#include <stdint.h>
#include <util_common_param.h>

#define MULTI_SESSION_APP_NAME "cherry-multi-session-app "

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
#define MULTI_SESSION_APP_USAGE_HELP                                                                     \
	"\nUsage: " MULTI_SESSION_APP_NAME                                                               \
	" [-C] [-s N] [-n N] [-i N] [-l N]" CALIB_APP_USAGE_HELP_COMMAND_EXT                             \
	"\n"                                                                                             \
	"\nOptions:\n"                                                                                   \
	"\t-C N\t\tact as controlee\n"                                                                   \
	"\t-s N\t\tset the number of sessions to create\n"                                               \
	"\t-n N\t\tset the number of measurements\n"                                                     \
	"\t-i N\t\tset the interval duration\n"                                                          \
	"\t-l N\t\tset cherry's log level (default is 2 for warning)\n"                                  \
	"\t-d device\tset the device path (UCI or TTY), default is /dev/uci0\n" CALIB_APP_USAGE_HELP_EXT \
	"\nExample command line:\n"                                                                      \
	"\tController:\tcherry-multi-session-app\n"                                                      \
	"\tControlee:\tcherry-multi-session-app -C\n"                                                    \
	"\nThis example uses the antenna set 0 by default.\n"
#endif

struct app_multi_session_parameter {
	uint16_t max_nb_measurements;
	uint8_t nr_of_sessions;
	int interval_ms;
	bool is_controller;
	const char *device;
	enum cherry_log_level log_level;
#ifdef CONFIG_CHERRY_CALIB_FOLDER
	const char *config_path;
	const char *country_code;
#endif
};

bool get_runtime_multi_session_app_param(
	int argc, char *argv[], struct app_multi_session_parameter *params);
