/*
 * Example for Cherry API get device info implementation
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "device_app.h"

#include "device_app_params.h"

#include <cherry/cherry.h>
#include <util_dump.h>
#include <util_log.h>

static void core_cb_device(struct cherry_core_event *event, void *user_data)
{
	util_dump_core_event(event);

	/* Free allocated event. */
	cherry_core_event_free(event);
	return;
}

static int do_device(struct app_device_parameter *params)
{
	enum cherry_err err;
	struct cherry *cherry_ctx;

	cherry_ctx = cherry_create(params->device, &core_cb_device, NULL);
	if (!cherry_ctx) {
		QLOGE("cherry_create failed !!!");
		return -1;
	}

	if (params->reset_soft) {
		if ((err = cherry_reset_device(cherry_ctx, false)) !=
		    CHERRY_ERR_NONE) {
			QLOGE("cherry_reset failed with error %d", err);
			goto err;
		}
	}

	if (params->reset_hard) {
		if ((err = cherry_reset_device(cherry_ctx, true)) !=
		    CHERRY_ERR_NONE) {
			QLOGE("cherry_reset failed with error %d", err);
			goto err;
		}
	}

	cherry_set_log_level(cherry_ctx, params->log_level,
			     CHERRY_LOG_MODULE_ALL);

	if ((err = cherry_get_device_info(cherry_ctx)) != CHERRY_ERR_NONE) {
		QLOGE("cherry_get_device_info failed with error %d", err);
		goto err;
	}

	if ((err = cherry_get_device_stats(cherry_ctx)) != CHERRY_ERR_NONE) {
		QLOGE("cherry_get_device_stats failed with error %d", err);
		goto err;
	}

err:
	cherry_destroy_sync(cherry_ctx);
	return (err == CHERRY_ERR_NONE) ? 0 : -1;
}

#ifndef CONFIG_CHERRY_EXAMPLES_ZEPHYR
int main(int argc, char *argv[])
{
	return main_device_app(argc, argv);
}
#endif

static struct app_device_parameter DEFAULT_DEVICE_PARAMS = {
	.reset_soft = false,
	.reset_hard = false,
	.device = "/dev/uci0",
	.log_level = CHERRY_LOG_LEVEL_WARN,
};

int main_device_app(int argc, char *argv[])
{
	struct app_device_parameter params = DEFAULT_DEVICE_PARAMS;

	if (!get_runtime_device_app_param(argc, argv, &params))
		return -1;

	return do_device(&params);
}
