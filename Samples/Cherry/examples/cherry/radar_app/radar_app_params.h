/*
 * This file contains parameters' structure and define for radar_app sample.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <cherry/cherry.h>
#include <stdint.h>

#define RADAR_APP_NAME "cherry-radar-app"

#ifdef CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT
#ifdef CONFIG_CHERRY_EXAMPLES_ZEPHYR
#define RADAR_APP_USAGE_HELP_COMMAND_EXT ""
#define RADAR_APP_USAGE_HELP_EXT ""
#else
#define RADAR_APP_USAGE_HELP_COMMAND_EXT " [-f file]"
#define RADAR_APP_USAGE_HELP_EXT \
	"\t-f N\t\tFile to write raw sweep to a file \n"
#endif

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

#define RADAR_APP_USAGE_HELP                                                                                    \
	"\nUsage: " RADAR_APP_NAME                                                                              \
	" [-l N] [-d device] [-b N] [-s N] [-n N] [-B N] [-S N] [-o N] [-i N]" RADAR_APP_USAGE_HELP_COMMAND_EXT \
		CALIB_APP_USAGE_HELP_COMMAND_EXT "\n"                                                           \
	"\nOptions:\n"                                                                                          \
	"\t-l N\t\tSet cherry's log level (default is 2 for warning)\n"                                         \
	"\t-d device\tset the device path (UCI or TTY), default is /dev/uci0\n"                                 \
	"\t-b N\t\tSet burst period, default 100ms\n"                                                           \
	"\t-s N\t\tSet sweep period in RSTU (default is 1200 for 1ms)\n"                                        \
	"\t-n N\t\tSet number of bursts, default 100, 0 for infinite loop (press a key to stop)\n"              \
	"\t-B N\t\tSet sweeps per burst, default 1\n"                                                           \
	"\t-S N\t\tSet samples per sweep, default 64\n"                                                         \
	"\t-o N\t\tSet sweep offset, default -10\n"                                                             \
	"\t-i N\t\tSet tx profile idx, default, 0\n" CALIB_APP_USAGE_HELP_EXT                                   \
	"\nExample command line:\n"                                                                             \
	"\tMore dynamic and infinite Radar:\tcherry-radar-app -n 0 -b 50\n"                                     \
	"\tDefault Radar config:\t\t\tcherry-radar-app\n"                                                       \
	"\nThis example uses the antenna set 3 by default.\n"
#endif

/* The QM firmware returns 157 sample at most. */
#define MAX_SAMPLES_PER_CIR 157
/* In PRF64 and a 1024 preamble length, the maximum of a sample is 65536.
 * For example: "-65536-65536j,"
 * So 14 bytes per sample with comma. */
#define MAX_SAMPLE_LEN 14
/* CIR are prefixed by "CIR:". */
#define CIR_HEADER_LEN 4

struct app_radar_parameter {
	uint32_t burst_period_ms;
	uint16_t sweep_period_rstu;
	uint8_t sweeps_per_burst;
	uint8_t samples_per_sweep;
	uint16_t number_of_bursts;
	int16_t sweep_offset;
	uint8_t tx_profile_idx;
	const char *file;
	const char *device;
	enum cherry_log_level log_level;
#ifdef CONFIG_CHERRY_CALIB_FOLDER
	const char *config_path;
	const char *country_code;
#endif
};

bool get_runtime_radar_app_param(int argc, char *argv[],
				 struct app_radar_parameter *params);
