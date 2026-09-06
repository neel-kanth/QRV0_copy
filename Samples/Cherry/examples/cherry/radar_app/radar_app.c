/*
 * This example purpose is to launch a radar session.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "radar_app.h"

#include "radar_app_params.h"

#ifdef CONFIG_CHERRY_CALIB_FOLDER
#include <cherry/cherry_calib_folder.h>
#endif
#include <cherry/cherry_radar.h>
#include <qerr.h>
#include <qmalloc.h>
#include <qsemaphore.h>
#include <util_calib_default.h>
#include <util_calib_qm35825.h>
#include <util_common_param.h>
#include <util_dump.h>
#include <util_log.h>

/**
 * In this example we will create a cherry context and a radar session.
 * Then we will start the session and let it run 100 bursts (by default) before stopping it
 * and destroying the session and context we created.
 */

struct app_context {
	struct cherry *cherry_ctx;
	bool is_sip;
	uint32_t session_id;
	bool is_package_id_known;
	uint32_t device_id;
	uint8_t device_soi;
	uint8_t antenna_id;
	bool error;
	bool end;
	bool deinit;
	struct qsemaphore *wait_sem;
	struct app_radar_parameter *params;
	bool has_ant_set;
	bool has_lut_set;
	const struct cherry_calib *calib;
	struct qsemaphore *calib_check_sem;
#ifdef CONFIG_CHERRY_CALIB_FOLDER
	struct cherry_calib *country_calib;
#endif
	char *samples_buffer;
	bool first_frame;
};

static void core_cb_radar(struct cherry_core_event *event, void *user_data)
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
	case CHERRY_CORE_EVENT_TYPE_ERROR:
		app_ctx->error = true;
		break;
	case CHERRY_CORE_EVENT_TYPE_GET_CALIB: {
		struct cherry_calib *calib = event->data.get_calib->calib;
		if (check_calib(calib, &app_ctx->has_ant_set,
				&app_ctx->has_lut_set))
			qsemaphore_give(app_ctx->calib_check_sem);
		break;
	}
	default:
		break;
	}

	/* Free allocated event. */
	cherry_core_event_free(event);
	return;
}

void radar_cb(struct cherry_radar_event *event, void *user_data)
{
	struct app_context *app_ctx = user_data;

	util_dump_session_radar(event, app_ctx->params->file,
				app_ctx->samples_buffer, app_ctx->first_frame);

	switch (event->type) {
	case CHERRY_RADAR_EVENT_TYPE_SESSION_STATUS:
		switch (event->data.status->session_state) {
		case CHERRY_RADAR_SESSION_STATE_DEINIT:
			app_ctx->deinit = true;
			break;
		case CHERRY_RADAR_SESSION_STATE_IDLE:
			if (event->data.status->reason_code ==
			    CHERRY_RADAR_STATE_CHANGE_REASON_MAX_MEASUREMENT)
				app_ctx->end = true;
			break;
		default:
			break;
		}
		break;
	case CHERRY_RADAR_EVENT_TYPE_SESSION_ERROR:
		app_ctx->error = true;
		break;
	case CHERRY_RADAR_EVENT_TYPE_SESSION_REPORT:
		app_ctx->first_frame = false;
		break;
	default:
		break;
	}

	/* Free allocated event. */
	cherry_radar_event_free(event);
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

static bool check_calib_loaded(struct app_context *app_ctx)
{
	enum cherry_err err;
	char **keys = qmalloc(util_calib_default.n_keys * sizeof(char *));
	if (keys) {
		for (uint32_t index = 0; index < util_calib_default.n_keys;
		     index++)
			keys[index] =
				(char *)util_calib_default.keys[index].name;
		err = cherry_get_calib(app_ctx->cherry_ctx, (const char **)keys,
				       util_calib_default.n_keys);
		if (err) {
			QLOGE("cherry_get_calib failed with error %d.", err);
			qfree(keys);
			return false;
		}
	} else {
		QLOGE("Unable to allocate memory for keys.");
		return false;
	}
	if (qsemaphore_take(app_ctx->calib_check_sem, 1000)) {
		qfree(keys);
		return false;
	}
	qfree(keys);
	if ((app_ctx->device_id == DEVICE_ID_QM357 ||
	     (app_ctx->device_id == DEVICE_ID_QM358))) {
		/* Select proper configuration. */
		enum device_chip chip = get_device_chip(app_ctx->device_id,
							app_ctx->device_soi);
		if (chip != QM35825) {
			choose_calib(&app_ctx->calib, chip, app_ctx->is_sip);
		} else {
			if (!app_ctx->has_ant_set) {
				QLOGD("Apply full calibration ...");
				app_ctx->calib = &util_calib_qm35825;
			} else if (!app_ctx->has_lut_set) {
				QLOGD("Apply partial calibration ...");
				app_ctx->calib = &util_partial_calib_qm35825;
			}
		}
	}
	return true;
}

static bool init_cherry(struct app_context *app_ctx)
{
	enum cherry_err err;
	enum qerr qerr;

#ifdef CONFIG_CHERRY_CALIB_FOLDER
	app_ctx->country_calib = NULL;
#endif
	app_ctx->cherry_ctx = NULL;
	app_ctx->is_package_id_known = false;
	app_ctx->has_ant_set = false;
	app_ctx->has_lut_set = false;
	app_ctx->calib = NULL;
	app_ctx->is_sip = false;

	app_ctx->cherry_ctx =
		cherry_create(app_ctx->params->device, &core_cb_radar, app_ctx);
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

	if (!check_calib_loaded(app_ctx)) {
		QLOGD("error: calibration check failed");
		goto err;
	}
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
		if (app_ctx->calib) {
			err = cherry_set_calib(app_ctx->cherry_ctx,
					       app_ctx->calib);
			if (err != CHERRY_ERR_NONE) {
				QLOGD("cherry_set_calib_keys failed with error %d",
				      err);
				goto err;
			}
		} else {
			QLOGD("Calibration already loaded.");
		}

#ifdef CONFIG_CHERRY_CALIB_FOLDER
	}
#endif

	return true;
err:
	close_cherry(app_ctx);
	return false;
}

static bool do_radar_bursts(struct app_context *app_ctx)
{
	struct cherry_radar_session *session;
	enum cherry_err err;
	struct app_radar_parameter *params = app_ctx->params;

	app_ctx->session_id = 1;
	app_ctx->antenna_id = RADAR_ANTENNA_SET;
	app_ctx->end = false;
	app_ctx->deinit = false;
	app_ctx->error = false;

	session = cherry_radar_session_create(
		app_ctx->cherry_ctx, radar_cb, app_ctx, app_ctx->session_id,
		params->burst_period_ms, params->sweep_period_rstu,
		params->sweeps_per_burst, params->samples_per_sweep,
		app_ctx->antenna_id);
	if (!session) {
		QLOGE("cherry_radar_session_create failed.");
		return false;
	}

	err = cherry_radar_session_set_number_of_bursts(
		session, params->number_of_bursts);
	if (err) {
		QLOGE("cherry_radar_session_set_number_of_bursts failed with error %d.",
		      err);
		goto err;
	}

	err = cherry_radar_session_set_sweep_offset(session,
						    params->sweep_offset);
	if (err) {
		QLOGE("cherry_radar_session_set_sweep_offset failed with error %d.",
		      err);
		goto err;
	}

	err = cherry_radar_session_set_tx_profile_idx(session,
						      params->tx_profile_idx);
	if (err) {
		QLOGE("cherry_radar_session_set_tx_profile_idx failed with error %d.",
		      err);
		goto err;
	}

	err = cherry_radar_session_start(session);
	if (err) {
		QLOGE("cherry_radar_session_start failed with error %d.", err);
		goto err;
	}

	/**
	 * Radar sessions will stop automatically as the number of bursts has been set.
	 * The client application is free to do anything else until the session stop automatically
	 */
	if (params->number_of_bursts) {
		while (!app_ctx->error && !app_ctx->end)
			qsemaphore_take(app_ctx->wait_sem, 100);
	} else {
		/* Infinite loop. */
		getchar();

		err = cherry_radar_session_stop(session);
		if (err) {
			QLOGE("cherry_radar_session_stop failed with error %d.",
			      err);
			goto err;
		}
	}

err:
	cherry_radar_session_destroy(session);

	while (!app_ctx->deinit && !app_ctx->error)
		qsemaphore_take(app_ctx->wait_sem, 100);

	return (err == CHERRY_ERR_NONE) ? true : false;
}

static int do_radar(struct app_radar_parameter *params)
{
	struct app_context app_ctx;
	bool ret;

	app_ctx.params = params;
	app_ctx.wait_sem = qsemaphore_init(0, 1);
	app_ctx.calib_check_sem = qsemaphore_init(0, 1);
	app_ctx.first_frame = true;

	app_ctx.samples_buffer =
		qmalloc(MAX_SAMPLES_PER_CIR * MAX_SAMPLE_LEN + 4 + 1);
	if (!app_ctx.samples_buffer) {
		QLOGE("failed to allocate samples buffer");
		return QERR_ENOMEM;
	}

	if (!init_cherry(&app_ctx)) {
		qsemaphore_deinit(app_ctx.wait_sem);
		qsemaphore_deinit(app_ctx.calib_check_sem);
		return -1;
	}

	ret = do_radar_bursts(&app_ctx);

	close_cherry(&app_ctx);

	qfree(app_ctx.samples_buffer);

	qsemaphore_deinit(app_ctx.wait_sem);
	qsemaphore_deinit(app_ctx.calib_check_sem);

	return (ret ? 0 : -1);
}

#ifndef CONFIG_CHERRY_EXAMPLES_ZEPHYR
int main(int argc, char *argv[])
{
	return main_radar_app(argc, argv);
}
#endif

static struct app_radar_parameter DEFAULT_RADAR_PARAMS = {
	.burst_period_ms = 100,
	.sweep_period_rstu = 1200,
	.number_of_bursts = 100,
	.sweeps_per_burst = 1,
	.samples_per_sweep = 64,
	.sweep_offset = -10,
	.tx_profile_idx = 0,
	.file = NULL,
	.device = "/dev/uci0",
	.log_level = CHERRY_LOG_LEVEL_WARN,
#ifdef CONFIG_CHERRY_CALIB_FOLDER
	.country_code = NULL,
	.config_path = NULL,
#endif
};

int main_radar_app(int argc, char *argv[])
{
	struct app_radar_parameter params = DEFAULT_RADAR_PARAMS;

	if (!get_runtime_radar_app_param(argc, argv, &params))
		return -1;

	return do_radar(&params);
}
