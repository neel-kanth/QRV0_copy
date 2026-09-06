/*
 * Public header for utilities for dumping different structures to a terminal.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <cherry/cherry_ccc.h>
#include <cherry/cherry_common.h>
#include <cherry/cherry_fira.h>
#include <cherry/cherry_radar.h>

/**
 * util_dump_twr_range_report() - Print TWR measurements report.
 * @cur_meas: TWR measurements report.
 *
 * Prints all content hidden under the given pointer
 * as well as recursively prints all nested fields.
 */
void util_dump_twr_range_report(
	const struct cherry_fira_session_twr_measurements *cur_meas);

/**
 * util_dump_dt_tag_report() - Print DT-TAG report.
 * @cur_meas: DT-TAG report.
 *
 * Prints all content hidden under the given pointer
 * as well as recursively prints all nested fields.
 */
void util_dump_dt_tag_report(
	const struct cherry_fira_session_dt_tag_measurements *cur_meas);

/**
 * util_dump_diagnostic() - Print diagnostic report.
 * @diagnostic: Diagnostic report.
 *
 * Prints all content hidden under the given pointer
 * as well as recursively prints all nested fields.
 */
void util_dump_diagnostic(const struct cherry_common_diag_report *diagnostic);

/**
 * util_dump_core_event() - Print core event data.
 * @event: Cherry core event.
 *
 * Prints all content hidden under the given pointer
 * as well as recursively prints all nested fields.
 */
void util_dump_core_event(const struct cherry_core_event *event);

/**
 * util_dump_session_dltdoa() - Print DLTDOA event data.
 * @event: DLTDOA event.
 * @session_id: ID of the session for logs.
 *
 * Prints all content hidden under the given pointer
 * as well as recursively prints all nested fields.
 */
void util_dump_session_fira(const struct cherry_fira_event *event,
			    uint32_t session_id);

/**
 * util_dump_session_radar() - Print RADAR event data.
 * @event: RADAR event.
 * @file: if non null the file to write raw sweeps.
 * @buffer: A buffer used by this function to dump the CIRs data. This buffer
 *  must be long enough to contain a complete CIR in text form.
 * @first_frame: Set to true to add common additional dump information.
 *
 * Prints all content hidden under the given pointer
 * as well as recursively prints all nested fields.
 */
void util_dump_session_radar(const struct cherry_radar_event *event,
			     const char *file, char *buffer, bool first_frame);

/**
 * util_dump_anchor_parameters() - Output round configurations for an anchor.
 * @round_config: Pointer to round configuration(s).
 * @n_rounds: Number of round configuration to go through.
 *
 * Prints all information on each round configurations.
 */
void util_dump_anchor_parameters(
	const struct cherry_fira_anchor_round_config *round_conf, int n_rounds);

/**
 * util_dump_calib() - Print calibrations data.
 * @calib: Pointer to calibration's structure.
 *
 * Prints keys' names and values.
 */
void util_dump_calib(const struct cherry_calib *calib);
