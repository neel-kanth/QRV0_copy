/*
 * Header file for uci ccc client
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#ifndef CHERRY_CCC_CLIENT_H
#define CHERRY_CCC_CLIENT_H

#include "cherry_session_client.h"

#include <cherry/cherry_ccc.h>
#include <stdbool.h>
#include <stdint.h>
#include <uci/uci.h>

/**
 * cherry_uci_client_ccc_free_data_controller_report() - Free data memory allocated for controller in report notification.
 *
 * @report: Pointer to report notifification data to free.
 */
void cherry_uci_client_ccc_free_data_controller_report(
	struct cherry_ccc_controller_session_report *controller_report);

/**
 * cherry_uci_client_ccc_free_data_controlee_report() - Free data memory allocated for controlee in report notification.
 *
 * @report: Pointer to report notifification data to free.
 */
void cherry_uci_client_ccc_free_data_controlee_report(
	struct cherry_ccc_controlee_session_report *controlee_report);

/**
 * cherry_uci_client_parse_ccc_controller_measurements() - Parse CCC controller measurements.
 * @data: Notification genereic data.
 * @twr_results: Pointer to structure to fill in.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum qerr cherry_uci_client_parse_ccc_controller_measurements(
	const struct session_ranging_data *data,
	struct cherry_ccc_controller_session_report *controller_report);

/**
 * cherry_uci_client_parse_ccc_controlee_measurements() - Parse CCC controlee measurements.
 * @data: Notification genereic data.
 * @twr_results: Pointer to structure to fill in.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum qerr cherry_uci_client_parse_ccc_controlee_measurements(
	const struct session_ranging_data *data,
	struct cherry_ccc_controlee_session_report *controlee_report);

#endif /* CHERRY_CCC_CLIENT_H */
