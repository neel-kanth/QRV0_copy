/*
 * Header file for uci calibration client
 *
 * SPDX-FileCopyrightText: SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#ifndef CHERRY_CALIB_CLIENT_H
#define CHERRY_CALIB_CLIENT_H

#include <cherry/cherry.h>
#include <qerr.h>
#include <uci/uci.h>

/*
 * typedef cherry_uci_calib_alloc_cb_t - Type for calib allocation callback
 *
 * Callback function type used for calibration memory allocation.
 *
 * @user_data: Pointer to user data.
 * @size: Size to allocate.
 *
 * Return: Pointer to allocated data.
 */
typedef void *(*cherry_uci_calib_alloc_cb_t)(void *user_data, uint8_t size);

/**
 * typedef cherry_uci_calib_key_notif_cb_t - Type for calib key notifation callback.
 * @user_data: Pointer to user data.
 * @key_name: Pointer to the name of the key.
 * @data: Pointer to the data of the key.
 * @data_size: Size of the data.
 *
 * Return: True if no error.
 */
typedef bool (*cherry_uci_calib_key_notif_cb_t)(void *user_data,
						const char *key_name,
						const void *data,
						size_t data_size);

/**
 * struct cherry_calib_cb - Calibration callback functions wrapper
 * @user_data: Pointer to user-specific data passed to callbacks
 * @alloc: Callback function for allocation during calibration
 * @key_notif: Callback function for key notification during calibration
 *
 * This structure encapsulates the callback functions used during the
 * calibration process. It provides mechanisms for memory allocation
 * and key event notifications while maintaining user context through
 * the user_data pointer.
 */
struct cherry_calib_cb {
	void *user_data;
	cherry_uci_calib_alloc_cb_t alloc;
	cherry_uci_calib_key_notif_cb_t key_notif;
};

/**
 * DOC: uci client calibrations overview.
 *
 * The cherry_uci_client_calib allows to set configuration and calibration
 * values through UCI commands.
 *
 * Those commands uses the QORVO MCPS GID.
 */

struct cherry_calib_context;
struct cherry_uci_client_uwbs_config_set_cmd;

/**
 * cherry_uci_client_calib_set_key() - Sets a calibration value for a key.
 * @context: calibration context.
 * @key: the calibration key to define.
 * @value: the value to set for the calibration key.
 * @value_size: the size of the value to set.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_calib_set_key(struct cherry_calib_context *context,
				const char *key, const char *value,
				size_t value_size);

/**
 * cherry_uci_client_uwbs_config_set_cmd_create() - Create command to set UWBS configuration.
 * @context: calibration context.
 *
 * Return: new command or NULL if error.
 */
struct cherry_uci_client_uwbs_config_set_cmd *
cherry_uci_client_uwbs_config_set_cmd_create(
	struct cherry_calib_context *context);

/**
 * cherry_uci_client_uwbs_config_set_cmd_put() - Append new new to command.
 * @cmd: UWBS set configuration command.
 * @keyname: UWBS configuration key name.
 * @value: UWBS configuration value pointer.
 * @value_size: UWBS configuration value size.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_uwbs_config_set_cmd_put(
	struct cherry_uci_client_uwbs_config_set_cmd *cmd, const char *keyname,
	const uint8_t *value, uint8_t value_size);

/**
 * cherry_uci_client_uwbs_config_set_cmd_send() - Send UWBS set configuration command and
 * free associated memory.
 * @cmd: UWBS set configuration command.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_uwbs_config_set_cmd_send(
	struct cherry_uci_client_uwbs_config_set_cmd *cmd);

/**
 * cherry_uci_client_uwbs_config_set_cmd_abort() - Abort pending UWBS configuration
 * command.
 * @cmd: UWBS set configuration command.
 */
void cherry_uci_client_uwbs_config_set_cmd_abort(
	struct cherry_uci_client_uwbs_config_set_cmd *cmd);

/**
 * cherry_uci_client_calib_get_key() - Gets calibration values for keys.
 * @context: calibration context.
 * @keys: array of calibration keys to get.
 * @n_keys: number of keys in the array.
 * @cb: callbacks for allocation and notification.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_calib_get_key(struct cherry_calib_context *context,
				const char **keys, const uint16_t n_keys,
				struct cherry_calib_cb *cb);

/**
 * cherry_uci_client_calib_open() - Attach this backend to the uci context
 * to bridge uci communication to the MAC.
 * @context: cherry_calib_context to initialize.
 * @uci: UCI context.
 *
 * The uci_client_calibration is dependant on core context.
 *
 * Return: QERR_SUCCESS or error.
 */
enum qerr cherry_uci_client_calib_open(struct cherry_calib_context **context,
				       struct uci *uci);

/**
 * cherry_uci_client_calib_close() - Close this backend to the uci context
 * to bridge uci communication to the MAC.
 * @context: cherry_calib_context to close.
 * Return: QERR_SUCCESS or error.
 */
void cherry_uci_client_calib_close(struct cherry_calib_context *context);

#endif
