/*
 * Header file for uci proxy client
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#ifndef CHERRY_PROXY_CLIENT_H
#define CHERRY_PROXY_CLIENT_H

#include <qerr.h>
#include <uci/uci.h>

/**
 * typedef cherry_uci_client_proxy_boot_cb_t - Type for boot NTF callback.
 * @new_state: The new state of the device.
 * @user_data: User data pointer given to proxy_client_open.
 *
 * Return: Nothing.
 */
typedef void (*cherry_uci_client_proxy_boot_cb_t)(
	const enum uci_qorvo_boot_reason reason, void *user_data);

/**
 * typedef cherry_uci_client_proxy_data_cb_t - Type for data callback.
 * @data_notif: Notification data.
 * @data_notif_sz: Size of notification data.
 * @user_data: User data pointer given to proxy_client_open.
 *
 * Return: Nothing.
 */
typedef void (*cherry_uci_client_proxy_data_cb_t)(uint8_t *data_notif,
						  uint16_t data_notif_sz,
						  void *user_data);

/**
 * typedef struct cherry_proxy_context - Proxy client context.
 *
 */
struct cherry_proxy_context;

/**
 * cherry_uci_client_proxy_open() - Initialize the internal resources of the client.
 *
 * @context: Proxy context to initialize.
 * @tr: UCI transport to use.
 * @user_data: Application context to pass along for the callback.
 * @bridge: True to act as a bridge to handle commands,
 *          else it handles responses.
 * @boot_cb: Callback to use to notify each boot NTF, NULL is bridge is true.
 * @data_cb: Callback to use to notify each data received.
 *
 * Return: QERR_SUCCESS or error.
 */
enum qerr cherry_uci_client_proxy_open(
	struct cherry_proxy_context **context, struct uci_transport *tr,
	void *user_data, bool bridge, cherry_uci_client_proxy_boot_cb_t boot_cb,
	cherry_uci_client_proxy_data_cb_t data_cb);

/**
 * cherry_uci_client_proxy_close() - Free all internal resources of the client.
 *
 * @context: Proxy context to free.
 *
 */
void cherry_uci_client_proxy_close(struct cherry_proxy_context *context);

/**
 * cherry_uci_client_proxy_send_data() - Send data to UCI.
 *
 * @context: Proxy context.
 * @data: Pointer to data to send.
 * @len: Data length.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_proxy_send_data(struct cherry_proxy_context *context,
				  const uint8_t *data, uint16_t len);

/**
 * cherry_uci_client_proxy_get() - Retrieve UCI client instance.
 *
 * @context: Proxy context.
 *
 * Return: Pointer to enclosed struct uci.
 */
struct uci *cherry_uci_client_proxy_get(struct cherry_proxy_context *context);

#endif /* CHERRY_PROXY_CLIENT_H */
