/*
 * Test for Cherry API reload calibration implementation.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include <cherry/cherry.h>
#include <qlog.h>
#include <qsemaphore.h>
#include <qtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define OPTSTR "hl:d:"

/*
 * This example:
 * - Creates the Cherry context.
 * - Set some calibration's keys.
 * - sleep 10 seconds for which we can perform manual reset.
 * - After reset we have to receive the device status notification.
 *   And the Calibration's keys should be reloaded automatically by Cherry.
 * - Finally, a soft reset is done. The device status notification should be received
 *   and the Calibration's keys reloaded.
*/

struct app_context {
	struct cherry *cherry_ctx;
	bool soft_reset;
};

void core_cb(struct cherry_core_event *event, void *user_data)
{
	struct app_context *app_ctx = user_data;

	switch (event->type) {
	case CHERRY_CORE_EVENT_TYPE_DEVICE_STATUS:
		switch (event->data.device_status->state) {
		case CHERRY_CORE_DEVICE_STATE_READY:
			QLOGD("Device status changed: CHERRY_CORE_DEVICE_STATE_READY");
			break;
		case CHERRY_CORE_DEVICE_STATE_ACTIVE:
			QLOGD("Device status changed: CHERRY_CORE_DEVICE_STATE_ACTIVE");
			break;
		case CHERRY_CORE_DEVICE_STATE_ERROR:
			QLOGD("Device status changed: CHERRY_CORE_DEVICE_STATE_ERROR");
			break;
		}
		switch (event->data.device_status->reason) {
		case CHERRY_CORE_STATE_CHANGE_ACTIVITY:
			QLOGD("Change reason: CHERRY_CORE_STATE_CHANGE_ACTIVITY");
			break;
		case CHERRY_CORE_STATE_CHANGE_BOOT_REASON_UNKNOWN:
			QLOGD("Change reason: CHERRY_CORE_STATE_CHANGE_BOOT_REASON_UNKNOWN");
			break;
		case CHERRY_CORE_STATE_CHANGE_BOOT_REASON_FATAL:
			QLOGD("Change reason: CHERRY_CORE_STATE_CHANGE_BOOT_REASON_FATAL");
			break;
		case CHERRY_CORE_STATE_CHANGE_SOFT_RESET:
			QLOGD("Change reason: CHERRY_CORE_STATE_CHANGE_SOFT_RESET");
			app_ctx->soft_reset = true;
			break;
		}
		break;
	case CHERRY_CORE_EVENT_TYPE_ERROR:
		QLOGD("Error event %d received",
		      event->data.device_error->status_err);
		break;
	case CHERRY_CORE_EVENT_TYPE_CALIB_UPDATE:
		QLOGD("CALIB UPDATE EVENT received with status %d",
		      event->data.calib_update->status_err);
		break;
	default:
		break;
	}

	/* Free allocated event. */
	cherry_core_event_free(event);
	return;
}

int main(int argc, char *argv[])
{
	struct cherry_calib calib;
	enum cherry_err err;
	struct app_context app_ctx;
	int opt;
	static const uint32_t tx_power_index_data = 0x4E4E4E4E;
	static const struct cherry_calib_key calib_keys[] = {
		CHERRY_CALIB_UINT8("ch9.pll_locking_code", 1),
		CHERRY_CALIB_UINT8("xtal_trim", 0x20),
		CHERRY_CALIB_UINT32("ant0.ch5.ref_frame0.tx_power_index",
				    tx_power_index_data),
	};
	enum cherry_log_level log_level = CHERRY_LOG_LEVEL_WARN;
	char *endptr;
	char *help =
		"\nUsage: ./cherry-test-reload-calibration-app [-l N] [-d device]\n"
		"\nOptions:\n"
		"\t-l N\tset cherry's log level (default is 2 for warning)\n"
		"\t-d device\tset the device path (uci or tty), default is /dev/uci0\n";
	char device[32];

	strcpy(device, "/dev/uci0");

	while ((opt = getopt(argc, argv, OPTSTR)) != -1)
		switch (opt) {
		case 'l':
			log_level = (uint8_t)strtol(optarg, &endptr, 10);
			if (*endptr || log_level < CHERRY_LOG_LEVEL_NONE ||
			    log_level > CHERRY_LOG_LEVEL_DEBUG) {
				QLOGE("Invalid log level.");
				return -1;
			}
			break;
		case 'd':
			strncpy(device, optarg, sizeof(device));
			break;
		case 'h':
		default:
			QLOGD("%s", help);
			return 0;
		}

	calib.n_keys = sizeof(calib_keys) / sizeof(calib_keys[0]);
	calib.keys = calib_keys;

	app_ctx.soft_reset = false;

	app_ctx.cherry_ctx = cherry_create(device, &core_cb, &app_ctx);
	if (!app_ctx.cherry_ctx) {
		QLOGE("cherry_create failed !!!");
		return 0;
	}

	cherry_set_log_level(app_ctx.cherry_ctx, log_level,
			     CHERRY_LOG_MODULE_ALL);

	if ((err = cherry_set_calib(app_ctx.cherry_ctx, &calib)) !=
	    CHERRY_ERR_NONE)
		QLOGD("cherry_set_calib_keys failed with error %d", err);
	else {
		QLOGD("You can manually reset the QM35 during 10 seconds");
		/* Wait commands are processing. */
		qtime_msleep(10000);

		QLOGD("Soft reset in progress ...");
		/* Soft reset: calibration shall be automatically reloaded. */
		cherry_reset_device(app_ctx.cherry_ctx, 0);

		while (!app_ctx.soft_reset)
			qtime_msleep(100);
	}

	cherry_destroy_sync(app_ctx.cherry_ctx);

	return 0;
}
