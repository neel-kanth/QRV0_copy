/*
 * This example allows to test all possible destroy sequences.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include <cherry/cherry_fira.h>
#include <getopt.h>
#include <qlog.h>
#include <qsemaphore.h>
#include <qtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OPTSTR "a12:3hl:d:"

extern char *optarg;

/* Default session config */
#define CONTROLLER_MAC_ADDRESS 10
#define CONTROLEE_MAC_ADDRESS 11
#define SESSION_ID 1
#define RANGING_INTERVAL 200

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

struct session_context {
	struct cherry_fira_session *session;
	enum cherry_fira_session_state session_state;
	unsigned nb_session_state_event;
	uint32_t session_id;
	bool error;
	bool deinit;
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
	struct session_context *session_ctx = user_data;

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
		break;
	default:
		QLOGD("Ignored FiRa session %d event %u",
		      session_ctx->session_id, event->type);
		break;
	}

	/* Free allocated event. */
	cherry_fira_event_free(event);
}

static bool reset_device(struct app_context *app_ctx, struct cherry *cherry_ctx,
			 bool hard)
{
	app_ctx->device_state = CHERRY_CORE_DEVICE_STATE_ERROR;
	cherry_reset_device(cherry_ctx, hard);
	return app_wait_device_state_ready(app_ctx, 5000);
}

/**
 * 1. Init Cherry
 * 2. Reset UWBS
 * 3. Create a session
 * 4. Start the session
 * 5. Destroy the session
 * 6. Set an extended parameter of the session
 *	'-> Should return an error as session is tag for destroy
 * 7. Release Cherry
 *      '-> Wait for device state READY event
 */
int test_destroy_before_stop(enum cherry_log_level log_level,
			     const char *device)
{
	enum cherry_err err;
	struct cherry *cherry_ctx;
	struct cherry_fira_session *session;
	struct app_context app_ctx;
	struct session_context sess_ctx = { .deinit = false,
					    .error = false,
					    .session_id = SESSION_ID,
					    .nb_session_state_event = 0 };
	uint16_t address[] = { CONTROLEE_MAC_ADDRESS };
	int error_count = 0;

	QLOGI("############################################");
	QLOGI("#  %s()", __func__);
	QLOGI("############################################");

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

	QLOGI("Create sessions...");
	session = cherry_fira_session_create_twr_controller(
		cherry_ctx, fira_cb, &sess_ctx, sess_ctx.session_id,
		RANGING_INTERVAL, address, 1, CONTROLLER_MAC_ADDRESS);
	if (!session) {
		QLOGE("cherry_fira_session_create_twr_controller failed.");
		error_count++;
	}

	QLOGI("Start session...");
	err = cherry_fira_session_start(session);
	if (err) {
		QLOGE("cherry_fira_session_start failed with error %d.", err);
		error_count++;
	}

	/* Let some time for the session to start */
	qtime_msleep(300);

	QLOGI("Destroy session...");
	cherry_fira_session_destroy(session);

destroy_core:
	QLOGI("Destroy cherry...");
	cherry_destroy_sync(cherry_ctx);

end:
	app_deinit(&app_ctx);
	QLOGI("TEST RESULT ==> %s ended with %d error\n", __func__,
	      error_count);
	return error_count;
}

int create_session(struct cherry *cherry_ctx, struct session_context *sess_ctx,
		   int state)
{
	enum cherry_err err;
	uint16_t address[] = { CONTROLEE_MAC_ADDRESS };
	int wait = 0;
	int error_count = 0;

	QLOGI("Create session %d...", sess_ctx->session_id);
	sess_ctx->session = cherry_fira_session_create_twr_controller(
		cherry_ctx, fira_cb, sess_ctx, sess_ctx->session_id,
		RANGING_INTERVAL, address, 1, CONTROLLER_MAC_ADDRESS);
	if (!sess_ctx->session) {
		QLOGE("cherry_fira_session_create_twr_controller failed.");
		error_count++;
		return error_count;
	}

	if (state >= 1) {
		QLOGI("Start session %d...", sess_ctx->session_id);
		err = cherry_fira_session_start(sess_ctx->session);
		if (err) {
			QLOGE("cherry_fira_session_start failed with error %d.",
			      err);
			error_count++;
		}

		/* Let some time for the session to start */
		qtime_msleep(300);
	}

	if (state >= 2) {
		QLOGI("Stop session %d...", sess_ctx->session_id);
		err = cherry_fira_session_stop(sess_ctx->session);
		if (err) {
			QLOGE("cherry_fira_session_stop failed with error %d.",
			      err);
			error_count++;
		}

		wait = 0;
		while (sess_ctx->session_state !=
		       CHERRY_FIRA_SESSION_STATE_IDLE) {
			if (wait++ > 500) {
				QLOGE("TEST ERROR: 5sec timeout waiting for session %d state IDLE",
				      sess_ctx->session_id);
				error_count++;
				break;
			}
			qtime_msleep(10);
		}
	}

	if (state == 3) {
		QLOGI("Destroy session %d...", sess_ctx->session_id);
		cherry_fira_session_destroy(sess_ctx->session);

		wait = 0;
		while (sess_ctx->session_state !=
		       CHERRY_FIRA_SESSION_STATE_DEINIT) {
			if (wait++ > 500) {
				QLOGE("TEST ERROR: 5sec timeout waiting for session %d state DEINIT",
				      sess_ctx->session_id);
				error_count++;
				break;
			}
			qtime_msleep(10);
		}
	}

	return error_count;
}

int destroy_session(struct session_context *sess_ctx)
{
	int wait = 0;
	int error_count = 0;

	QLOGI("Destroy session %d...", sess_ctx->session_id);
	cherry_fira_session_destroy(sess_ctx->session);

	wait = 0;
	while (sess_ctx->session_state != CHERRY_FIRA_SESSION_STATE_DEINIT) {
		if (wait++ > 500) {
			QLOGE("TEST ERROR: 5sec timeout waiting for session %d state DEINIT",
			      sess_ctx->session_id);
			error_count++;
			break;
		}
		qtime_msleep(10);
	}

	return error_count;
}

/**
 * 1. Init Cherry
 * 3. Create a session
 * 4. Start the session
 * 5. Destroy the session
 * 6. Set an extended parameter of the session
 *	'-> Should return an error as session is tag for destroy
 * 7. Release Cherry
 *      '-> Wait for device state READY event
 */
int test_reset_with_session(bool hard_reset, int state,
			    enum cherry_log_level log_level, const char *device)
{
	const unsigned nb_session_state_event[] = { 3, 4, 5, 5 };
	struct cherry *cherry_ctx;
	struct app_context app_ctx;
	struct session_context sess_ctx[] = { { .deinit = false,
						.error = false,
						.session_id = 2,
						.nb_session_state_event = 0 },
					      { .deinit = false,
						.error = false,
						.session_id = 3,
						.nb_session_state_event = 0 },
					      { .deinit = false,
						.error = false,
						.session_id = 4,
						.nb_session_state_event = 0 },
					      { .deinit = false,
						.error = false,
						.session_id = 5,
						.nb_session_state_event = 0 } };
	int error_count = 0;

	QLOGI("############################################");
	QLOGI("#  %s(%s, %u)", __func__,
	      (hard_reset ? "hard reset" : "soft reset"), state);
	QLOGI("############################################");

	app_init(&app_ctx);

	cherry_ctx = cherry_create(device, &core_cb, &app_ctx);
	if (!cherry_ctx) {
		QLOGE("cherry_create failed !!!");
		error_count++;
		goto end;
	}

	cherry_set_log_level(cherry_ctx, log_level, CHERRY_LOG_MODULE_ALL);

	if (state < 4) {
		error_count += create_session(cherry_ctx, &sess_ctx[0], state);
	} else {
		error_count += create_session(cherry_ctx, &sess_ctx[0], 0);
		error_count += create_session(cherry_ctx, &sess_ctx[1], 1);
		error_count += create_session(cherry_ctx, &sess_ctx[2], 2);
		error_count += create_session(cherry_ctx, &sess_ctx[3], 3);
	}

	qtime_msleep(1000);
	/* Trigger the reset */
	QLOGI("Reset UWBS...");
	if (!reset_device(&app_ctx, cherry_ctx, hard_reset)) {
		QLOGE("Wait device ready failed.");
		++error_count;
		goto destroy_core;
	}

	if ((hard_reset &&
	     app_ctx.state_reason !=
		     CHERRY_CORE_STATE_CHANGE_BOOT_REASON_UNKNOWN) ||
	    (!hard_reset &&
	     app_ctx.state_reason != CHERRY_CORE_STATE_CHANGE_SOFT_RESET)) {
		QLOGE("TEST ERROR: invalid reboot state after reset");
		error_count++;
		goto destroy_core;
	}

	if (state < 3)
		error_count += destroy_session(&sess_ctx[0]);
	if (state >= 4) {
		error_count += destroy_session(&sess_ctx[0]);
		error_count += destroy_session(&sess_ctx[1]);
		error_count += destroy_session(&sess_ctx[2]);
	}

destroy_core:
	QLOGI("Destroy cherry...");
	cherry_destroy_sync(cherry_ctx);

end:
	/* Check no missing or duplicated event */
	if (state < 4) {
		if (sess_ctx[0].nb_session_state_event !=
		    nb_session_state_event[state]) {
			QLOGE("TEST ERROR: Wrong nb session state NTF, %d instead of %d",
			      sess_ctx[0].nb_session_state_event,
			      nb_session_state_event[state]);

			error_count++;
		}
	} else {
		for (int i = 0; i < 4; i++) {
			if (sess_ctx[i].nb_session_state_event !=
			    nb_session_state_event[i]) {
				QLOGE("TEST ERROR: Wrong nb session state NTF, %d instead of %d",
				      sess_ctx[i].nb_session_state_event,
				      nb_session_state_event[i]);

				error_count++;
			}
		}
	}
	app_deinit(&app_ctx);
	QLOGI("TEST RESULT ==> %s ended with %d error\n", __func__,
	      error_count);
	return error_count;
}

/**
 * 1. Init Cherry
 * 2. Create a session
 * 3. Start the session
 * 6. Release Cherry
 *      '-> Wait for session DEINIT state event
 */
int test_cherry_destroy_while_ranging(enum cherry_log_level log_level,
				      const char *device)
{
	enum cherry_err err;
	struct cherry *cherry_ctx;
	struct app_context app_ctx;
	struct session_context sess_ctx = { .deinit = false,
					    .error = false,
					    .session_id = SESSION_ID,
					    .nb_session_state_event = 0 };
	uint16_t address[] = { CONTROLEE_MAC_ADDRESS };
	int wait = 0;
	int error_count = 0;

	QLOGI("############################################");
	QLOGI("#  %s()", __func__);
	QLOGI("############################################");

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

	QLOGI("Create session...");
	sess_ctx.session = cherry_fira_session_create_twr_controller(
		cherry_ctx, fira_cb, &sess_ctx, sess_ctx.session_id,
		RANGING_INTERVAL, address, 1, CONTROLLER_MAC_ADDRESS);
	if (!sess_ctx.session) {
		QLOGE("cherry_fira_session_create_twr_controller failed.");
		error_count++;
	}

	QLOGI("Start session...");
	err = cherry_fira_session_start(sess_ctx.session);
	if (err) {
		QLOGE("cherry_fira_session_start failed with error %d.", err);
		error_count++;
	}

	/* Let some time for the session to start */
	qtime_msleep(1000);

	QLOGI("Destroy session %d...", sess_ctx.session_id);
	cherry_fira_session_destroy(sess_ctx.session);

	wait = 0;
	while (sess_ctx.session_state != CHERRY_FIRA_SESSION_STATE_DEINIT) {
		if (wait++ > 500) {
			QLOGE("TEST ERROR: 5sec timeout waiting for session %d state DEINIT",
			      sess_ctx.session_id);
			error_count++;
			break;
		}
		qtime_msleep(10);
	}

destroy_core:
	QLOGI("Destroy cherry...");
	cherry_destroy_sync(cherry_ctx);

	qtime_msleep(1000);

end:
	app_deinit(&app_ctx);
	QLOGI("TEST RESULT ==> %s ended with %d error\n", __func__,
	      error_count);
	return error_count;
}

#define TEST_2_MIN 0
#define TEST_2_MAX 4

int main(int argc, char *argv[])
{
	int opt;
	char *endptr;
	char *help =
		"\nusage: ./cherry-test-session-deinit [-1] [-2 X] [-3]\n"
		"\nOptions:\n"
		"\t-1\tdo session destroy before stop\n"
		"\t-2 X\tdo reset session in state X (0: Created, 1: Running, 3: Stopped, 4: Destroyed)\n"
		"\t-3\tdo cherry destroy before session destroy\n"
		"\t-l N\tset cherry's log level (default is 2 for warning)\n"
		"When no parameter are provided, all tests are executed\n"
		"\t-d device\tset the uci device path, default is /dev/uci0\n";
	bool test_1 = false;
	bool test_2 = false;
	bool test_3 = false;
	bool test_not_all = false;
	int test_2_min_state = TEST_2_MIN;
	int test_2_max_state = TEST_2_MAX;
	int error_count = 0;
	enum cherry_log_level log_level = CHERRY_LOG_LEVEL_WARN;
	char device[32];

	strcpy(device, "/dev/uci0");

	while ((opt = getopt(argc, argv, OPTSTR)) != -1) {
		switch (opt) {
		case '1':
			test_not_all = true;
			test_1 = true;
			break;
		case '2':
			test_not_all = true;
			test_2 = true;
			test_2_min_state = (int)strtol(optarg, &endptr, 10);
			if (*endptr) {
				QLOGE("Invalid number for session state.");
				return -1;
			}
			test_2_max_state = test_2_min_state;
			if (test_2_min_state < TEST_2_MIN ||
			    test_2_min_state > TEST_2_MAX) {
				QLOGE("Invalid session state %d, it has to be between %d and %d",
				      test_2_min_state, TEST_2_MIN, TEST_2_MAX);
				return -1;
			}
			break;
		case '3':
			test_not_all = true;
			test_3 = true;
			break;
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
			printf("%s", help);
			return 0;
		}
	}

	if (!test_not_all) {
		test_1 = true;
		test_2 = true;
		test_3 = true;
	}

	if (test_1)
		error_count += test_destroy_before_stop(log_level, device);

	if (test_2) {
		int i;

		for (i = test_2_min_state; i <= test_2_max_state; i++) {
			error_count += test_reset_with_session(
				false, i, log_level, device);
			error_count += test_reset_with_session(
				true, i, log_level, device);
		}
	}

	if (test_3)
		error_count +=
			test_cherry_destroy_while_ranging(log_level, device);

	QLOGI("TEST Summary ==> %d errors\n", error_count);
	return error_count ? -1 : 0;
}
