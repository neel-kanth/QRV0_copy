/*
 * This file contains the entry point for uci bridge app.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#define UCI_BRIDGE_APP_NAME "uci-bridge"

#ifdef CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT

#ifndef CONFIG_CHERRY_CALIB_FOLDER
#define CALIB_APP_USAGE_HELP_COMMAND_EXT ""
#define CALIB_APP_USAGE_HELP_EXT ""
#else
#define CALIB_APP_USAGE_HELP_COMMAND_EXT \
	" [-Y country_code] [-Z configuration_path]"
#define CALIB_APP_USAGE_HELP_EXT                                                          \
	"\t-Y CODE\t\tSelect the country code. Configuration path needs to be set too\n " \
	"\t-Z PATH\t\tSelect the path of the config folder\n "
#endif

#ifdef CONFIG_CHERRY_EXAMPLES_ZEPHYR
#define LINUX_APP_USAGE_HELP_COMMAND_EXT ""
#define LINUX_APP_USAGE_HELP_EXT ""
#else
#define LINUX_APP_USAGE_HELP_COMMAND_EXT \
	" [-b N] [-d device uci] [-p device proxy]"
#define LINUX_APP_USAGE_HELP_EXT                                           \
	"\t-b N\t\tSet baud rate, default is 115200bps\n"                  \
	"\t-d device uci\tSet the device uci path, default is /dev/uci0\n" \
	"\t-p device proxy\tSet the device proxy path, default is /dev/ttyUSB0\n"
#endif

#define UCI_BRIDGE_APP_USAGE_HELP                                                                \
	"\nUsage: " UCI_BRIDGE_APP_NAME                                                          \
	" [-c N] [-l N]" LINUX_APP_USAGE_HELP_COMMAND_EXT                                        \
		CALIB_APP_USAGE_HELP_COMMAND_EXT "\n"                                            \
	"\nOptions:\n"                                                                           \
	"\t-c N\t\tSet calibration: 1 for QM357 sip, 2 for soc, 3 for QM35825, 4 for QM35822\n"  \
	"\t-l N\t\tSet cherry's log level (default is 2 for warning)\n" LINUX_APP_USAGE_HELP_EXT \
		CALIB_APP_USAGE_HELP_EXT
#endif

int main_uci_bridge_app(int argc, char *argv[]);
