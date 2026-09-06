/**
 * @file      shell.h
 *
 * @brief     HSSPI driver shell test command
 *
 * @author    Qorvo Paris Applications
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */
#pragma once

#include <stddef.h>
#include <zephyr/shell/shell.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief HSSPI driver shell test command.
 *
 * Run HSSPI driver tests.
 *
 * @param shell The shell instance to use.
 * @param argc The number of argument on the command line.
 * @param argv Array of all arguments on the command line.
 */
int uwb_hsspi_tests_cmd(const struct shell *shell, size_t argc, char **argv);

#ifdef __cplusplus
}
#endif
