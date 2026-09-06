/*
 * This example purpose is to launch dltdoa Tag/Anchor session
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */
#include "dl_tdoa_app.h"

#include "dl_tdoa_app_params.h"

#ifdef CONFIG_CHERRY_CALIB_FOLDER
#include <cherry/cherry_calib_folder.h>
#endif

#include <cherry/cherry_fira.h>
#include <qmalloc.h>
#include <qsemaphore.h>
#include <util_calib_default.h>
#include <util_calib_qm35825.h>
#include <util_common_param.h>
#include <util_dump.h>
#include <util_log.h>

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
	struct app_dltdoa_parameter *params;
	bool has_ant_set;
	bool has_lut_set;
	const struct cherry_calib *calib;
	struct qsemaphore *calib_check_sem;
#ifdef CONFIG_CHERRY_CALIB_FOLDER
	struct cherry_calib *country_calib;
#endif
};

static void core_cb_dl_tdoa(struct cherry_core_event *event, void *user_data)
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

void fira_cb(struct cherry_fira_event *event, void *user_data)
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
	case CHERRY_FIRA_EVENT_TYPE_SESSION_DT_TAG_RANGING_REPORT: {
		app_ctx->nb_measurements++;
		break;
	}
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

	app_ctx->cherry_ctx = cherry_create(app_ctx->params->device,
					    &core_cb_dl_tdoa, app_ctx);
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
	cherry_destroy_sync(app_ctx->cherry_ctx);
#ifdef CONFIG_CHERRY_CALIB_FOLDER
	if (app_ctx->params->config_path && app_ctx->country_calib) {
		cherry_calib_destroy(app_ctx->country_calib);
	}
#endif
	return false;
}

static bool do_dl_tdoa(struct app_context *app_ctx)
{
	struct cherry_fira_session *session;
	enum cherry_err err;
	struct app_dltdoa_parameter *params = app_ctx->params;
	const struct cherry_fira_anchor_round_config *round_configuration;

	app_ctx->stop = false;
	app_ctx->deinit = false;
	app_ctx->error = false;
	app_ctx->nb_measurements = 0;

	QLOGD("Create and start DL-Tdoa session.");

	if (params->config_choice) {
		round_configuration = params->light_conf.round_config;
		if (params->interval_ms < params->slots_per_rr * 10) {
			params->interval_ms = params->slots_per_rr * 10;
			QLOGW("Needs at least 10 slots to run default configuration, changing interval to %dms",
			      params->interval_ms);
		}
	} else
		round_configuration = params->light_conf.round_config_custom;

	if (params->is_anchor) {
		struct cherry_fira_anchor_location location = {
			.type = CHERRY_FIRA_ANCHOR_LOCATION_TYPE_NONE,
		};
		util_dump_anchor_parameters(round_configuration,
					    params->light_conf.n_rounds);
		session = cherry_fira_session_create_dt_anchor(
			app_ctx->cherry_ctx, fira_cb, app_ctx,
			params->session_id, params->interval_ms,
			params->slot_duration,
			params->light_conf.device_mac_address,
			params->light_conf.n_rounds, round_configuration,
			&location, params->is_time_ref);
		if (!session) {
			QLOGE("cherry_fira_session_create_dt_anchor failed (session_id = %u)",
			      params->session_id);
			return false;
		}
	} else {
		QLOGD("Creating DT tag");

		session = cherry_fira_session_create_dt_tag(
			app_ctx->cherry_ctx, fira_cb, app_ctx,
			params->session_id, params->interval_ms,
			params->slot_duration, params->block_skipping,
			params->light_conf.n_rounds, params->index_tag);
		if (!session) {
			QLOGE("cherry_fira_session_create_dt_tag failed (session_id = %u)",
			      params->session_id);
			return false;
		}
	}

	QLOGD("Set number of measurements as %d !",
	      params->max_nb_measurements);
	err = cherry_fira_session_set_nb_measurement(
		session, params->max_nb_measurements);
	if (err) {
		QLOGE("cherry_fira_session_set_nb_measurement failed with error %d.",
		      err);
		goto err;
	}

	/* Enable/Disable RSSI report */
	if (params->report_rssi == 1) {
		QLOGD("Enable RSSI report !");
		err = cherry_fira_session_set_report_rssi(session, 1);
		if (err) {
			QLOGE("cherry_fira_session_set_report_rssi failed with error %d.",
			      err);
			goto err;
		}
	}

	err = cherry_fira_session_set_antenna(session, DEFAULT_ANTENNA_SET);
	if (err) {
		QLOGE("cherry_fira_session_set_antenna failed with error %d.",
		      err);
		goto err;
	}

	/* Static STS. */
	QLOGD("Set static STS !");
	err = cherry_fira_session_set_static_sts(session, static_sts_iv);
	if (err) {
		QLOGE("cherry_fira_session_set_static_sts failed with error %d.",
		      err);
		goto err;
	}

	QLOGD("Set vendor !");
	err = cherry_fira_session_set_vendor_id(session, vendor_id);
	if (err) {
		QLOGE("cherry_fira_session_set_vendor_id failed with error %d.",
		      err);
		goto err;
	}

	if (params->phy_params) {
		QLOGD("Set phy params!");
		err = cherry_fira_session_set_phy(session, params->phy_params);
		if (err) {
			QLOGE("cherry_fira_session_set_phy failed with error %d (session_id = %u)",
			      err, params->session_id);
			goto err;
		}
	}

	QLOGD("Set preamble code !");
	err = cherry_fira_session_set_preamble_code_index(
		session, params->preamble_code_index);
	if (err) {
		QLOGE("cherry_fira_session_set_preamble_code_index failed with error %d (session_id = %u)",
		      err, params->session_id);
		goto err;
	}

	if (params->slots_per_rr) {
		QLOGD("Set slots per ranging round !");
		err = cherry_fira_session_set_slots_per_ranging_round(
			session, params->slots_per_rr);
		if (err) {
			QLOGE("cherry_fira_session_set_slots_per_ranging_round failed with error %d (session_id = %u)",
			      err, params->session_id);
			goto err;
		}
	}

	if (params->tx_active_rr) {
		QLOGD("Set tx active ranging round !");
		err = cherry_fira_session_set_dltdoa_tx_active_rr(session, 1);
		if (err) {
			QLOGE("cherry_fira_session_set_dltdoa_tx_active_rr failed with error %d (session_id = %u)",
			      err, params->session_id);
		}
	}

	QLOGD("Start session !");
	err = cherry_fira_session_start(session);
	if (err) {
		QLOGE("cherry_fira_session_start failed with error %d (session_id = %u)",
		      err, params->session_id);
		goto err;
	}
	QLOGD("Session started");

	/**
	 * The sessions will stop automatically as the number of measurement has been set.
	 * The client application is free to do anything else until the session stop automatically.
	 */
	while (!app_ctx->stop && !app_ctx->error)
		qsemaphore_take(app_ctx->wait_sem, 100);

err:
	cherry_fira_session_destroy(session);

	while (!app_ctx->deinit && !app_ctx->error)
		qsemaphore_take(app_ctx->wait_sem, 100);

	return (err == CHERRY_ERR_NONE) ? true : false;
}

static int do_dt_app(struct app_dltdoa_parameter *params)
{
	struct app_context app_ctx;
	bool ret;

	app_ctx.params = params;

	app_ctx.wait_sem = qsemaphore_init(0, 1);
	app_ctx.calib_check_sem = qsemaphore_init(0, 1);

	if (!init_cherry(&app_ctx)) {
		ret = false;
		goto cleanup;
	}

	ret = do_dl_tdoa(&app_ctx);

	cherry_destroy_sync(app_ctx.cherry_ctx);
#ifdef CONFIG_CHERRY_CALIB_FOLDER
	if (app_ctx.params->config_path != NULL) {
		cherry_calib_destroy(app_ctx.country_calib);
	}
#endif

cleanup:
	qsemaphore_deinit(app_ctx.wait_sem);
	qsemaphore_deinit(app_ctx.calib_check_sem);

	return (ret ? 0 : -1);
}

#ifndef CONFIG_CHERRY_EXAMPLES_ZEPHYR
int main(int argc, char *argv[])
{
	return main_dl_tdoa_app(argc, argv);
}
#endif

static struct app_dltdoa_parameter DEFAULT_DL_TDOA_PARAMS = {
	.block_skipping = 0,
	.config_choice = 0,
	.device = "/dev/uci0",
	.dst_address.addresses[0] = 0,
	.dst_address.n_addresses = 0,
	.index_tag[0] = 0,
	.interval_ms = 200,
	.is_anchor = true,
	.is_multi_cluster = false,
	.is_time_ref = false,
	.light_conf.device_mac_address = ANCHOR_MAC_ADDRESS_2,
	.light_conf.n_rounds = 0,
	.light_conf.round_config = NULL,
	.light_conf.round_config_custom = NULL,
	.log_level = CHERRY_LOG_LEVEL_WARN,
	.max_nb_measurements = 50,
	.preamble_code_index = 10,
	.phy_params = NULL,
	.report_rssi = 1,
	.session_id = 1,
	.slot_duration = 1200,
	.slots_per_rr = 25,
	.sts_config = 1,
	.tx_active_rr = false,
#ifdef CONFIG_CHERRY_CALIB_FOLDER
	.country_code = NULL,
	.config_path = NULL,
#endif
};

int main_dl_tdoa_app(int argc, char *argv[])
{
	struct app_dltdoa_parameter params = DEFAULT_DL_TDOA_PARAMS;
	int ret;

	if (!get_runtime_dltdoa_app_param(argc, argv, &params))
		return -1;

	ret = do_dt_app(&params);
	free_runtime_dltdoa_app_param(&params);
	return ret;
}
