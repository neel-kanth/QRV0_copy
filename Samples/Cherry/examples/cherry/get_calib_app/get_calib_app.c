/*
 * This example purpose is to get the calibration.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "get_calib_app.h"

#include "get_calib_app_params.h"

#ifdef CONFIG_CHERRY_CALIB_FOLDER
#include <cherry/cherry_calib_folder.h>
#endif
#include <qmalloc.h>
#include <qsemaphore.h>
#include <util_common_param.h>
#include <util_dump.h>
#include <util_log.h>

/**
 * In this example we will create a cherry context then:
 * - If '-a' parameter is specified on command line we will get all calibration's keys and dump them.
 * - Else we will set (static or from file according the parameters) calibration's keys in firmware,
 *   then we will get theses calibration's keys and dump them.
 */

struct app_context {
	struct cherry *cherry_ctx;
	bool is_sip;
	bool is_package_id_known;
	uint32_t device_id;
	uint8_t device_soi;
	struct cherry_core_event_device_info *device_info;
	struct qsemaphore *wait_sem;
	struct app_get_calib_parameter *params;
	const struct cherry_calib *calib;
#ifdef CONFIG_CHERRY_CALIB_FOLDER
	struct cherry_calib *country_calib;
#endif
};

static void core_cb(struct cherry_core_event *event, void *user_data)
{
	struct app_context *app_ctx = user_data;

	util_dump_core_event(event);

	switch (event->type) {
	case CHERRY_CORE_EVENT_TYPE_DEVICE_INFO: {
		if (event->data.device_info->status_err == CHERRY_ERR_NONE) {
			app_ctx->device_id = event->data.device_info->device_id;
			if (event->data.device_info->package_id) {
				app_ctx->is_sip = true;
			}
			app_ctx->device_soi =
				event->data.device_info->soi_variant;
			app_ctx->is_package_id_known = true;
			qsemaphore_give(app_ctx->wait_sem);
		}
		break;
	}
	case CHERRY_CORE_EVENT_TYPE_GET_CALIB: {
		if (event->data.get_calib->status_err == CHERRY_ERR_NONE)
			util_dump_calib(event->data.get_calib->calib);
	}
	default:
		break;
	}
	/* Free allocated event. */
	cherry_core_event_free(event);
	return;
}

static void close_cherry(struct app_context *app_ctx)
{
	cherry_destroy_sync(app_ctx->cherry_ctx);

#ifdef CONFIG_CHERRY_CALIB_FOLDER
	if (app_ctx->params->config_path && app_ctx->country_calib) {
		cherry_calib_destroy(app_ctx->country_calib);
	}
#endif

	return;
}

static bool init_cherry(struct app_context *app_ctx)
{
	enum cherry_err err;
	enum qerr qerr;

	app_ctx->cherry_ctx = NULL;
	app_ctx->is_sip = false;
	app_ctx->is_package_id_known = false;
	app_ctx->device_id = 0;
	app_ctx->calib = NULL;
#ifdef CONFIG_CHERRY_CALIB_FOLDER
	app_ctx->country_calib = NULL;
#endif

	app_ctx->cherry_ctx =
		cherry_create(app_ctx->params->device, &core_cb, app_ctx);
	if (!app_ctx->cherry_ctx) {
		QLOGE("cherry_create failed !!!");
		return false;
	}

	cherry_set_log_level(app_ctx->cherry_ctx, app_ctx->params->log_level,
			     CHERRY_LOG_MODULE_ALL);

	if ((err = cherry_get_device_info(app_ctx->cherry_ctx)) !=
	    CHERRY_ERR_NONE) {
		QLOGD("cherry_get_device_info failed with error %d", err);
		goto err;
	}

	if ((err = cherry_get_device_stats(app_ctx->cherry_ctx)) !=
	    CHERRY_ERR_NONE) {
		QLOGD("cherry_get_device_stats failed with error %d", err);
		goto err;
	}

	/* Wait device info. */
	if ((qerr = qsemaphore_take(app_ctx->wait_sem, 3000))) {
		QLOGD("error in qsemaphore_take : %d", qerr);
		goto err;
	}

	if (!app_ctx->is_package_id_known) {
		QLOGD("error: package id is unknown");
		goto err;
	}

	if (app_ctx->params->all_config)
		goto end;

#ifdef CONFIG_CHERRY_CALIB_FOLDER
	if (app_ctx->params->config_path != NULL) {
		QLOGD("Apply calibration from config file %s",
		      app_ctx->params->config_path);
		app_ctx->country_calib =
			cherry_calib_folder_load(app_ctx->params->config_path,
						 app_ctx->params->country_code);
		if (!app_ctx->country_calib) {
			QLOGD("Error loading country code files");
			goto err;
		}
		if ((err = cherry_set_calib(app_ctx->cherry_ctx,
					    app_ctx->country_calib)) !=
		    CHERRY_ERR_NONE) {
			QLOGD("cherry_set_calib_keys failed with error %d",
			      err);
			goto err;
		}
	} else {
#endif
		if (app_ctx->device_id == DEVICE_ID_QM357 ||
		    (app_ctx->device_id == DEVICE_ID_QM358)) {
			/* Select proper configuration. */
			enum device_chip chip = get_device_chip(
				app_ctx->device_id, app_ctx->device_soi);
			choose_calib(&app_ctx->calib, chip, app_ctx->is_sip);
			if ((err = cherry_set_calib(app_ctx->cherry_ctx,
						    app_ctx->calib)) !=
			    CHERRY_ERR_NONE) {
				QLOGD("cherry_set_calib_keys failed with error %d",
				      err);
				goto err;
			}
		}
#ifdef CONFIG_CHERRY_CALIB_FOLDER
	}
#endif

end:
	return true;
err:
	close_cherry(app_ctx);
	return false;
}

static int do_get_calib(struct app_get_calib_parameter *params)
{
	struct app_context app_ctx;
	enum cherry_err err;
	char **keys = NULL;
	uint32_t index;
	struct cherry_calib *calib;

	app_ctx.params = params;

	app_ctx.wait_sem = qsemaphore_init(0, 1);

	if (!init_cherry(&app_ctx)) {
		qsemaphore_deinit(app_ctx.wait_sem);
		return -1;
	}

	if (app_ctx.params->all_config) {
		err = cherry_get_calib(app_ctx.cherry_ctx, NULL, 0);
		if (err) {
			QLOGE("cherry_get_calib failed with error %d.", err);
		}
	} else {
#ifdef CONFIG_CHERRY_CALIB_FOLDER
		if (app_ctx.params->config_path != NULL) {
			calib = app_ctx.country_calib;
		} else {
#endif
			calib = (struct cherry_calib *)app_ctx.calib;
#ifdef CONFIG_CHERRY_CALIB_FOLDER
		}
#endif
		keys = qmalloc(calib->n_keys * sizeof(char *));
		if (keys) {
			for (index = 0; index < calib->n_keys; index++)
				keys[index] = (char *)calib->keys[index].name;
			err = cherry_get_calib(app_ctx.cherry_ctx,
					       (const char **)keys,
					       calib->n_keys);
			if (err) {
				QLOGE("cherry_get_calib failed with error %d.",
				      err);
			}
		} else
			err = CHERRY_ERR_INVALID_PARAMETER;
	}

	close_cherry(&app_ctx);

	qsemaphore_deinit(app_ctx.wait_sem);

	/*
	 * Pointer and content must remain valid for the duration of cherry_get_calib call
	 * until receiving the core event CHERRY_CORE_EVENT_GET_CALIB.
	 */
	if (keys)
		qfree(keys);

	return (err == CHERRY_ERR_NONE) ? 0 : -1;
}

#ifndef CONFIG_CHERRY_EXAMPLES_ZEPHYR
int main(int argc, char *argv[])
{
	return main_get_calib_app(argc, argv);
}
#endif

static struct app_get_calib_parameter DEFAULT_GET_CALIB_PARAMS = {
	.device = "/dev/uci0",
	.log_level = CHERRY_LOG_LEVEL_WARN,
	.all_config = false,
#ifdef CONFIG_CHERRY_CALIB_FOLDER
	.country_code = NULL,
	.config_path = NULL,
#endif
};

int main_get_calib_app(int argc, char *argv[])
{
	struct app_get_calib_parameter params = DEFAULT_GET_CALIB_PARAMS;

	if (!get_runtime_get_calib_app_param(argc, argv, &params))
		return -1;

	return do_get_calib(&params);
}
