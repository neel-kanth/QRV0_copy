/*
 * This file contains parameters' structure and define for twr_app sample.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <cherry/cherry_fira.h>
#include <stdbool.h>
#include <stdint.h>
#include <util_common_param.h>

#define CONTROLLER_MAC_ADDRESS 10
#define CONTROLEE_1_MAC_ADDRESS 11
#define CONTROLEE_2_MAC_ADDRESS 21

#define TWR_APP_NAME "cherry-twr-app"

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
#define TWR_APP_USAGE_HELP                                                                                                            \
	"\nUsage: " TWR_APP_NAME                                                                                                      \
	" [-C] [-A N] [-D] [-e N] [-i N] [-l N] -[m N] -[n N] [-p N] [-R] [-s N] [-v N] [-d device]" CALIB_APP_USAGE_HELP_COMMAND_EXT \
	"\n"                                                                                                                          \
	"\nOptions:\n"                                                                                                                \
	"\t-C\t\tact as controlee (default act as controller)\n"                                                                      \
	"\t-A N\t\tset address of the device [Controlee Only]\n"                                                                      \
	"\t-D\t\tenable diagnostic messages\n"                                                                                        \
	"\t-a\t\tset Taurus antenna configuration for 360 AoA measurements for the duration of the app\n"                             \
	"\t-e N\t\tset the report config setting (default 1)\n"                                                                       \
	"\t\t\tb0 = TOF report (0: Disable, 1: Enable)\n"                                                                             \
	"\t\t\tb1 = AOA Azimuth report (0: Disable, 1: Enable)\n"                                                                     \
	"\t\t\tb2 = AOA elevation report (0: Disable, 1: Enable)\n"                                                                   \
	"\t\t\tb3 = AOA FOM report (0: Disable, 1: Enable)\n"                                                                         \
	"\t-i N\t\tset the interval duration in ms (default 200ms)\n"                                                                 \
	"\t-l N\t\tset cherry's log level (default is 2 for warning)\n"                                                               \
	"\t-m N\t\tset the max rr retry value (default 0)\n"                                                                          \
	"\t-n N\t\tset the number of measurements (default 50)\n"                                                                     \
	"\t-p N\t\tset the preamble code index (default 10)\n"                                                                        \
	"\t-P N\t\tset a phy parameters set, refer to FiRa PHY Technical specifiations:\n"                                            \
	"\t\t\t1: HPRF set 6\n"                                                                                                       \
	"\t\t\t2: HPRF set 20\n"                                                                                                      \
	"\t\t\t3: BPRF set 3\n"                                                                                                       \
	"\t\t\t4: BPRF set 4\n"                                                                                                       \
	"\t\t\t5: BPRF set 5\n"                                                                                                       \
	"\t\t\t6: BPRF set 6\n"                                                                                                       \
	"\t-R\t\tenable RSSI report\n"                                                                                                \
	"\t-s N\t\tset the session priority value (default 50)\n"                                                                     \
	"\t-S N\t\tset the antenna set to use for this session\n"                                                                     \
	"\t-v N\t\tset the STS mode (default 1 for static mode, set 0 to disable)\n"                                                  \
	"\t-d device\tset the device path (UCI or TTY) (default is /dev/uci0)\n" CALIB_APP_USAGE_HELP_EXT                             \
	"\n"                                                                                                                          \
	"\nExample command line:\n"                                                                                                   \
	"\tController :\t\tcherry-twr-app\n"                                                                                          \
	"\tControlee :\t\tcherry-twr-app -C\n"                                                                                        \
	"\nThis example uses the antenna set 0 by default.\n"
#endif

extern const struct cherry_fira_phy_params phy_params_set[];

extern const uint16_t controlee_addresses[];

struct app_twr_parameter {
	uint16_t short_addr;
	uint32_t session_id;
	bool is_controller;
	bool diagnostic_enabled;
	bool aoa_360_enabled;
	uint16_t max_nb_measurements;
	int interval_ms;
	uint8_t preamble_code_index;
	uint8_t report_rssi;
	uint8_t session_priority;
	uint8_t ant_set_id;
	uint16_t max_rr_retry;
	uint8_t result_report_config;
	uint8_t sts_config;
	const struct cherry_fira_phy_params *phy_params;
	const char *device;
	enum cherry_log_level log_level;
#ifdef CONFIG_CHERRY_CALIB_FOLDER
	const char *config_path;
	const char *country_code;
#endif
};

bool get_runtime_twr_app_param(int argc, char *argv[],
			       struct app_twr_parameter *params);
