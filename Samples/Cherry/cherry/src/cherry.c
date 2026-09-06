/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#ifndef CHERRY_C
#define CHERRY_C

#include "cherry_calib_mut.h"
#include "cherry_log.h"
#include "cherry_priv.h"
#include "cherry_session_manager.h"

#include <byteswap.h>
#include <cherry/cherry.h>
#include <cherry_calib_client.h>
#include <cherry_core_client.h>
#include <qmalloc.h>
#include <qsemaphore.h>
#include <qtime.h>

unsigned int cherry_log_level = CHERRY_LOG_LEVEL_WARN;

struct cherry_set_calib_params {
	bool reload;
	const struct cherry_calib *calib;
};

struct cherry_get_calib_params {
	const char **keys;
	const uint32_t n_keys;
};

enum cherry_err cherry_set_log_level(struct cherry *ctx,
				     enum cherry_log_level level,
				     enum cherry_log_module module)
{
	if (module == CHERRY_LOG_MODULE_ALL) {
		cherry_log_level = level;
		uci_set_log_level(level);
	} else
		return CHERRY_ERR_INVALID_PARAMETER;

	return CHERRY_ERR_NONE;
}

/* Translate uci status code to cherry error. */
enum cherry_err cherry_get_error(enum uci_status_code code)
{
	enum cherry_err err;

	switch (code) {
	case UCI_STATUS_OK:
		err = CHERRY_ERR_NONE;
		break;
	case UCI_STATUS_INVALID_PARAM:
		err = CHERRY_ERR_INVALID_PARAMETER;
		break;
	case UCI_STATUS_UCI_MESSAGE_RETRY:
		err = CHERRY_ERR_UWBS_TIMEOUT;
		break;
	case UCI_STATUS_ERROR_SESSION_ACTIVE:
		err = CHERRY_ERR_SESSION_ACTIVE;
		break;
	case UCI_STATUS_UNKNOWN:
	default:
		err = CHERRY_ERR_INTERNAL;
		break;
	}
	return err;
}

bool cherry_state_is_valid(struct cherry *cherry_ctx)
{
	return (cherry_ctx->internal_state != CHERRY_CORE_INTERNAL_STATE_ERROR);
}

/* Set internal state ERROR and force stop all sessions. */
void cherry_set_error_state(struct cherry *ctx)
{
	ctx->internal_state = CHERRY_CORE_INTERNAL_STATE_ERROR;
	cherry_force_stop_all_sessions(ctx);
}

static void cherry_thread_task_set_calib(const void *context,
					 const void *params, bool abort);

/* Error event */
static void cherry_send_error_event(struct cherry *ctx,
				    enum cherry_err status_err)
{
	struct cherry_core_event *event;
	struct cherry_core_event_device_error *cherry_device_error;

	event = qmalloc(sizeof(struct cherry_core_event));
	if (!event) {
		QLOGE("%s: Unable to allocate event.", __func__);
		return; /* No notif ?? */
	}

	event->data.device_error =
		qmalloc(sizeof(struct cherry_core_event_device_error));
	cherry_device_error = event->data.device_error;
	if (!cherry_device_error) {
		qfree(event);
		QLOGE("%s: Unable to allocate event.", __func__);
		return; /* No notif ?? */
	}

	event->type = CHERRY_CORE_EVENT_TYPE_ERROR;
	cherry_device_error->status_err = status_err;
	ctx->core_cb(event, ctx->user_data);
}

/*
 * Wait boot notification.
 */
static bool wait_boot_notification(struct cherry *ctx)
{
	int64_t timeout;
	int64_t timeout_ms;

	timeout = qtime_get_uptime_us() + CHERRY_BOOT_NOTIFICATION_TIMEOUT_US;
	do {
		if (ctx->boot_reason != CHERRY_CORE_STATE_CHANGE_ACTIVITY)
			return true;
		timeout_ms = (timeout - qtime_get_uptime_us()) / 1000;
		if (timeout_ms <= 0)
			break;

		cherry_thread_process_prio_task(&ctx->thread_ctx, timeout_ms);
	} while (true);

	return false;
}

/*
 * Wait device status notification.
 */
static bool wait_device_state(struct cherry *ctx)
{
	int64_t timeout;
	int64_t timeout_ms;

	/* Wait the device state notification. */
	timeout = qtime_get_uptime_us() +
		  CHERRY_DEVICE_STATE_NOTIFICATION_TIMEOUT_US;

	do {
		if (ctx->internal_state != CHERRY_CORE_INTERNAL_STATE_REBOOTING)
			return true;
		timeout_ms = (timeout - qtime_get_uptime_us()) / 1000;
		if (timeout_ms <= 0)
			break;

		cherry_thread_process_prio_task(&ctx->thread_ctx, timeout_ms);
	} while (true);

	QLOGE("Device status notification not received.");
	cherry_set_error_state(ctx);
	cherry_send_error_event(ctx, CHERRY_ERR_UWBS_TIMEOUT);

	return false;
}

static void
cherry_send_device_status_event(struct cherry *ctx,
				enum cherry_core_device_state device_state)
{
	struct cherry_core_event *event;
	struct cherry_core_event_device_status *cherry_device_status;

	event = qmalloc(sizeof(struct cherry_core_event));
	if (!event) {
		QLOGE("%s: Unable to allocate event.", __func__);
		return; /* No notif ?? */
	}

	event->data.device_status =
		qmalloc(sizeof(struct cherry_core_event_device_status));
	cherry_device_status = event->data.device_status;
	if (!cherry_device_status) {
		qfree(event);
		QLOGE("%s: Unable to allocate event.", __func__);
		return; /* No notif ?? */
	}
	/*
	 * Careful here: it works only if boot notification is received
	 * before device status notification.
	 */
	event->type = CHERRY_CORE_EVENT_TYPE_DEVICE_STATUS;
	cherry_device_status->state = device_state;
	cherry_device_status->reason = ctx->boot_reason;

	ctx->core_cb(event, ctx->user_data);
}

static void cherry_thread_device_status_cb(const void *user_data,
					   const void *params, bool abort)
{
	struct cherry *context = (struct cherry *)user_data;
	enum uci_device_state new_state = *(enum uci_device_state *)params;
	enum cherry_core_device_state device_state =
		CHERRY_CORE_DEVICE_STATE_ERROR;

	if (abort)
		return;

	if (context->boot_reason != CHERRY_CORE_STATE_CHANGE_ACTIVITY) {
		/* In case of fw crash we force stop all sessions. */
		if (context->boot_reason != CHERRY_CORE_STATE_CHANGE_SOFT_RESET)
			cherry_force_stop_all_sessions(context);
		/* Automatic reload of calibration. */
		if (context->calib) {
			struct cherry_set_calib_params params = {
				.reload = true,
			};
			if (cherry_thread_send_list_task(
				    &context->thread_ctx, context, &params,
				    sizeof(params),
				    cherry_thread_task_set_calib,
				    false) != CHERRY_ERR_NONE)
				QLOGE("%s: unable to reload the calibration.",
				      __func__);
		}
	}

	switch (new_state) {
	case UCI_DEVICE_STATE_READY:
		device_state = CHERRY_CORE_DEVICE_STATE_READY;
		context->internal_state = CHERRY_CORE_INTERNAL_STATE_READY;
		break;
	case UCI_DEVICE_STATE_ACTIVE:
		device_state = CHERRY_CORE_DEVICE_STATE_ACTIVE;
		context->internal_state = CHERRY_CORE_INTERNAL_STATE_ACTIVE;
		break;
	case UCI_DEVICE_STATE_ERROR:
		device_state = CHERRY_CORE_DEVICE_STATE_ERROR;
		context->internal_state = CHERRY_CORE_INTERNAL_STATE_ERROR;
		break;
	case UCI_DEVICE_STATE_INITIALIZING:
		/* Nothing to do, we don't notify the app and ignore it. */
		return;
	default:
		QLOGW("cherry_uci_client_device_status_cb received unknown UCI Device State: %d",
		      new_state);
		context->internal_state = CHERRY_CORE_INTERNAL_STATE_ERROR;
		break;
	}

	cherry_send_device_status_event(context, device_state);

	/* Always reset boot reason cause we do not always get it. */
	context->boot_reason = CHERRY_CORE_STATE_CHANGE_ACTIVITY;
}

static void
cherry_uci_client_device_status_cb(const enum uci_device_state new_state,
				   void *user_data)
{
	struct cherry *ctx = (struct cherry *)user_data;

	cherry_thread_send_prio_task(&ctx->thread_ctx, user_data, &new_state,
				     sizeof(new_state),
				     cherry_thread_device_status_cb);
}

/* Boot */
static void cherry_thread_boot_cb(const void *user_data, const void *params,
				  bool abort)
{
	struct cherry *context = (struct cherry *)user_data;
	enum uci_qorvo_boot_reason reason =
		*(enum uci_qorvo_boot_reason *)params;

	if (abort)
		return;

	switch (reason) {
	case UCI_QORVO_BOOT_REASON_UNKNOWN:
		context->boot_reason =
			CHERRY_CORE_STATE_CHANGE_BOOT_REASON_UNKNOWN;
		break;
	case UCI_QORVO_BOOT_REASON_FATAL_ERROR_RESET:
		context->boot_reason =
			CHERRY_CORE_STATE_CHANGE_BOOT_REASON_FATAL;
		break;
	default:
		QLOGW("cherry_uci_client_boot_cb received unknown UCI Boot reason: %d",
		      reason);
		break;
	}
}

static void cherry_uci_client_boot_cb(const enum uci_qorvo_boot_reason reason,
				      void *user_data)
{
	struct cherry *ctx = (struct cherry *)user_data;

	cherry_thread_send_prio_task(&ctx->thread_ctx, user_data, &reason,
				     sizeof(reason), cherry_thread_boot_cb);
}

static void cherry_thread_soft_reboot_cb(const void *user_data,
					 const void *params, bool abort)
{
	struct cherry *context = (struct cherry *)user_data;

	if (abort)
		return;

	context->boot_reason = CHERRY_CORE_STATE_CHANGE_SOFT_RESET;
}

static void cherry_set_soft_reboot_reason(struct cherry *ctx)
{
	cherry_thread_send_prio_task(&ctx->thread_ctx, ctx, NULL, 0,
				     cherry_thread_soft_reboot_cb);
}

static void cherry_thread_task_create_event(const void *context,
					    const void *params, bool abort)
{
	struct cherry *cherry_ctx = (struct cherry *)context;
	const char *device = params;
	enum cherry_err ret;
	enum uci_status_code r;
	enum qerr err;
	enum uci_device_state uwbs_state;
	enum cherry_core_device_state device_state =
		CHERRY_CORE_DEVICE_STATE_ERROR;

	if (abort)
		return;

	/* If something fails here all allocated memory will be deallocated in cherry_destroy_sync(). */
	ret = cherry_init_transport(&cherry_ctx->uci_transport_ctx,
				    &cherry_ctx->uci, device);
	if (ret != CHERRY_ERR_NONE) {
		QLOGE("%s: cherry_init_transport failed.", __func__);
		cherry_send_error_event(cherry_ctx, CHERRY_ERR_INTERNAL);
		return;
	}

	if ((err = cherry_uci_client_core_open(
		     &cherry_ctx->core_client_ctx, &cherry_ctx->uci, cherry_ctx,
		     cherry_uci_client_device_status_cb,
		     cherry_uci_client_boot_cb))) {
		QLOGE("%s: cherry_uci_client_core_open failed with error %d.",
		      __func__, err);
		cherry_send_error_event(cherry_ctx, CHERRY_ERR_INTERNAL);
		return;
	}

	/*
	 * If boot notification has been received, wait for the device state notification:
	 * this is the case where the UWBS was suspended (reset or power off).
	 * In this case the device status event will be sent as soon the device state notification
	 * will be received.
	 */
	if (wait_boot_notification(cherry_ctx)) {
		QLOGI("Boot notification has been received, wait device is Ready");
		wait_device_state(cherry_ctx);
		return;
	}

	/* If boot notification is not received: get the UWBS state. */
	if ((r = cherry_uci_client_core_get_uwbs_state(
		     cherry_ctx->core_client_ctx, &uwbs_state))) {
		QLOGE("%s: cherry_uci_client_core_get_uwbs_state failed with error %d.",
		      __func__, r);
		cherry_send_error_event(cherry_ctx, cherry_get_error(r));
		return;
	}

	switch (uwbs_state) {
	case UCI_DEVICE_STATE_READY:
		device_state = CHERRY_CORE_DEVICE_STATE_READY;
		cherry_ctx->internal_state = CHERRY_CORE_INTERNAL_STATE_READY;
		break;
	case UCI_DEVICE_STATE_ACTIVE:
		device_state = CHERRY_CORE_DEVICE_STATE_ACTIVE;
		cherry_ctx->internal_state = CHERRY_CORE_INTERNAL_STATE_ACTIVE;
		break;
	case UCI_DEVICE_STATE_ERROR:
		device_state = CHERRY_CORE_DEVICE_STATE_ERROR;
		cherry_ctx->internal_state = CHERRY_CORE_INTERNAL_STATE_ERROR;
		break;
	case UCI_DEVICE_STATE_INITIALIZING:
		QLOGI("UWBS is initializing, wait device is Ready");
		wait_device_state(cherry_ctx);
		break;
	default:
		QLOGW("Get unknown UCI Device State: %d", uwbs_state);
		cherry_ctx->internal_state = CHERRY_CORE_INTERNAL_STATE_ERROR;
		break;
		;
	}

	/* If UWBS was initializing, we should have received a device state notification. */
	if (uwbs_state == UCI_DEVICE_STATE_INITIALIZING)
		return;

	/* Send the device status event with current device state. */
	cherry_send_device_status_event(cherry_ctx, device_state);
	/* Always reset boot reason cause we do not always get it. */
	cherry_ctx->boot_reason = CHERRY_CORE_STATE_CHANGE_ACTIVITY;
}

static enum cherry_err cherry_send_task_create_event(struct cherry *ctx,
						     const char *device)
{
	if (!ctx)
		return CHERRY_ERR_INVALID_PARAMETER;

	return cherry_thread_send_list_task(&ctx->thread_ctx, ctx, device,
					    strlen(device) + 1,
					    cherry_thread_task_create_event,
					    false);
}

static struct uci_blk *simple_acquire(struct uci_allocator *allocator,
				      size_t size_hint, uint8_t flags_hint)
{
	struct uci_blk *p;

	p = (struct uci_blk *)qmalloc(sizeof(*p) + UCI_MAX_PACKET_SIZE);
	if (p) {
		p->data = (uint8_t *)&p[1];
		p->size = UCI_MAX_PACKET_SIZE;
	}
	return p;
}

static void simple_release(struct uci_allocator *allocator,
			   struct uci_blk *packet)
{
	qfree(packet);
}

static struct uci_allocator_ops simple_allocator_ops = {
	.alloc = simple_acquire,
	.free = simple_release,
};

static struct uci_allocator simple_allocator = { .ops = &simple_allocator_ops };

struct cherry *cherry_create(const char *device, cherry_core_cb_t core_cb,
			     void *user_data)
{
	struct cherry *context;

	if (!core_cb)
		return NULL;

	if (!device)
		return NULL;

	context = (struct cherry *)qcalloc(1, sizeof(struct cherry));

	if (!context) {
		QLOGE("%s: Unable to allocate memory.", __func__);
		return NULL;
	}

	if ((context->sessions_mutex = qmutex_init()) == NULL) {
		QLOGE("%s: qmutex_init failed.", __func__);
		qfree(context);
		return NULL;
	}

	if (cherry_thread_create(context) != CHERRY_ERR_NONE) {
		QLOGE("%s: Cherry thread creation failed.", __func__);
		qmutex_deinit(context->sessions_mutex);
		qfree(context);
		return NULL;
	}

	context->core_cb = core_cb;
	context->user_data = user_data;
	context->internal_state = CHERRY_CORE_INTERNAL_STATE_REBOOTING;
	context->boot_reason = CHERRY_CORE_STATE_CHANGE_ACTIVITY;
	context->core_client_ctx = NULL;
	context->calib = NULL;
	context->sessions = NULL;

	context->sessions_ntf = qsemaphore_init(0, 1);

	/* Initialization of UCI client */
	if (uci_init(&context->uci, &simple_allocator, true) != QERR_SUCCESS) {
		QLOGE("%s: Failed to initialize uci client", __func__);
		goto error_uci;
	}

	cherry_session_uci_client_setup(context);

	/* Init uci transport and core in thread. */
	if (cherry_send_task_create_event(context, device) != CHERRY_ERR_NONE) {
		QLOGE("%s: can not create Cherry core.", __func__);
		goto error;
	}

	return context;

error:
	cherry_session_close(context);
	uci_uninit(&context->uci);
error_uci:
	qsemaphore_deinit(context->sessions_ntf);
	qmutex_deinit(context->sessions_mutex);
	qfree(context);
	return NULL;
}

static void cherry_thread_task_destroy(const void *context, const void *params,
				       bool abort)
{
	struct cherry *cherry_ctx = (struct cherry *)context;

	if (abort)
		return;

	if (cherry_ctx->sessions) {
		QLOGE("There are pending sessions! Sessions have to be destroyed before calling"
		      " cherry_destroy_sync!");
		cherry_force_stop_all_sessions(cherry_ctx);
	}

	/* First close UCI thread to be sure no more notification comes.*/
	cherry_de_init_transport(&cherry_ctx->uci_transport_ctx);

	/* Then close the clients.*/
	cherry_fira_close(cherry_ctx);
	cherry_radar_close(cherry_ctx);
	cherry_session_close(cherry_ctx);

	cherry_uci_client_core_close(cherry_ctx->core_client_ctx);

	cherry_thread_stop(&cherry_ctx->thread_ctx);

	/* De-initialize the semaphore. */
	qsemaphore_deinit(cherry_ctx->sessions_ntf);

	qmutex_deinit(cherry_ctx->sessions_mutex);
	uci_uninit(&cherry_ctx->uci);
}

void cherry_destroy_sync(struct cherry *ctx)
{
	if (!ctx)
		return;

	if (cherry_thread_send_list_task(&ctx->thread_ctx, ctx, NULL, 0,
					 cherry_thread_task_destroy,
					 true) != CHERRY_ERR_NONE) {
		QLOGE("%s: error destroying Cherry.", __func__);
		return;
	}

	cherry_thread_join(&ctx->thread_ctx);
	cherry_thread_destroy(&ctx->thread_ctx);
	qfree(ctx);
}

void cherry_core_event_free(struct cherry_core_event *event)
{
	if (event) {
		switch (event->type) {
		case CHERRY_CORE_EVENT_TYPE_DEVICE_STATUS:
			if (event->data.device_status)
				qfree(event->data.device_status);
			break;
		case CHERRY_CORE_EVENT_TYPE_ERROR:
			if (event->data.device_error)
				qfree(event->data.device_error);
			break;
		case CHERRY_CORE_EVENT_TYPE_CALIB_UPDATE:
			if (event->data.calib_update)
				qfree(event->data.calib_update);
			break;
		case CHERRY_CORE_EVENT_TYPE_DEVICE_INFO:
			if (event->data.device_info) {
				if (event->data.device_info->fw_version)
					qfree(event->data.device_info
						      ->fw_version);
				qfree(event->data.device_info);
			}
			break;
		case CHERRY_CORE_EVENT_TYPE_DEVICE_STATS:
			if (event->data.device_stats)
				qfree(event->data.device_stats);
			break;
		case CHERRY_CORE_EVENT_TYPE_GET_CALIB:
			if (event->data.get_calib) {
				if (event->data.get_calib->calib)
					cherry_calib_destroy(
						event->data.get_calib->calib);
				qfree(event->data.get_calib);
			}
			break;
		case CHERRY_CORE_EVENT_TYPE_TIMESTAMP:
			if (event->data.device_timestamp)
				qfree(event->data.device_timestamp);
			break;
		case CHERRY_CORE_EVENT_TYPE_DEVICE_CAPS:
			if (event->data.device_caps) {
				cherry_uci_client_core_capabilities_free(
					event->data.device_caps);
				qfree(event->data.device_caps);
			}
			break;
		case CHERRY_CORE_EVENT_TYPE_GPIO_TOGGLE:
			if (event->data.gpio_toggle)
				qfree(event->data.gpio_toggle);
			break;
		}
		qfree(event);
	}
}

static void cherry_thread_task_reset(const void *context, const void *params,
				     bool abort)
{
	struct cherry *cherry_ctx = (struct cherry *)context;
	bool hard = *(bool *)params;
	enum uci_status_code r;

	if (abort)
		return;

	if (!cherry_ctx->uci_transport_ctx.transport) {
		QLOGE("%s: no transport, task can not be sent.", __func__);
		cherry_send_error_event(cherry_ctx, CHERRY_ERR_INTERNAL);
		return;
	}

	/*
	 * Internal state has to be updated before calling reset task to be
	 * able to reject all received data until receiving the device state
	 * notification.
	 */
	cherry_ctx->internal_state = CHERRY_CORE_INTERNAL_STATE_REBOOTING;

	/* Force stop the all sessions before reset to have clean behavior */
	cherry_force_stop_all_sessions(cherry_ctx);

	if (hard) {
		/* Reset the uci client then wait for the device state notification. */
		if (cherry_client_reset(&cherry_ctx->uci_transport_ctx) != 0) {
			QLOGE("%s: cherry_client_reset failed.", __func__);
			goto error;
		}
		/* Manage the case where no boot notification is sent. */
		cherry_ctx->boot_reason =
			CHERRY_CORE_STATE_CHANGE_BOOT_REASON_UNKNOWN;
	} else {
		/*
		 * Emulate a boot reason through a task to keep messages synchronized.
		 * Before send Reset command to be sure the boot reason comes just before the
		 * status notification.
		 */
		cherry_set_soft_reboot_reason(cherry_ctx);
		/* Send Software reset task. */
		if ((r = cherry_uci_client_core_device_reset(
			     cherry_ctx->core_client_ctx, 0))) {
			QLOGE("%s: cherry_uci_client_core_device_reset failed with error %d.",
			      __func__, r);
			goto error;
		}
	}

	wait_device_state(cherry_ctx);
	/* The event is sent in `cherry_uci_client_device_status_cb`. */
	return;

error:
	cherry_set_error_state(cherry_ctx);
	cherry_send_error_event(cherry_ctx, CHERRY_ERR_UWBS_TIMEOUT);
}

static void cherry_thread_task_get_device_info(const void *context,
					       const void *params, bool abort)
{
	struct cherry *cherry_ctx = (struct cherry *)context;
	struct cherry_core_event *event;
	struct cherry_core_event_device_info *cherry_device_info;
	enum uci_status_code r;
	enum cherry_err err;

	if (abort)
		return;

	event = (struct cherry_core_event *)qmalloc(
		sizeof(struct cherry_core_event));
	if (!event) {
		QLOGE("%s: Unable to allocate event.", __func__);
		return; /* No notif ?? */
	}

	event->data.device_info =
		qcalloc(1, sizeof(struct cherry_core_event_device_info));
	cherry_device_info = event->data.device_info;
	if (!cherry_device_info) {
		qfree(event);
		QLOGE("%s: Unable to allocate event.", __func__);
		return; /* No notif ?? */
	}

	cherry_device_info->fw_version =
		qcalloc(1, CHERRY_DEV_INFO_FW_VERSION_SIZE);
	if (!event->data.device_info->fw_version) {
		qfree(cherry_device_info);
		qfree(event);
		QLOGE("%s: Unable to allocate event.", __func__);
		return; /* No notif ?? */
	}

	if (!cherry_ctx->uci_transport_ctx.transport) {
		QLOGE("%s: no transport, task can not be sent.", __func__);
		err = CHERRY_ERR_INTERNAL;
		goto event;
	}

	if ((r = cherry_uci_client_core_get_device_info(
		     cherry_ctx->core_client_ctx, cherry_device_info))) {
		QLOGE("%s: cherry_uci_client_core_get_device_info  failed with error %d.",
		      __func__, r);
	}

	err = cherry_get_error(r);

event:
	event->type = CHERRY_CORE_EVENT_TYPE_DEVICE_INFO;
	cherry_device_info->status_err = err;
	cherry_ctx->core_cb(event, cherry_ctx->user_data);
}

static void cherry_thread_task_get_device_timestamp(const void *context,
						    const void *params,
						    bool abort)
{
	struct cherry *cherry_ctx = (struct cherry *)context;
	struct cherry_core_event *event;
	struct cherry_core_event_device_timestamp *cherry_device_timestamp;
	enum uci_status_code r = 0;
	enum cherry_err err;

	if (abort)
		return;

	event = (struct cherry_core_event *)qmalloc(
		sizeof(struct cherry_core_event));

	if (!event) {
		QLOGE("%s: Unable to allocate event.", __func__);
		return; /* No notif ?? */
	}

	event->data.device_timestamp =
		qmalloc(sizeof(struct cherry_core_event_device_timestamp));
	cherry_device_timestamp = event->data.device_timestamp;
	if (!cherry_device_timestamp) {
		qfree(event);
		QLOGE("%s: Unable to allocate event.", __func__);
		return; /* No notif ?? */
	}

	if (!cherry_ctx->uci_transport_ctx.transport) {
		QLOGE("%s: no transport, task can not be sent.", __func__);
		err = CHERRY_ERR_INTERNAL;
		goto event;
	}

	if ((r = cherry_uci_client_core_get_device_timestamp(
		     cherry_ctx->core_client_ctx, cherry_device_timestamp))) {
		QLOGE("%s: cherry_uci_client_core_get_device_timestamp failed with error %d.",
		      __func__, r);
	}
	err = cherry_get_error(r);

event:
	event->type = CHERRY_CORE_EVENT_TYPE_TIMESTAMP;
	cherry_device_timestamp->status_err = err;
	cherry_ctx->core_cb(event, cherry_ctx->user_data);
}

static void cherry_thread_task_get_capabilities(const void *context,
						const void *params, bool abort)
{
	struct cherry *cherry_ctx = (struct cherry *)context;
	struct cherry_core_event *event;
	struct cherry_core_event_device_capabilities *cherry_device_capabilities;
	enum uci_status_code r;
	enum cherry_err err;

	if (abort)
		return;

	event = (struct cherry_core_event *)qmalloc(
		sizeof(struct cherry_core_event));
	if (!event) {
		QLOGE("%s: Unable to allocate event.", __func__);
		return; /* No notif ?? */
	}

	event->data.device_caps = qcalloc(
		1, sizeof(struct cherry_core_event_device_capabilities));
	cherry_device_capabilities = event->data.device_caps;
	if (!cherry_device_capabilities) {
		qfree(event);
		QLOGE("%s: Unable to allocate event.", __func__);
		return; /* No notif ?? */
	}

	if (!cherry_ctx->uci_transport_ctx.transport) {
		QLOGE("%s: no transport, task can not be sent.", __func__);
		err = CHERRY_ERR_INTERNAL;
		goto event;
	}

	if ((r = cherry_uci_client_core_get_capabilities(
		     cherry_ctx->core_client_ctx,
		     cherry_device_capabilities))) {
		QLOGE("%s: cherry_uci_client_core_get_fira_capabilities  failed with error %d.",
		      __func__, r);
	}

	err = cherry_get_error(r);

event:
	event->type = CHERRY_CORE_EVENT_TYPE_DEVICE_CAPS;
	cherry_device_capabilities->status_err = err;
	cherry_ctx->core_cb(event, cherry_ctx->user_data);
}

static void cherry_thread_task_get_device_stats(const void *context,
						const void *params, bool abort)
{
	struct cherry *cherry_ctx = (struct cherry *)context;
	struct cherry_core_event *event;
	struct cherry_core_event_device_stats *cherry_device_stats;
	enum uci_status_code r;
	enum cherry_err err;

	if (abort)
		return;

	event = (struct cherry_core_event *)qmalloc(
		sizeof(struct cherry_core_event));

	if (!event) {
		QLOGE("%s: Unable to allocate event.", __func__);
		return; /* No notif ?? */
	}

	event->data.device_stats =
		qmalloc(sizeof(struct cherry_core_event_device_stats));
	cherry_device_stats = event->data.device_stats;
	if (!cherry_device_stats) {
		qfree(event);
		QLOGE("%s: Unable to allocate event.", __func__);
		return; /* No notif ?? */
	}

	if (!cherry_ctx->uci_transport_ctx.transport) {
		QLOGE("%s: no transport, task can not be sent.", __func__);
		err = CHERRY_ERR_INTERNAL;
		goto event;
	}

	if ((r = cherry_uci_client_core_get_uwb_device_stats(
		     cherry_ctx->core_client_ctx, cherry_device_stats))) {
		QLOGE("%s: cherry_uci_client_core_get_uwb_device_stats failed with error %d.",
		      __func__, r);
	}
	err = cherry_get_error(r);

event:
	event->type = CHERRY_CORE_EVENT_TYPE_DEVICE_STATS;
	cherry_device_stats->status_err = err;
	cherry_ctx->core_cb(event, cherry_ctx->user_data);
}

static void cherry_thread_task_toggle_gpio_time_sync(const void *context,
						     const void *params,
						     bool abort)
{
	struct cherry *cherry_ctx = (struct cherry *)context;
	struct cherry_core_event *event;
	struct cherry_core_event_gpio_toggle *cherry_gpio_toggle;
	uint8_t mode;
	enum uci_status_code r = 0;
	enum cherry_err err;

	if (abort)
		return;

	event = (struct cherry_core_event *)qmalloc(
		sizeof(struct cherry_core_event));

	if (!event) {
		QLOGE("%s: Unable to allocate event.", __func__);
		return; /* No notif ?? */
	}

	event->data.gpio_toggle =
		qmalloc(sizeof(struct cherry_core_event_gpio_toggle));
	cherry_gpio_toggle = event->data.gpio_toggle;
	if (!cherry_gpio_toggle) {
		qfree(event);
		QLOGE("%s: Unable to allocate event.", __func__);
		return; /* No notif ?? */
	}

	if (!cherry_ctx->uci_transport_ctx.transport) {
		QLOGE("%s: no transport, task can not be sent.", __func__);
		err = CHERRY_ERR_INTERNAL;
		goto event;
	}

	/* Get mode to use. */
	mode = *(uint8_t *)params;

	if ((r = cherry_uci_client_core_set_gpio_toggle_mode(
		     cherry_ctx->core_client_ctx, mode, cherry_gpio_toggle))) {
		QLOGE("%s: cherry_uci_client_core_set_gpio_toggle_mode failed with error %d.",
		      __func__, r);
	}
	err = cherry_get_error(r);

event:
	event->type = CHERRY_CORE_EVENT_TYPE_GPIO_TOGGLE;
	cherry_gpio_toggle->status_err = err;
	cherry_ctx->core_cb(event, cherry_ctx->user_data);
}

static bool cherry_calib_key_check_number_size(const char *name, uint8_t size)
{
	switch (size) {
	case 2:
	case 4:
	case 1:
		break;
	default:
		QLOGE("%s: invalid key size %u for %s key.", __func__, size,
		      name);
		return false;
	}
	return true;
}

union cherry_number {
	uint8_t value8;
	uint16_t value16;
	uint32_t value32;
};

static const uint8_t *cherry_calib_key_convert_number(const uint32_t number,
						      uint8_t size,
						      union cherry_number *out)
{
	/* Swap into little endian if needed. */
	switch (size) {
	case 1:
		out->value8 = number;
		return (const uint8_t *)&out->value8;
	case 2:
		out->value16 = qhtole16(number);
		return (const uint8_t *)&out->value16;
	case 4:
		out->value32 = qhtole32(number);
		return (const uint8_t *)&out->value32;
	}
	return NULL;
}

static void
cherry_calib_key_convert_number_array(const struct cherry_calib_key *key,
				      uint8_t *out)
{
	const uint8_t size = key->size / key->nb_array_items;
	const uint8_t *iter_in;
	const uint8_t *last_in;
	uint8_t *iter_out;

	/* Iterate from the beginning of input array... */
	iter_in = (const uint8_t *)key->data;
	/* ...until we reach the end of this input array. */
	last_in = iter_in + key->size;
	/* Iterate from the beginning of the output array. */
	iter_out = out;
	for (; iter_in < last_in; iter_in += size, iter_out += size) {
		switch (size) {
		case 1:
			*iter_out = *iter_in;
			break;
		case 2:
			(*(uint16_t *)iter_out) =
				qhtole16(*(uint16_t *)iter_in);
			break;
		case 4:
			(*(uint32_t *)iter_out) =
				qhtole32(*(uint32_t *)iter_in);
			break;
		}
	}
}

static void cherry_send_calib_event(struct cherry *cherry_ctx,
				    enum cherry_err status_err)
{
	struct cherry_core_event *event;
	struct cherry_core_event_calib_update *cherry_calib_update;

	event = (struct cherry_core_event *)qmalloc(
		sizeof(struct cherry_core_event));

	if (!event) {
		QLOGE("%s: Unable to allocate event.", __func__);
		return; /* No notif ?? */
	}
	event->data.calib_update =
		qmalloc(sizeof(struct cherry_core_event_calib_update));
	cherry_calib_update = event->data.calib_update;
	if (!cherry_calib_update) {
		QLOGE("%s: Unable to allocate event.", __func__);
		qfree(event);
		return; /* No notif ?? */
	}
	event->type = CHERRY_CORE_EVENT_TYPE_CALIB_UPDATE;
	cherry_calib_update->status_err = status_err;
	cherry_ctx->core_cb(event, cherry_ctx->user_data);
}

enum cherry_err
cherry_send_calib(const struct cherry_calib *calib,
		  struct cherry_uci_transport *uci_transport_ctx)
{
	struct cherry_calib_context *calib_ctx;
	struct cherry_uci_client_uwbs_config_set_cmd *cmd;
	const struct cherry_calib_key *key;
	uint32_t index;
	union cherry_number value;
	uint8_t *number_array_data = NULL;
	const uint8_t *data;
	enum cherry_err err;
	enum qerr qerror;
	enum uci_status_code r;

	if (!uci_transport_ctx->transport) {
		QLOGE("%s: no transport, command can not be sent.", __func__);
		err = CHERRY_ERR_INTERNAL;
		goto error;
	}

	/* Check keys validity. */
	if (calib->n_keys == 0) {
		QLOGE("%s: number of key is null.", __func__);
		err = CHERRY_ERR_INVALID_PARAMETER;
		goto error;
	}

	for (index = 0; index < calib->n_keys; index++) {
		key = &calib->keys[index];
		switch (key->type) {
		case CHERRY_CALIB_VALUE_NUMBER:
			if (!cherry_calib_key_check_number_size(key->name,
								key->size)) {
				err = CHERRY_ERR_INVALID_PARAMETER;
				goto error;
			}
			break;
		case CHERRY_CALIB_VALUE_NUMBER_ARRAY: {
			const uint8_t size = key->size / key->nb_array_items;
			if ((size * key->nb_array_items != key->size) ||
			    !cherry_calib_key_check_number_size(key->name,
								size)) {
				err = CHERRY_ERR_INVALID_PARAMETER;
				goto error;
			}
			break;
		}
		case CHERRY_CALIB_VALUE_DATA:
			break;
		default:
			QLOGE("%s: invalid key type %u for %s key.", __func__,
			      key->type, key->name);
			err = CHERRY_ERR_INVALID_PARAMETER;
			goto error;
		}
	}

	/* Open the calib client. */
	if ((qerror = cherry_uci_client_calib_open(&calib_ctx,
						   uci_transport_ctx->uci))) {
		QLOGE("%s: cherry_uci_client_calib_open fails, err %d.",
		      __func__, qerror);
		err = CHERRY_ERR_INTERNAL;
		goto error;
	}

	cmd = cherry_uci_client_uwbs_config_set_cmd_create(calib_ctx);
	if (!cmd) {
		QLOGE("%s: cherry_uci_client_uwbs_config_set_cmd_create fails, err %d.",
		      __func__, qerror);
		err = CHERRY_ERR_INTERNAL;
		goto error;
	}

	for (index = 0; index < calib->n_keys; index++) {
		key = &calib->keys[index];
		switch (key->type) {
		case CHERRY_CALIB_VALUE_NUMBER:
			data = cherry_calib_key_convert_number(
				key->number, key->size, &value);
			break;
		case CHERRY_CALIB_VALUE_NUMBER_ARRAY: {
			/*
			 * TODO: This part can be simplified for little endian
			 * platform
			 */
			uint8_t *tmp_number_array_data =
				qrealloc(number_array_data, key->size);

			if (!tmp_number_array_data) {
				QLOGE("%s: alloc(%d) failed for %s key ",
				      __func__, key->size, key->name);
				err = CHERRY_ERR_INTERNAL;
				goto close_calib;
			}
			number_array_data = tmp_number_array_data;

			cherry_calib_key_convert_number_array(
				key, number_array_data);
			data = number_array_data;
			break;
		}
		case CHERRY_CALIB_VALUE_DATA:
			data = (const uint8_t *)key->data;
			break;
		default:
			continue;
		}

		/* Append the key. */
		if ((r = cherry_uci_client_uwbs_config_set_cmd_put(
			     cmd, key->name, data, key->size))) {
			QLOGE("%s: cherry_uci_client_uwbs_config_set_cmd_put failed for %s key with error %d.",
			      __func__, key->name, r);
			err = cherry_get_error(r);
			/* Stop sending keys at first error. */
			goto close_calib;
		}
	}

	err = cherry_get_error(cherry_uci_client_uwbs_config_set_cmd_send(cmd));

close_calib:
	/* Close the calib client. */
	cherry_uci_client_calib_close(calib_ctx);
	qfree(number_array_data);

error:
	return err;
}

static void cherry_thread_task_set_calib(const void *context,
					 const void *params, bool abort)

{
	struct cherry *cherry_ctx = (struct cherry *)context;
	const struct cherry_set_calib_params *calib_params = params;
	const struct cherry_calib *calib;
	enum cherry_err err;

	if (abort)
		return;

	if (calib_params->reload)
		QLOGD("%s: calibration is reloaded.", __func__);
	else
		cherry_ctx->calib = calib_params->calib;

	calib = cherry_ctx->calib;
	if (!calib) {
		QLOGD("%s: calibration is null, nothing to do.", __func__);
		return;
	}

	err = cherry_send_calib(calib, &cherry_ctx->uci_transport_ctx);

	if (!calib_params->reload) {
		cherry_send_calib_event(cherry_ctx, err);
		return;
	}

	if (err)
		QLOGE("%s: error in calibration reloading.", __func__);

	return;
}

/* PUBLIC API */
enum cherry_err cherry_reset_device(struct cherry *ctx, bool hard)
{
	if (!ctx)
		return CHERRY_ERR_INVALID_PARAMETER;

	if (!cherry_state_is_valid(ctx) && !hard) {
		QLOGE("%s: internal state error.", __func__);
		return CHERRY_ERR_INTERNAL;
	}

	return cherry_thread_send_list_task(&ctx->thread_ctx, ctx, &hard,
					    sizeof(hard),
					    cherry_thread_task_reset, false);
}

enum cherry_err cherry_get_device_info(struct cherry *ctx)
{
	if (!ctx)
		return CHERRY_ERR_INVALID_PARAMETER;

	if (!cherry_state_is_valid(ctx)) {
		QLOGE("%s: internal state error.", __func__);
		return CHERRY_ERR_INTERNAL;
	}

	return cherry_thread_send_list_task(&ctx->thread_ctx, ctx, NULL, 0,
					    cherry_thread_task_get_device_info,
					    false);
}

enum cherry_err cherry_get_device_timestamp(struct cherry *ctx)
{
	if (!ctx)
		return CHERRY_ERR_INVALID_PARAMETER;

	if (!cherry_state_is_valid(ctx)) {
		QLOGE("%s: internal state error.", __func__);
		return CHERRY_ERR_INTERNAL;
	}

	return cherry_thread_send_list_task(
		&ctx->thread_ctx, ctx, NULL, 0,
		cherry_thread_task_get_device_timestamp, false);
}

enum cherry_err cherry_get_device_capabilities(struct cherry *ctx)
{
	if (!ctx)
		return CHERRY_ERR_INVALID_PARAMETER;

	if (!cherry_state_is_valid(ctx)) {
		QLOGE("%s: internal state error.", __func__);
		return CHERRY_ERR_INTERNAL;
	}

	return cherry_thread_send_list_task(&ctx->thread_ctx, ctx, NULL, 0,
					    cherry_thread_task_get_capabilities,
					    false);
}

enum cherry_err cherry_get_device_stats(struct cherry *ctx)
{
	if (!ctx)
		return CHERRY_ERR_INVALID_PARAMETER;

	if (!cherry_state_is_valid(ctx)) {
		QLOGE("%s: internal state error.", __func__);
		return CHERRY_ERR_INTERNAL;
	}

	return cherry_thread_send_list_task(&ctx->thread_ctx, ctx, NULL, 0,
					    cherry_thread_task_get_device_stats,
					    false);
}

enum cherry_err cherry_set_calib(struct cherry *ctx,
				 const struct cherry_calib *calib)

{
	struct cherry_set_calib_params params = {
		.reload = false,
		.calib = calib,
	};

	if (!ctx)
		return CHERRY_ERR_INVALID_PARAMETER;

	if (!cherry_state_is_valid(ctx)) {
		QLOGE("%s: internal state error.", __func__);
		return CHERRY_ERR_INTERNAL;
	}

	return cherry_thread_send_list_task(&ctx->thread_ctx, ctx, &params,
					    sizeof(params),
					    cherry_thread_task_set_calib,
					    false);
}

enum cherry_err
cherry_toggle_gpio_time_sync(struct cherry *ctx,
			     enum cherry_core_gpio_toggle_mode mode)
{
	if (!ctx)
		return CHERRY_ERR_INVALID_PARAMETER;

	if (!cherry_state_is_valid(ctx)) {
		QLOGE("%s: internal state error.", __func__);
		return CHERRY_ERR_INTERNAL;
	}

	return cherry_thread_send_list_task(
		&ctx->thread_ctx, ctx, &mode, sizeof(mode),
		cherry_thread_task_toggle_gpio_time_sync, false);
}

static void *cherry_calib_alloc_cb(void *user_data, uint8_t size)
{
	struct cherry_calib_mut *calib_mut =
		(struct cherry_calib_mut *)user_data;

	if (!calib_mut)
		return NULL;

	return cherry_calib_mut_alloc(calib_mut, size);
}

static bool cherry_calib_key_notif_cb(void *user_data, const char *key_name,
				      const void *data, size_t data_size)
{
	struct cherry_calib_mut *calib_mut =
		(struct cherry_calib_mut *)user_data;
	struct cherry_calib_key *tmp;
	struct cherry_calib_key *key;

	if (!calib_mut)
		return false;

	if (!calib_mut->mut_keys)
		calib_mut->calib.n_keys = 0;

	/* Extend array if required. */
	if (calib_mut->mut_keys_capacity == 0 ||
	    calib_mut->mut_keys_capacity == calib_mut->calib.n_keys) {
		calib_mut->mut_keys_capacity += 64;
		tmp = qrealloc(calib_mut->mut_keys,
			       calib_mut->mut_keys_capacity *
				       sizeof(*calib_mut->mut_keys));
		if (!tmp) {
			return false;
		}

		calib_mut->mut_keys = tmp;
	}

	key = &calib_mut->mut_keys[calib_mut->calib.n_keys++];
	key->name = key_name;
	key->type = CHERRY_CALIB_VALUE_DATA;
	key->size = data_size;
	key->data = data;

	return true;
}

static void cherry_thread_task_get_calib(const void *context,
					 const void *params, bool abort)

{
	struct cherry *cherry_ctx = (struct cherry *)context;
	const struct cherry_get_calib_params *calib_params = params;
	struct cherry_calib_context *calib_ctx;
	enum qerr qerror;
	enum uci_status_code r;
	struct cherry_core_event *event;
	struct cherry_core_event_get_calib *get_calib;
	enum cherry_err err;
	struct cherry_calib_mut *calib_mut = NULL;
	struct cherry_calib_cb calib_cb;

	if (abort)
		return;

	event = (struct cherry_core_event *)qmalloc(
		sizeof(struct cherry_core_event));
	if (!event) {
		QLOGE("%s: Unable to allocate event.", __func__);
		return; /* No notif ?? */
	}

	event->data.get_calib =
		qmalloc(sizeof(struct cherry_core_event_get_calib));
	get_calib = event->data.get_calib;
	if (!get_calib) {
		qfree(event);
		QLOGE("%s: Unable to allocate event.", __func__);
		return; /* No notif ?? */
	}

	get_calib->calib = NULL;

	if (!cherry_ctx->uci_transport_ctx.transport) {
		QLOGE("%s: no transport, command can not be sent.", __func__);
		err = CHERRY_ERR_INTERNAL;
		goto event;
	}

	/* Open the calib client. */
	if ((qerror = cherry_uci_client_calib_open(
		     &calib_ctx, cherry_ctx->uci_transport_ctx.uci))) {
		QLOGE("%s: cherry_uci_client_calib_open fails, err %d.",
		      __func__, qerror);
		err = CHERRY_ERR_INTERNAL;
		goto event;
	}

	/* Initialize a struct cherry_calib_mut to store data. */
	calib_mut = qmalloc(sizeof(*calib_mut));
	if (!calib_mut) {
		err = CHERRY_ERR_INTERNAL;
		goto event;
	}

	*calib_mut = CHERRY_CALIB_DYN_INIT_STATIC;

	calib_cb.user_data = calib_mut;
	calib_cb.alloc = cherry_calib_alloc_cb;
	calib_cb.key_notif = cherry_calib_key_notif_cb;

	/* Get the calibration keys. */
	if ((r = cherry_uci_client_calib_get_key(calib_ctx, calib_params->keys,
						 calib_params->n_keys,
						 &calib_cb))) {
		QLOGE("%s: cherry_uci_client_calib_get_key failed with error %d.",
		      __func__, r);
	}

	err = cherry_get_error(r);

	/* Close the calib client. */
	cherry_uci_client_calib_close(calib_ctx);

	if (err) {
		if (calib_mut) {
			cherry_calib_mut_destroy(calib_mut);
			qfree(calib_mut);
		}
	} else {
		calib_mut->calib.keys = calib_mut->mut_keys;
		get_calib->calib = &calib_mut->calib;
	}

event:
	event->type = CHERRY_CORE_EVENT_TYPE_GET_CALIB;
	get_calib->status_err = err;
	cherry_ctx->core_cb(event, cherry_ctx->user_data);
}

enum cherry_err cherry_get_calib(struct cherry *ctx, const char **keys,
				 const uint16_t n_keys)
{
	struct cherry_get_calib_params params = {
		.keys = keys,
		.n_keys = n_keys,
	};

	if (!ctx)
		return CHERRY_ERR_INVALID_PARAMETER;

	if (!cherry_state_is_valid(ctx)) {
		QLOGE("%s: internal state error.", __func__);
		return CHERRY_ERR_INTERNAL;
	}

	return cherry_thread_send_list_task(&ctx->thread_ctx, ctx, &params,
					    sizeof(params),
					    cherry_thread_task_get_calib,
					    false);
}

void cherry_calib_destroy(struct cherry_calib *calib)
{
	struct cherry_calib_mut *calib_mut;

	if (!calib)
		return;

	calib_mut = qparent_of(calib, struct cherry_calib_mut, calib);
	cherry_calib_mut_destroy(calib_mut);
	qfree(calib_mut);
}

const char *cherry_err_str(enum cherry_err err)
{
	switch (err) {
	case CHERRY_ERR_NONE:
		return "No error";
	case CHERRY_ERR_INVALID_PARAMETER:
		return "Invalid parameter";
	case CHERRY_ERR_UWBS_TIMEOUT:
		return "Timeout";
	case CHERRY_ERR_INTERNAL:
		return "Internal error";
	case CHERRY_ERR_SESSION_INIT:
		return "Session initialization error";
	case CHERRY_ERR_SESSION_ACTIVE:
		return "Invalid operation on active session";
	case CHERRY_ERR_SESSION_CONFIG:
		return "Invalid session configuration";
	case CHERRY_ERR_SESSION_TYPE_NOT_SUPPORTED:
		return "Session type not supported by UWBS";
	}
	return "Unkown";
}

#endif /* CHERRY_C */
