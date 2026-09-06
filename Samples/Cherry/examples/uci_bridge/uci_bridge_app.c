/*
 * Example for Cherry API get device info implementation
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */
#include <cherry/cherry_calib_folder.h>
#include <cherry/cherry_proxy.h>
#include <errno.h>
#include <qtime.h>
#ifndef CONFIG_CHERRY_EXAMPLES_ZEPHYR
#include <signal.h>
#else
#include <getopt.h>
#endif
#include "uci_bridge_app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util_calib_qm357_sip.h>
#include <util_calib_qm35822.h>
#include <util_calib_qm35825.h>
#include <util_calib_soc.h>
#include <util_convert.h>
#include <util_log.h>

#ifdef CONFIG_CHERRY_CALIB_FOLDER
#define OPTSTR "hb:c:d:p:l:Y:Z:"
#else
#define OPTSTR "hb:c:d:p:l:"
#endif

#ifndef CONFIG_CHERRY_EXAMPLES_ZEPHYR
static int terminate = 0;
/* Dummy signal handler to make pause() return */
void handler(int signum)
{
	terminate = 1;
}
#endif

int main_uci_bridge_app(int argc, char *argv[])
{
	int ret = 0;
#ifndef CONFIG_CHERRY_EXAMPLES_ZEPHYR
	struct sigaction sa;
#endif
	enum cherry_err rc;
	struct cherry_proxy *proxy_ctx;
	enum cherry_log_level log_level = CHERRY_LOG_LEVEL_WARN;
	int opt;
	char *endptr;
	char *help = UCI_BRIDGE_APP_USAGE_HELP;
	uint8_t calib_nb = 0;
#ifndef CONFIG_CHERRY_EXAMPLES_ZEPHYR
	speed_t baud_rate = B115200;
#else
	int baud_rate = 0;
#endif
	char device_uci[32];
	char device_proxy[32];
	const struct cherry_calib *calib = NULL;

#ifdef CONFIG_CHERRY_CALIB_FOLDER
	const char *config_path = NULL;
	const char *country_code = NULL;
#endif

	/* Set default path for UCI char device and proxy */
	strcpy(device_uci, "/dev/uci0");
	strcpy(device_proxy, "/dev/ttyUSB0");

#ifdef CONFIG_CHERRY_EXAMPLES_ZEPHYR
	/* On zephyr, getopt must be reseted between multiple calls. */
	getopt_init();
#endif
	/* Manage application options */
	while ((opt = getopt(argc, argv, OPTSTR)) != -1) {
		switch (opt) {
#ifndef CONFIG_CHERRY_EXAMPLES_ZEPHYR
		case 'b':
			baud_rate = util_conver_long_to_speed_t(
				strtol(optarg, &endptr, 10));
			if (*endptr || baud_rate == B0) {
				QLOGE("Invalid baud rate.");
				return -EINVAL;
			}
			break;
#endif
		case 'c':
			calib_nb = (uint8_t)strtol(optarg, &endptr, 10);
			if (*endptr || calib_nb < 1 || calib_nb > 4) {
				QLOGE("Invalid calib number.");
				return -EINVAL;
			}
			break;
#ifndef CONFIG_CHERRY_EXAMPLES_ZEPHYR
		case 'd':
			strncpy(device_uci, optarg, sizeof(device_uci));
			break;
		case 'p':
			strncpy(device_proxy, optarg, sizeof(device_proxy));
			break;
#endif
		case 'l':
			log_level = (uint8_t)strtol(optarg, &endptr, 10);
			if (*endptr || log_level < CHERRY_LOG_LEVEL_NONE ||
			    log_level > CHERRY_LOG_LEVEL_DEBUG) {
				QLOGE("Invalid log level.");
				return -EINVAL;
			}
			break;
#ifdef CONFIG_CHERRY_CALIB_FOLDER
		case 'Y':
			country_code = optarg;
			break;
		case 'Z':
			config_path = optarg;
			break;
#endif
		case 'h':
		default:
			QLOGD("%s", help);
			return 0;
		}
	}

#ifndef CONFIG_CHERRY_EXAMPLES_ZEPHYR
	/* Register SIGINT and SIGTERM handler */
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = handler;
	if (sigaction(SIGINT, &sa, NULL) < 0) {
		QLOGE("sigaction failed");
		ret = -errno;
		goto error;
	}
	if (sigaction(SIGTERM, &sa, NULL) < 0) {
		QLOGE("sigaction failed");
		ret = -errno;
		goto error;
	}
#endif

	/* Create a Cherry proxy instance */
	proxy_ctx = cherry_proxy_create(device_uci, device_proxy, baud_rate);
	if (!proxy_ctx) {
		QLOGE("cherry_proxy_create failed");
		ret = -EINVAL;
		goto error;
	}

	/* Set given log level for cherry or Warning level by default */
	rc = cherry_proxy_set_log_level(proxy_ctx, log_level,
					CHERRY_LOG_MODULE_ALL);
	if (rc != CHERRY_ERR_NONE) {
		QLOGE("cherry_proxy_set_log_level has failed (err = %d)", rc);
		ret = -EINVAL;
		goto error_destroy;
	}

#ifdef CONFIG_CHERRY_CALIB_FOLDER
	/* Load a calibration file with or without Country Code (optional) */
	if (config_path != NULL) {
		if (calib_nb != 0) {
			QLOGW("Calibration file SIP or SOC has been ignored "
			      "(use given calibration file)");
		}
		calib = cherry_calib_folder_load(config_path, country_code);
		if (calib == NULL) {
			QLOGE("Failed to load calibration file");
			goto error_destroy;
		} else {
			QLOGI("Calibration file loaded successfully !");
		}
	} else
#endif
		switch (calib_nb) {
		case 0:
			/* No calibration */
			break;
		case 1:
			calib = &util_calib_qm357_sip;
			break;
		case 2:
			calib = &util_calib_soc;
			break;
		case 3:
			calib = &util_calib_qm35825;
			break;
		case 4:
			calib = &util_calib_qm35822;
			break;
		default:
			QLOGW("Unexpected calibration index. Nothing set.");
			break;
		}

	/* Start cherry proxy */
	rc = cherry_proxy_start(proxy_ctx, calib);
	if (rc != CHERRY_ERR_NONE) {
		QLOGE("cherry_proxy_start has failed !!! (err = %d)", rc);
		ret = -EINVAL;
		goto error_destroy;
	}

#ifndef CONFIG_CHERRY_EXAMPLES_ZEPHYR
	/* Wait for a signal to exit */
	while (!terminate)
		pause();
#else
	return 0;
#endif

	/* Stop cherry proxy */
	rc = cherry_proxy_stop(proxy_ctx);
	if (rc != CHERRY_ERR_NONE) {
		QLOGE("cherry_proxy_stop has failed !!! (err = %d)", rc);
		ret = -EINVAL;
	}

error_destroy:
	/* No return code on destroy */
	cherry_proxy_destroy(proxy_ctx);
#ifdef CONFIG_CHERRY_CALIB_FOLDER
	/* Destroy calib */
	if (config_path && calib) {
		cherry_calib_destroy((struct cherry_calib *)calib);
	}
#endif

error:
	return ret;
}

#ifndef CONFIG_CHERRY_EXAMPLES_ZEPHYR
int main(int argc, char *argv[])
{
	return main_uci_bridge_app(argc, argv);
}
#endif
