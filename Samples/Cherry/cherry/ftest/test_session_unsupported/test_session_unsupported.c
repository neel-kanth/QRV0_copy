/*
 * This example allows to test error code on unsupported Radar session type.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include <cherry/cherry_fira.h>
#include <cherry/cherry_radar.h>
#include <getopt.h>
#include <qlog.h>
#include <qsemaphore.h>
#include <qtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OPTSTR "hl:d:"

extern char *optarg;

/* Default session config */
#define CONTROLLER_MAC_ADDRESS 10
#define CONTROLEE_MAC_ADDRESS 11
#define FIRA_SESSION_ID 1
#define FIRA_SESSION_NB_MEASUREMENTS 30
#define RANGING_INTERVAL 200
#define RADAR_SESSION_ID 10

struct app_context {
	enum cherry_core_device_state device_state;
	struct qsemaphore *device_state_sem;
	enum cherry_core_state_change_reason state_reason;
};

void app_init(struct app_context *app)
{
	app->device_state = CHERRY_CORE_DEVICE_STATE_ERROR;
	app->device_state_sem = qsemaphore_init(0, 1);
	app->state_reason = CHERRY_CORE_STATE_CHANGE_BOOT_REASON_UNKNOWN;
}

void app_deinit(struct app_context *app)
{
	qsemaphore_deinit(app->device_state_sem);
}

bool app_wait_device_state_ready(struct app_context *app_ctx,
				 const int timeout_ms)
{
	enum qerr err = QERR_SUCCESS;
	int retry = 3;

	while (app_ctx->device_state != CHERRY_CORE_DEVICE_STATE_READY &&
	       retry--) {
		err = qsemaphore_take(app_ctx->device_state_sem, timeout_ms);
	}
	return err == QERR_SUCCESS;
}

struct fira_session_context {
	struct cherry_fira_session *session;
	enum cherry_fira_session_state session_state;
	unsigned nb_session_state_event;
	uint32_t session_id;
	bool error;
	bool stop;
	bool deinit;
	uint16_t nb_measurments;
};

struct radar_session_context {
	struct cherry_radar_session *session;
	enum cherry_radar_session_state session_state;
	unsigned nb_session_state_event;
	uint32_t session_id;
	bool error;
	bool deinit;
	bool not_supported;
};

static const char *device_state_str(enum cherry_core_device_state state)
{
	switch (state) {
	case CHERRY_CORE_DEVICE_STATE_READY:
		return "READY";
	case CHERRY_CORE_DEVICE_STATE_ACTIVE:
		return "ACTIVE";
	case CHERRY_CORE_DEVICE_STATE_ERROR:
		return "ERROR";
	}

	return "UNDEFINED";
}

static const char *
device_boot_reason(enum cherry_core_state_change_reason reason)
{
	switch (reason) {
	case CHERRY_CORE_STATE_CHANGE_ACTIVITY:
		return "UWBS changes ready/active";
	case CHERRY_CORE_STATE_CHANGE_BOOT_REASON_UNKNOWN:
		return "UWBS boot for unknow reason";
	case CHERRY_CORE_STATE_CHANGE_BOOT_REASON_FATAL:
		return "UWBS reset after a fatal error";
	case CHERRY_CORE_STATE_CHANGE_SOFT_RESET:
		return "UWBS received soft reset command";
	}
	return "Unknown";
}

void core_cb(struct cherry_core_event *event, void *user_data)
{
	struct app_context *app_ctx = user_data;

	switch (event->type) {
	case CHERRY_CORE_EVENT_TYPE_DEVICE_STATUS: {
		QLOGD("Device status changed to %s because %s",
		      device_state_str(event->data.device_status->state),
		      device_boot_reason(event->data.device_status->reason));
		app_ctx->device_state = event->data.device_status->state;
		app_ctx->state_reason = event->data.device_status->reason;
		qsemaphore_give(app_ctx->device_state_sem);
		break;
	}
	case CHERRY_CORE_EVENT_TYPE_ERROR:
		QLOGD("Error core event %d received",
		      event->data.device_error->status_err);
		break;
	default:
		QLOGD("Ignored Cherry core event %u", event->type);
		break;
	}

	/* Free allocated event. */
	cherry_core_event_free(event);
	return;
}

void fira_cb(struct cherry_fira_event *event, void *user_data)
{
	struct fira_session_context *session_ctx = user_data;

	switch (event->type) {
	case CHERRY_FIRA_EVENT_TYPE_SESSION_STATUS:
		session_ctx->session_state = event->data.status->session_state;
		session_ctx->nb_session_state_event++;

		switch (event->data.status->session_state) {
		case CHERRY_FIRA_SESSION_STATE_INIT:
			QLOGD("FiRa session %d status changed to INIT",
			      session_ctx->session_id);
			break;
		case CHERRY_FIRA_SESSION_STATE_DEINIT:
			QLOGD("FiRa session %d status changed to DEINIT",
			      session_ctx->session_id);
			session_ctx->deinit = true;
			break;
		case CHERRY_FIRA_SESSION_STATE_ACTIVE:
			QLOGD("FiRa session %d status changed to ACTIVE",
			      session_ctx->session_id);
			break;
		case CHERRY_FIRA_SESSION_STATE_IDLE:
			QLOGD("FiRa session %d status changed to IDLE",
			      session_ctx->session_id);
			if (session_ctx->nb_measurments > 0)
				session_ctx->stop = true;
			break;
		default:
			QLOGD("FiRa session %d status changed: unknown state %d",
			      event->data.status->session_state,
			      session_ctx->session_id);
			break;
		}
		break;
	case CHERRY_FIRA_EVENT_TYPE_SESSION_ERROR:
		QLOGD("FiRa session %d error event received: %s",
		      session_ctx->session_id,
		      cherry_err_str(event->data.error->status_err));
		session_ctx->error = true;
		break;
	case CHERRY_FIRA_EVENT_TYPE_SESSION_TWR_RANGING_REPORT:
		QLOGD("Ranging report for session id %u",
		      session_ctx->session_id);
		session_ctx->nb_measurments++;
		break;
	default:
		QLOGD("Ignored FiRa session %d event %u",
		      session_ctx->session_id, event->type);
		break;
	}

	/* Free allocated event. */
	cherry_fira_event_free(event);
}

void radar_cb(struct cherry_radar_event *event, void *user_data)
{
	struct radar_session_context *session_ctx = user_data;

	switch (event->type) {
	case CHERRY_RADAR_EVENT_TYPE_SESSION_STATUS:
		session_ctx->session_state = event->data.status->session_state;
		session_ctx->nb_session_state_event++;

		switch (event->data.status->session_state) {
		case CHERRY_RADAR_SESSION_STATE_INIT:
			QLOGD("Radar session %d status changed to INIT",
			      session_ctx->session_id);
			break;
		case CHERRY_RADAR_SESSION_STATE_DEINIT:
			QLOGD("Radar session %d status changed to DEINIT",
			      session_ctx->session_id);
			session_ctx->deinit = true;
			break;
		case CHERRY_RADAR_SESSION_STATE_ACTIVE:
			QLOGD("Radar session %d status changed to ACTIVE",
			      session_ctx->session_id);
			break;
		case CHERRY_RADAR_SESSION_STATE_IDLE:
			QLOGD("Radar session %d status changed to IDLE",
			      session_ctx->session_id);
			break;
		default:
			QLOGD("Radar session %d status changed: unknown state %d",
			      event->data.status->session_state,
			      session_ctx->session_id);
			break;
		}
		break;
	case CHERRY_RADAR_EVENT_TYPE_SESSION_ERROR:
		QLOGD("Radar session %d error event received: %s",
		      session_ctx->session_id,
		      cherry_err_str(event->data.error->status_err));
		session_ctx->error = true;
		if (event->data.error->status_err ==
		    CHERRY_ERR_SESSION_TYPE_NOT_SUPPORTED)
			session_ctx->not_supported = true;
		break;
	default:
		QLOGD("Ignored Radar session %d event %u",
		      session_ctx->session_id, event->type);
		break;
	}

	/* Free allocated event. */
	cherry_radar_event_free(event);
}

static bool reset_device(struct app_context *app_ctx, struct cherry *cherry_ctx,
			 bool hard)
{
	app_ctx->device_state = CHERRY_CORE_DEVICE_STATE_ERROR;
	cherry_reset_device(cherry_ctx, hard);
	return app_wait_device_state_ready(app_ctx, 5000);
}

/**
 *  1. Init Cherry
 *  2. Reset UWBS
 *  3. Create a first FiRa session and set nb a measurements
 *  4. Start the first FiRa session and wait a little to get some ranging rounds
 *  5. Create a Radar session
 *  6. Wait a little to continue to get some ranging rounds
 *  7. Check if CHERRY_ERR_SESSION_TYPE_NOT_SUPPORTED has been received in Radar callback
 *  8. Create a second FiRa session and set nb a measurements
 *  9. Start the Radar session and check if CHERRY_ERR_SESSION_TYPE_NOT_SUPPORTED has been returned
 * 10. Start the second FiRa session and wait for all measurements
 * 11. Check the both FiRa sessions have received the correct number of ranging rounds
 * 12. Destroy the sessions
 * 13. Release Cherry
 *      '-> Wait for device state READY event
 */
int test_session_unsupported(enum cherry_log_level log_level,
			     const char *device)
{
	enum cherry_err err;
	struct cherry *cherry_ctx;
	struct cherry_fira_session *fira_session[2];
	struct cherry_radar_session *radar_session = NULL;
	struct app_context app_ctx;
	struct fira_session_context fira_sess_ctx[2] = {
		{ .stop = false,
		  .deinit = false,
		  .error = false,
		  .session_id = FIRA_SESSION_ID,
		  .nb_measurments = 0,
		  .nb_session_state_event = 0 },
		{ .stop = false,
		  .deinit = false,
		  .error = false,
		  .session_id = FIRA_SESSION_ID + 1,
		  .nb_measurments = 0,
		  .nb_session_state_event = 0 }
	};

	struct radar_session_context radar_sess_ctx = {
		.deinit = false,
		.error = false,
		.session_id = RADAR_SESSION_ID,
		.nb_session_state_event = 0,
		.not_supported = false,
	};

	uint16_t address[] = { CONTROLEE_MAC_ADDRESS };
	int error_count = 0;

	app_init(&app_ctx);

	cherry_ctx = cherry_create(device, &core_cb, &app_ctx);
	if (!cherry_ctx) {
		QLOGE("cherry_create failed !!!");
		error_count++;
		goto end;
	}

	cherry_set_log_level(cherry_ctx, log_level, CHERRY_LOG_MODULE_ALL);

	/* Make sure we start from a known state */
	QLOGI("Reset UWBS...");

	if (!reset_device(&app_ctx, cherry_ctx, true)) {
		QLOGE("Wait device ready failed.");
		++error_count;
		goto destroy_core;
	}

	QLOGI("Create FiRa session %d...", fira_sess_ctx[0].session_id);
	fira_session[0] = cherry_fira_session_create_twr_controller(
		cherry_ctx, fira_cb, &fira_sess_ctx[0],
		fira_sess_ctx[0].session_id, RANGING_INTERVAL, address, 1,
		CONTROLLER_MAC_ADDRESS);
	if (!fira_session[0]) {
		QLOGE("cherry_fira_session_create_twr_controller failed.");
		error_count++;
		goto destroy_core;
	}

	/* Set nb measurements to check all ranging rounds are done */
	err = cherry_fira_session_set_nb_measurement(
		fira_session[0], FIRA_SESSION_NB_MEASUREMENTS);
	if (err) {
		QLOGE("cherry_fira_session_set_nb_measurement failed with error %d.",
		      err);
		error_count++;
		goto destroy_fira1;
	}

	QLOGI("Start FiRa session %d...", fira_sess_ctx[0].session_id);
	err = cherry_fira_session_start(fira_session[0]);
	if (err) {
		QLOGE("cherry_fira_session_start failed with error %d.", err);
		error_count++;
		goto destroy_fira1;
	}

	/* Let some time for the session to start and do some ranging rounds */
	qtime_msleep(2000);

	/* Create RADAR session */
	QLOGI("Create Radar session...");
	radar_session = cherry_radar_session_create(cherry_ctx, radar_cb,
						    &radar_sess_ctx,
						    radar_sess_ctx.session_id,
						    100, 1200, 1, 64, 1);
	if (!radar_session) {
		QLOGE("cherry_radar_session_create failed.");
		error_count++;
		goto destroy_fira1;
	}

	/* Continue to receive some ranging rounds */
	qtime_msleep(2000);

	/* Check error received on Radar session creation */
	if (!radar_sess_ctx.not_supported) {
		QLOGE("CHERRY_ERR_SESSION_TYPE_NOT_SUPPORTED has not been received in Radar cb.");
		error_count++;
	}

	QLOGI("Create FiRa session %d...", fira_sess_ctx[1].session_id);
	fira_session[1] = cherry_fira_session_create_twr_controller(
		cherry_ctx, fira_cb, &fira_sess_ctx[1],
		fira_sess_ctx[1].session_id, RANGING_INTERVAL, address, 1,
		CONTROLLER_MAC_ADDRESS);
	if (!fira_session[1]) {
		QLOGE("cherry_fira_session_create_twr_controller failed.");
		error_count++;
		goto destroy_fira1;
	}

	/* Set nb measurements to check all ranging rounds are done */
	err = cherry_fira_session_set_nb_measurement(
		fira_session[1], FIRA_SESSION_NB_MEASUREMENTS);
	if (err) {
		QLOGE("cherry_fira_session_set_nb_measurement failed with error %d.",
		      err);
		error_count++;
		goto destroy_fira1;
	}

	/* Start Radar session: it shall fail */
	err = cherry_radar_session_start(radar_session);
	if (err) {
		QLOGI("cherry_radar_session_start failed: %s.",
		      cherry_err_str(err));
		if (err != CHERRY_ERR_SESSION_TYPE_NOT_SUPPORTED)
			error_count++;
	} else {
		QLOGE("cherry_radar_session_start did not failed whereas it is unsupported!");
		error_count++;
	}

	QLOGI("Start FiRa session %d...", fira_sess_ctx[1].session_id);
	err = cherry_fira_session_start(fira_session[1]);
	if (err) {
		QLOGE("cherry_fira_session_start failed with error %d.", err);
		error_count++;
		goto destroy_fira1;
	}

	/* Wait ranging rounds done or error occurs */
	while (!fira_sess_ctx[1].stop && !fira_sess_ctx[1].error &&
	       !fira_sess_ctx[0].error)
		qtime_msleep(10);

	if (fira_sess_ctx[0].nb_measurments != FIRA_SESSION_NB_MEASUREMENTS) {
		QLOGE("Error: %u/%u of received for session.",
		      fira_sess_ctx[0].nb_measurments,
		      fira_sess_ctx[0].session_id);
		error_count++;
	}

	if (fira_sess_ctx[1].nb_measurments != FIRA_SESSION_NB_MEASUREMENTS) {
		QLOGE("Error: %u/%u of received for session.",
		      fira_sess_ctx[1].nb_measurments,
		      fira_sess_ctx[1].session_id);
		error_count++;
	}

	/* Destroy the sessions */
	cherry_fira_session_destroy(fira_session[1]);
	cherry_radar_session_destroy(radar_session);

destroy_fira1:
	cherry_fira_session_destroy(fira_session[0]);

destroy_core:
	QLOGI("Destroy cherry...");
	cherry_destroy_sync(cherry_ctx);

end:
	app_deinit(&app_ctx);
	QLOGI("TEST RESULT ==> %s ended with %d error\n", __func__,
	      error_count);
	return error_count;
}

int main(int argc, char *argv[])
{
	int opt;
	char *endptr;
	char *help =
		"\nusage: ./cherry-test-session-deinit [-r] [-l N] [-d device]\n"
		"\nOptions:\n"
		"\t-l N\tset cherry's log level (default is 2 for warning)\n"
		"\t-d device\tset the uci device path, default is /dev/uci0\n";
	int error_count = 0;
	enum cherry_log_level log_level = CHERRY_LOG_LEVEL_WARN;
	char *device = "/dev/uci0";

	while ((opt = getopt(argc, argv, OPTSTR)) != -1) {
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
			device = optarg;
			break;
		case 'h':
		default:
			printf("%s", help);
			return 0;
		}
	}

	error_count += test_session_unsupported(log_level, device);

	QLOGI("TEST Summary ==> %d errors\n", error_count);
	return error_count ? -1 : 0;
}
