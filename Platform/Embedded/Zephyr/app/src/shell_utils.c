/**
 * @file      shell-utils.c
 *
 * @brief     Shell UART utilities
 *
 * @author    Qorvo Paris Applications
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */

#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#include <zephyr/drivers/uart.h>

LOG_MODULE_REGISTER(uart_utils, CONFIG_APP_LOG_LEVEL);

void shell_init_from_work(struct k_work *work)
{
	const struct device *const dev =
	    DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
	bool log_backend = CONFIG_SHELL_BACKEND_SERIAL_LOG_LEVEL > 0;
	uint32_t level =
	    (CONFIG_SHELL_BACKEND_SERIAL_LOG_LEVEL > LOG_LEVEL_DBG) ?
		CONFIG_LOG_MAX_LEVEL :
		CONFIG_SHELL_BACKEND_SERIAL_LOG_LEVEL;

	int ret = shell_init(shell_backend_uart_get_ptr(), dev,
			     shell_backend_uart_get_ptr()->ctx->cfg.flags,
			     log_backend, level);
	if (ret != 0) {
		LOG_ERR("Failed to re-init shell %d", ret);
	}
}

static int shell_reinit_trigger(void)
{
	static struct k_work shell_init_work;

	k_work_init(&shell_init_work, shell_init_from_work);
	return k_work_submit(&shell_init_work);
}

static void shell_uninit_cb(const struct shell *sh, int res)
{
}

void shell_stop_and_uart_acquire(const struct shell *sh)
{
	/* If this function is called from an uart shell command and if
	 * uart_irq_callback_user_data_set() is called in this function, the kill
	 * signal handler (kill_handler() from zephyr/subsys/shell/shell.c) raised
	 * by shell_uninit() is never called. The consequence is that
	 * the shell_uninit_cb callback is never called and  the shell thread is
	 * never set to NULL, which prevents the shell from restarting.
	 * So, uart_irq_callback_user_data_set() should be called in
	 * shell_uninit_cb() or after shell_uninit_cb() has been called.
	 * Note: the kill signal handler is only called after the return of shell
	 * command function.
	 */
	/* Set the prompt to empty string to avoid the last shell prompt. */
	shell_prompt_change(sh, "");
	shell_uninit(sh, shell_uninit_cb);
}

void shell_start_and_uart_release()
{
	shell_reinit_trigger();
}

int shell_uart_configure(uint32_t baudrate, bool use_rts_cts)
{
	const struct device *uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
	struct uart_config uart_cfg = {
	    .parity = UART_CFG_PARITY_NONE,
	    .stop_bits = UART_CFG_STOP_BITS_1,
	    .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
	    .data_bits = UART_CFG_DATA_BITS_8,
	};
	uart_cfg.baudrate = baudrate;
	if (use_rts_cts) {
		uart_cfg.flow_ctrl = UART_CFG_FLOW_CTRL_RTS_CTS;
	}
	return uart_configure(uart, &uart_cfg);
}
