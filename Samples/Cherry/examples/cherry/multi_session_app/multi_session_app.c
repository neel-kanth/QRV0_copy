/*
 * This example purpose is to launch multiple session for controller/controlee.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "multi_session_app.h"

#include "multi_session_app_params.h"

#ifdef CONFIG_CHERRY_CALIB_FOLDER
#include <cherry/cherry_calib_folder.h>
#endif
#include <cherry/cherry_fira.h>
#include <qmalloc.h>
#include <qsemaphore.h>
#include <util_calib_default.h>
#include <util_calib_qm35825.h>
#include <util_common_param.h>
#include <util_convert.h>
#include <util_dump.h>
#include <util_log.h>

/**
 * In this example we will create a cherry context and n fira sessions for a controllers (default)
 * or controlees, then we will start sessions and let them run 50 measurements before stopping it
 * and destroying sessions and context we created.
 */

/* Device ID Const */
#define MAX_SESSIONS_NR 8
#define CONTROLLER_MAC_ADDRESS 10
#define CONTROLEE_MAC_ADDRESS 11

struct session_context {
	struct cherry_fira_session *session;
	uint32_t session_id;
	uint16_t nb_measurements;
	bool error;
	bool stop;
	bool deinit;
};

struct app_context {
	struct cherry *cherry_ctx;
	struct session_context *session_ctx;
	uint16_t short_addr;
	bool is_sip;
	bool is_package_id_known;
	uint32_t device_id;
	uint8_t device_soi;
	bool error;
	struct qsemaphore *wait_sem;
	struct app_multi_session_parameter *params;
	bool has_ant_set;
	bool has_lut_set;
	const struct cherry_calib *calib;
	struct qsemaphore *calib_check_sem;
#ifdef CONFIG_CHERRY_CALIB_FOLDER
	struct cherry_calib *country_calib;
#endif
};

void core_cb(struct cherry_core_event *event, void *user_data)
{
	struct app_context *app_ctx = user_data;

	util_dump_core_event(event);

	switch (event->type) {
	case CHERRY_CORE_EVENT_TYPE_ERROR:
		break;
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

void session_cb(struct cherry_fira_event *event, void *user_data)
{
	struct session_context *session_ctx = user_data;

	util_dump_session_fira(event, session_ctx->session_id);

	switch (event->type) {
	case CHERRY_FIRA_EVENT_TYPE_SESSION_STATUS:
		switch (event->data.status->session_state) {
		case CHERRY_FIRA_SESSION_STATE_DEINIT:
			session_ctx->deinit = true;
			break;
		case CHERRY_FIRA_SESSION_STATE_IDLE:
			if (session_ctx->nb_measurements > 0)
				session_ctx->stop = true;
			break;
		default:
			break;
		}
		break;
	case CHERRY_FIRA_EVENT_TYPE_SESSION_ERROR:
		session_ctx->error = true;
		break;
	case CHERRY_FIRA_EVENT_TYPE_SESSION_TWR_RANGING_REPORT:
		session_ctx->nb_measurements++;
		break;
	default:
		break;
	}

	/* Free allocated event. */
	cherry_fira_event_free(event);
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
	struct app_multi_session_parameter *params = app_ctx->params;

	app_ctx->cherry_ctx = NULL;
	app_ctx->is_sip = false;
	app_ctx->is_package_id_known = false;
	app_ctx->device_id = 0;
	app_ctx->error = false;
	app_ctx->has_ant_set = false;
	app_ctx->has_lut_set = false;
	app_ctx->calib = NULL;
#ifdef CONFIG_CHERRY_CALIB_FOLDER
	app_ctx->country_calib = NULL;
#endif
	app_ctx->short_addr = (params->is_controller) ? CONTROLLER_MAC_ADDRESS :
							CONTROLEE_MAC_ADDRESS;

	app_ctx->session_ctx = (struct session_context *)malloc(
		params->nr_of_sessions * sizeof(struct session_context));

	if (!app_ctx->session_ctx) {
		QLOGE("sessions context alocation failed");
		return false;
	}

	for (uint8_t i = 0; i < params->nr_of_sessions; i++) {
		app_ctx->session_ctx[i].session = NULL;
		app_ctx->session_ctx[i].session_id = i + 1;
		app_ctx->session_ctx[i].nb_measurements = 0;
		app_ctx->session_ctx[i].stop = false;
		app_ctx->session_ctx[i].error = false;
		app_ctx->session_ctx[i].deinit = false;
	}

	app_ctx->cherry_ctx = cherry_create(params->device, &core_cb, app_ctx);
	if (!app_ctx->cherry_ctx) {
		QLOGE("cherry_create failed !!!");
		free(app_ctx->session_ctx);
		return false;
	}

	cherry_set_log_level(app_ctx->cherry_ctx, params->log_level,
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
	cherry_destroy_sync(app_ctx->cherry_ctx);
#ifdef CONFIG_CHERRY_CALIB_FOLDER
	if (app_ctx->params->config_path && app_ctx->country_calib) {
		cherry_calib_destroy(app_ctx->country_calib);
	}
#endif
	free(app_ctx->session_ctx);
	return false;
}

static bool create_and_start_twr_session(struct app_context *app_ctx,
					 uint8_t session_idx)
{
	enum cherry_err err;
	struct app_multi_session_parameter *params = app_ctx->params;
	const uint16_t address = CONTROLEE_MAC_ADDRESS;

	app_ctx->session_ctx[session_idx].session_id = session_idx + 1;
	if (params->is_controller) {
		app_ctx->session_ctx[session_idx].session =
			cherry_fira_session_create_twr_controller(
				app_ctx->cherry_ctx, session_cb,
				&app_ctx->session_ctx[session_idx],
				app_ctx->session_ctx[session_idx].session_id,
				params->interval_ms, &address, 1,
				app_ctx->short_addr);
		if (!app_ctx->session_ctx[session_idx].session) {
			QLOGE("cherry_fira_session_create_twr_controller failed (sesion_id = %u)",
			      app_ctx->session_ctx[session_idx].session_id);
		}
	} else {
		app_ctx->session_ctx[session_idx].session =
			cherry_fira_session_create_twr_controlee(
				app_ctx->cherry_ctx, session_cb,
				&app_ctx->session_ctx[session_idx],
				app_ctx->session_ctx[session_idx].session_id,
				params->interval_ms, CONTROLLER_MAC_ADDRESS,
				app_ctx->short_addr);
		if (!app_ctx->session_ctx[session_idx].session) {
			QLOGE("cherry_fira_session_create_twr_controlee failed (sesion_id = %u)",
			      app_ctx->session_ctx[session_idx].session_id);
		}
	}

	if (!app_ctx->session_ctx[session_idx].session)
		return false;

	err = cherry_fira_session_set_antenna(
		app_ctx->session_ctx[session_idx].session, DEFAULT_ANTENNA_SET);
	if (err) {
		QLOGE("cherry_fira_session_set_antenna failed with error %d.",
		      err);
		return false;
	}

	/**
	 * Optional call to make the session stop automatically after a number of measurements.
	 */
	err = cherry_fira_session_set_nb_measurement(
		app_ctx->session_ctx[session_idx].session,
		params->max_nb_measurements);
	if (err) {
		QLOGE("cherry_fira_session_set_nb_measurement failed with error %d (sesion_id = %u)",
		      err, app_ctx->session_ctx[session_idx].session_id);
		return false;
	}

	err = cherry_fira_session_start(
		app_ctx->session_ctx[session_idx].session);
	if (err) {
		QLOGE("cherry_fira_session_start failed with error %d (sesion_id = %u)",
		      err, app_ctx->session_ctx[session_idx].session_id);
		return false;
	}
	return true;
}

static bool do_multi_session_ranging(struct app_context *app_ctx)
{
	bool all_stopped;
	bool all_deinit;
	bool error = false;
	bool ret = true;
	struct app_multi_session_parameter *params = app_ctx->params;

	for (uint8_t i = 0; i < params->nr_of_sessions; i++) {
		if (!(ret = create_and_start_twr_session(app_ctx, i)))
			goto err_create;
	}

	/**
	 * The sessions will stop automatically as the number of measurement has been set.
	 * The client application is free to do anything else until the session stop automatically.
	 */
	do {
		qsemaphore_take(app_ctx->wait_sem, 100);
		all_stopped = true;

		for (uint8_t i = 0; i < params->nr_of_sessions; i++) {
			if (!app_ctx->session_ctx[i].stop) {
				all_stopped = false;
			}

			if (app_ctx->session_ctx[i].error) {
				error = true;
				ret = false;
				break;
			}
		}
	} while (!all_stopped && !error && !app_ctx->error);

err_create:
	for (uint8_t i = 0; i < params->nr_of_sessions; i++) {
		if (app_ctx->session_ctx[i].session)
			cherry_fira_session_destroy(
				app_ctx->session_ctx[i].session);
		else
			break;
	}

	error = false;
	do {
		qsemaphore_take(app_ctx->wait_sem, 100);
		all_deinit = true;

		for (uint8_t i = 0; i < params->nr_of_sessions; i++) {
			if (!app_ctx->session_ctx[i].deinit) {
				all_deinit = false;
			}

			if (app_ctx->session_ctx[i].error) {
				error = true;
				ret = false;
				break;
			}
		}
	} while (!all_deinit && !error && !app_ctx->error);

	if (app_ctx->error)
		ret = false;

	return ret;
}

static int do_multi_session(struct app_multi_session_parameter *params)
{
	struct app_context app_ctx;
	bool ret;

	app_ctx.params = params;
	app_ctx.wait_sem = qsemaphore_init(0, 1);
	app_ctx.calib_check_sem = qsemaphore_init(0, 1);

	if (!init_cherry(&app_ctx)) {
		qsemaphore_deinit(app_ctx.wait_sem);
		qsemaphore_deinit(app_ctx.calib_check_sem);
		return -1;
	}

	ret = do_multi_session_ranging(&app_ctx);

	cherry_destroy_sync(app_ctx.cherry_ctx);
#ifdef CONFIG_CHERRY_CALIB_FOLDER
	if (app_ctx.params->config_path != NULL) {
		cherry_calib_destroy(app_ctx.country_calib);
	}
#endif

	free(app_ctx.session_ctx);

	qsemaphore_deinit(app_ctx.wait_sem);
	qsemaphore_deinit(app_ctx.calib_check_sem);

	return (ret ? 0 : -1);
}

#ifndef CONFIG_CHERRY_EXAMPLES_ZEPHYR
int main(int argc, char *argv[])
{
	return main_multi_session_app(argc, argv);
}
#endif

static struct app_multi_session_parameter DEFAULT_MULTI_SESSION_PARAMS = {
	.max_nb_measurements = 50,
	.nr_of_sessions = 1,
	.interval_ms = 400,
	.is_controller = true,
	.device = "/dev/uci0",
	.log_level = CHERRY_LOG_LEVEL_WARN,
#ifdef CONFIG_CHERRY_CALIB_FOLDER
	.country_code = NULL,
	.config_path = NULL,
#endif
};

int main_multi_session_app(int argc, char *argv[])
{
	struct app_multi_session_parameter params =
		DEFAULT_MULTI_SESSION_PARAMS;

	if (!get_runtime_multi_session_app_param(argc, argv, &params))
		return -1;

	return do_multi_session(&params);
}
