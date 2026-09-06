/*
 * Header file for uci radar client
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#ifndef CHERRY_RADAR_CLIENT_H
#define CHERRY_RADAR_CLIENT_H

#include <cherry/cherry_radar.h>
#include <stdbool.h>
#include <stdint.h>
#include <uci/uci.h>

struct cherry_uci_radar_ntf {
	uint32_t session_handle;
	struct cherry_radar_session_report *data;
};

/**
 * cherry_uci_client_radar_free_radar_report() - Free Radar data attached to report.
 *
 * @report: Pointer to Radar report.
 *
 * Return: Nothing.
 */
void cherry_uci_client_radar_free_radar_report(
	struct cherry_uci_radar_ntf *report);

/**
 * typedef cherry_uci_client_radar_notification_cb_t - Radar notification callback type.
 *
 * @report: Radar report.
 * @user_data: User data pointer given to cherry_uci_client_radar_open.
 *
 * Return: Nothing.
 */
typedef void (*cherry_uci_client_radar_notification_cb_t)(
	const struct cherry_uci_radar_ntf *report, void *user_data);

/*
 * typedef struct cherry_radar_context - Radar context
 */
struct cherry_radar_context;

/**
 * cherry_uci_client_radar_open() - Initialize the internal resources of the client.
 *
 * @context: Radar context to initialize.
 * @uci: UCI Core context.
 * @user_data: User data pointer to give back in callback.
 * @radar_cb: Callback to use to notify report.
 *
 * NOTE: This function must be called first. @cherry_uci_client_radar_close must be called
 * at the end of the application to ensure resources are freed.
 * The channel will be managed by the client, this means you should neither use
 * uwbmac_channel_create nor uwbmac_channel_release.
 *
 * Return: QERR_SUCCESS or error.
 */
enum qerr cherry_uci_client_radar_open(
	struct cherry_radar_context **context, struct uci *uci, void *user_data,
	cherry_uci_client_radar_notification_cb_t radar_cb);

/**
 * cherry_uci_client_radar_close() - Free all internal resources of the client.
 *
 * @context: Radar context to free.
 */
void cherry_uci_client_radar_close(struct cherry_radar_context *context);

/**
 * cherry_uci_client_radar_free_notif_base() - Free base memory allocated in report notification.
 *
 * @report Pointer to report notifification to free. Report remains allocated.
 */
void cherry_uci_client_radar_free_base_report(
	struct cherry_uci_radar_ntf *report);

/**
 * cherry_uci_client_radar_free_rtl_report() - Free data memory allocated in report notification.
 *
 * @report Pointer to report notifification data to free.
 */
void cherry_uci_client_radar_free_data_report(
	struct cherry_radar_session_report *data);

#endif /* CHERRY_RADAR_CLIENT_H */
