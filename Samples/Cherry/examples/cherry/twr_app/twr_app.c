/*
 * This example purpose is to launch a fira session for a controller
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "twr_app.h"

#include "twr_app_params.h"

#include <stdint.h>

#ifdef CONFIG_CHERRY_CALIB_FOLDER
#include <cherry/cherry_calib_folder.h>
#endif
#include <cherry/cherry_fira.h>
#include <qmalloc.h>
#include <qsemaphore.h>
#include <util_calib_360_aoa.h>
#include <util_calib_default.h>
#include <util_calib_qm35825.h>
#include <util_common_param.h>
#include <util_dump.h>
#include <util_log.h>

/**
 * In this example we will create a cherry context and a fira session for a controller (default) or a controlee.
 * Then we will start the session and let it run 50 measurements (by default) before stopping it
 * and destroying the session and context we created.
 */

/* Device ID Const */

struct app_context {
	struct cherry *cherry_ctx;
	bool is_sip;
	bool is_package_id_known;
	uint32_t device_id;
	uint8_t device_soi;
	bool error;
	bool stop;
	bool deinit;
	uint16_t nb_measurements;
	struct qsemaphore *wait_sem;
	struct app_twr_parameter *params;
	bool has_ant_set;
	bool has_lut_set;
	const struct cherry_calib *calib;
	struct qsemaphore *calib_check_sem;
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

static void fira_cb(struct cherry_fira_event *event, void *user_data)
{
	struct app_context *app_ctx = user_data;

	util_dump_session_fira(event, app_ctx->params->session_id);

	switch (event->type) {
	case CHERRY_FIRA_EVENT_TYPE_SESSION_STATUS:
		switch (event->data.status->session_state) {
		case CHERRY_FIRA_SESSION_STATE_DEINIT:
			app_ctx->deinit = true;
			break;
		case CHERRY_FIRA_SESSION_STATE_IDLE:
			if (app_ctx->nb_measurements > 0)
				app_ctx->stop = true;
			break;
		default:
			break;
		}
		break;
	case CHERRY_FIRA_EVENT_TYPE_SESSION_ERROR:
		app_ctx->error = true;
		break;
	case CHERRY_FIRA_EVENT_TYPE_SESSION_TWR_RANGING_REPORT:
		app_ctx->nb_measurements++;
		break;
	default:
		break;
	}

	/* Free allocated event. */
	cherry_fira_event_free(event);
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

	app_ctx->cherry_ctx = NULL;
	app_ctx->is_sip = false;
	app_ctx->is_package_id_known = false;
	app_ctx->device_id = 0;
	app_ctx->has_ant_set = false;
	app_ctx->has_lut_set = false;
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
	if (app_ctx->params->aoa_360_enabled) {
		app_ctx->calib = &util_calib_360_aoa;
		app_ctx->params->ant_set_id = 1;
	} else if (!check_calib_loaded(app_ctx)) {
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

static bool do_ranging(struct app_context *app_ctx)
{
	struct cherry_fira_session *session;
	enum cherry_err err;
	struct app_twr_parameter *params = app_ctx->params;

	app_ctx->stop = false;
	app_ctx->deinit = false;
	app_ctx->error = false;
	app_ctx->nb_measurements = 0;

	QLOGD("Ranging on %lu measurements with %lu ms intervals between each sequence "
	      " and with session priority %u and result report config %X as %s on address %u",
	      params->max_nb_measurements, params->interval_ms,
	      params->session_priority, params->result_report_config,
	      params->is_controller ? "controller" : "controlee",
	      params->is_controller ? CONTROLLER_MAC_ADDRESS :
				      params->short_addr);

	if (params->is_controller) {
		params->short_addr = CONTROLLER_MAC_ADDRESS;
		session = cherry_fira_session_create_twr_controller(
			app_ctx->cherry_ctx, fira_cb, app_ctx,
			params->session_id, params->interval_ms,
			controlee_addresses, 2, params->short_addr);
		if (!session) {
			QLOGE("cherry_fira_session_create_twr_controller failed.");
		}
	} else {
		session = cherry_fira_session_create_twr_controlee(
			app_ctx->cherry_ctx, fira_cb, app_ctx,
			params->session_id, params->interval_ms,
			CONTROLLER_MAC_ADDRESS, params->short_addr);
		if (!session) {
			QLOGE("cherry_fira_session_create_twr_controlee failed.");
		}
	}

	if (!session)
		return false;

	if (params->diagnostic_enabled) {
		struct cherry_common_diag_cfg config = {
			.aoa = true,
			.extra_status = true,
			.cirs = true,
			.emitter_addr = true,
			.cfo = true,
			.seg_metrics = true,
		};

		err = cherry_fira_session_set_diagnostics(session, config);
		if (err) {
			QLOGE("cherry_fira_session_set_diagnostics failed with error %d.",
			      err);
			goto err;
		}
	}

	/**
	 * Optional call to make the session stop automatically after a number of measurements
	 */
	err = cherry_fira_session_set_nb_measurement(
		session, params->max_nb_measurements);
	if (err) {
		QLOGE("cherry_fira_session_set_nb_measurement failed with error %d.",
		      err);
		goto err;
	}
	// Preamble code index
	err = cherry_fira_session_set_preamble_code_index(
		session, params->preamble_code_index);
	if (err) {
		QLOGE("cherry_fira_session_set_preamble_code_index failed with error %d.",
		      err);
		goto err;
	}

	//Enable/Disable RSSI report
	if (params->report_rssi == 1) {
		err = cherry_fira_session_set_report_rssi(session, 1);
		if (err) {
			QLOGE("cherry_fira_session_set_report_rssi failed with error %d.",
			      err);
			goto err;
		}
	}

	//Session priority
	err = cherry_fira_session_set_priority(session,
					       params->session_priority);
	if (err) {
		QLOGE("cherry_fira_session_set_priority failed with error %d.",
		      err);
		goto err;
	}

	// Antenna set.
	err = cherry_fira_session_set_antenna(session, params->ant_set_id);
	if (err) {
		QLOGE("cherry_fira_session_set_antenna failed with error %d.",
		      err);
		goto err;
	}

	//Max rr retry
	err = cherry_fira_session_set_rr_retry(session, params->max_rr_retry);
	if (err) {
		QLOGE("cherry_fira_session_set_rr_retry failed with error %d.",
		      err);
		goto err;
	}

	// Result report config
	err = cherry_fira_session_set_result_report_config(
		session, params->result_report_config);
	if (err) {
		QLOGE("cherry_fira_session_set_result_report_config failed with error %d.",
		      err);
		goto err;
	}

	// PHY parameters
	if (params->phy_params) {
		err = cherry_fira_session_set_phy(session, params->phy_params);
		if (err) {
			QLOGE("cherry_fira_session_set_phy failed with error %d.",
			      err);
			goto err;
		}
	}

	// Static STS
	if (params->sts_config == 1) {
		QLOGD("Set static STS !");

		err = cherry_fira_session_set_static_sts(session,
							 static_sts_iv);
		if (err) {
			QLOGE("cherry_fira_session_set_static_sts failed with error %d.",
			      err);
			goto err;
		}

		err = cherry_fira_session_set_vendor_id(session, vendor_id);
		if (err) {
			QLOGE("cherry_fira_session_set_vendor_id failed with error %d.",
			      err);
			goto err;
		}
	}
	// Provisioned STS
	else if (params->sts_config == 3) {
		uint8_t sts_key_len = 16;
		uint8_t sts_key[16] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
					0x07, 0x08, 0x09, 0x10, 0x11, 0x12,
					0x13, 0x14, 0x15, 0x16 };
		uint8_t key_rotation_rate = 0;

		QLOGD("Set prov STS !");

		err = cherry_fira_session_set_prov_sts(
			session, sts_key_len, sts_key, key_rotation_rate);
		if (err) {
			QLOGE("cherry_fira_session_set_prov_sts failed with error %d.",
			      err);
			goto err;
		}
	}

	err = cherry_fira_session_start(session);
	if (err) {
		QLOGE("cherry_fira_session_start failed with error %d.", err);
		goto err;
	}

	/**
	 * The sessions will stop automatically as the number of measurement has been set.
	 * The client application is free to do anything else until the session stop automatically
	 */
	while (!app_ctx->stop && !app_ctx->error)
		qsemaphore_take(app_ctx->wait_sem, 100);

err:
	cherry_fira_session_destroy(session);

	while (!app_ctx->deinit && !app_ctx->error)
		qsemaphore_take(app_ctx->wait_sem, 100);

	if (params->aoa_360_enabled) {
		/**
		 * In this case we have loaded a specific configuration for the AoA measurements.
		 * Now we can reset the chip to remove the configuration.
		 */
		cherry_set_calib(app_ctx->cherry_ctx, NULL);
		if ((err = cherry_reset_device(app_ctx->cherry_ctx, true)) !=
		    CHERRY_ERR_NONE) {
			QLOGE("cherry_reset failed with error %d", err);
			goto err;
		}
	}

	return (err == CHERRY_ERR_NONE) ? true : false;
}

static int do_twr(struct app_twr_parameter *params)
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

	ret = do_ranging(&app_ctx);

	close_cherry(&app_ctx);

	qsemaphore_deinit(app_ctx.wait_sem);
	qsemaphore_deinit(app_ctx.calib_check_sem);

	return (ret ? 0 : -1);
}

#ifndef CONFIG_CHERRY_EXAMPLES_ZEPHYR
int main(int argc, char *argv[])
{
	return main_twr_app(argc, argv);
}
#endif

static struct app_twr_parameter DEFAULT_TWR_PARAMS = {
	.is_controller = true,
	.diagnostic_enabled = false,
	.aoa_360_enabled = false,
	.max_nb_measurements = 50,
	.interval_ms = 200,
	.preamble_code_index = 10,
	.report_rssi = 0,
	.session_priority = 50,
	.ant_set_id = DEFAULT_ANTENNA_SET,
	.max_rr_retry = 0,
	.result_report_config = 1,
	.sts_config = 1,
	.short_addr = CONTROLEE_1_MAC_ADDRESS,
	.phy_params = NULL,
	.device = "/dev/uci0",
	.log_level = CHERRY_LOG_LEVEL_WARN,
	.session_id = 1,
#ifdef CONFIG_CHERRY_CALIB_FOLDER
	.country_code = NULL,
	.config_path = NULL,
#endif
};

int main_twr_app(int argc, char *argv[])
{
	struct app_twr_parameter params = DEFAULT_TWR_PARAMS;

	if (!get_runtime_twr_app_param(argc, argv, &params))
		return -1;

	return do_twr(&params);
}
