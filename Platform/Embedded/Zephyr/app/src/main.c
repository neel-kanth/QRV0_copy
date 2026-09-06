/**
 * @file      main.c
 *
 * @brief     Example entrance. Prepare QM35 and run the example(s)
 *
 * @author    Qorvo Paris Applications
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */

#include <app_version.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <twr_app.h>
#include <twr_app_params.h>
#include <device_app.h>
#include <device_app_params.h>
#include <dl_tdoa_app.h>
#include <dl_tdoa_app_params.h>

#include <radar_app.h>
#include <radar_app_params.h>
#include <multi_session_app.h>
#include <multi_session_app_params.h>
#include <get_calib_app.h>
#include <get_calib_app_params.h>
#include <uci_bridge_app.h>
#include <shell_utils.h>

#include "drivers/uwb/shell.h"
#include "shell_cmd_hyphen.h"

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

int qsoc_flash_bridge_cmd(const struct shell *shell, size_t argc, char **argv);
int main_qtrace(int argc, char *argv[]);

#define QTRACE_USAGE_HELP                                                      \
	"\nUsage: qtrace [-s] [-e]\n"                                          \
	"\nOptions:\n"                                                         \
	"\t-s Start qtrace read and forward to the host PC (via JLink RTT).\n" \
	"\t-e End qtrace read and forward to the host PC.\n"

static int uwb_get_calib_cmd(const struct shell *shell, size_t argc,
			     char **argv)
{
	return main_get_calib_app(argc, argv);
}

static int uwb_multi_session_cmd(const struct shell *shell, size_t argc,
				 char **argv)
{
	return main_multi_session_app(argc, argv);
}

static int uwb_radar_cmd(const struct shell *shell, size_t argc, char **argv)
{
	return main_radar_app(argc, argv);
}

static int uwb_dl_tdoa_cmd(const struct shell *shell, size_t argc, char **argv)
{
	return main_dl_tdoa_app(argc, argv);
}

static int uwb_twr_cmd(const struct shell *shell, size_t argc, char **argv)
{
	return main_twr_app(argc, argv);
}

static int uwb_device_cmd(const struct shell *shell, size_t argc, char **argv)
{
	return main_device_app(argc, argv);
}

static int uwb_qtrace_cmd(const struct shell *shell, size_t argc, char **argv)
{
	return main_qtrace(argc, argv);
}

int main(void)
{
	SHELL_CMD_REGISTER(cherry_device_app, "cherry-device-app", NULL,
			   DEVICE_APP_USAGE_HELP, uwb_device_cmd);
	SHELL_CMD_REGISTER(cherry_twr_app, "cherry-twr-app", NULL,
			   TWR_APP_USAGE_HELP, uwb_twr_cmd);
	SHELL_CMD_REGISTER(cherry_dl_tdoa_app, "cherry-dl-tdoa-app", NULL,
			   DL_TDOA_APP_USAGE_HELP, uwb_dl_tdoa_cmd);
	SHELL_CMD_REGISTER(cherry_radar_app, "cherry-radar-app", NULL,
			   RADAR_APP_USAGE_HELP, uwb_radar_cmd);
	SHELL_CMD_REGISTER(cherry_multi_session_app, "cherry-multi-session-app",
			   NULL, MULTI_SESSION_APP_USAGE_HELP,
			   uwb_multi_session_cmd);
	SHELL_CMD_REGISTER(cherry_get_calib_app, "cherry-get-calib-app", NULL,
			   GET_CALIB_APP_USAGE_HELP, uwb_get_calib_cmd);
	SHELL_CMD_REGISTER(hsspi_tests, "hsspi-tests", NULL,
			   "Run some HSSPI driver tests", uwb_hsspi_tests_cmd);
	SHELL_CMD_REGISTER(qtrace, "qtrace", NULL, QTRACE_USAGE_HELP,
			   uwb_qtrace_cmd);
	SHELL_CMD_REGISTER(qsoc_flash_bridge, "qsoc-flash-bridge", NULL,
			   "QSoC-Flash bridge", qsoc_flash_bridge_cmd);
	SHELL_CMD_REGISTER(uci_bridge, "uci-bridge", NULL,
			   UCI_BRIDGE_APP_USAGE_HELP, uwb_uci_bridge_cmd);

	LOG_INF("Zephyr UWB Example Application %s",
		STRINGIFY(APP_BUILD_VERSION));

	user_button_init();
	while (1) {
		k_msleep(100);
	}
	return 0;
}
