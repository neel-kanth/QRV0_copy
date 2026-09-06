/**
 * @file      shell-utils.h
 *
 * @brief     Shell UART utilities
 *
 * @author    Qorvo Paris Applications
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */

#pragma once

#include <zephyr/shell/shell.h>

void shell_stop_and_uart_acquire(const struct shell *sh);
int shell_start_and_uart_release();
int shell_uart_configure(uint32_t baudrate, bool use_rts_cts);
int user_button_init(void);
int uwb_uci_bridge_cmd(const struct shell *shell, size_t argc, char **argv);
