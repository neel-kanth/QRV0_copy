/**
 * @file      uci_bridge.c
 *
 * @brief     UCI bridge command implementation.
 *
 * @author    Qorvo Paris Applications
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */

#include <stdlib.h>
#include <uci_bridge_app.h>
#include <shell_utils.h>

LOG_MODULE_REGISTER(uci_bridge, CONFIG_APP_LOG_LEVEL);

struct start_uwb_uci_bridge_data {
	struct k_work work_uwb_uci_bridge;
	size_t argc;
	char **argv;
	const struct shell *shell;
};

static char **copy_argv(int argc, char *argv[])
{
	/* Calculate the contiguous argv buffer size. */
	int length = 0, i;
	size_t total_argc = argc + 1;

	for (i = 0; i < argc; i++) {
		length += (strlen(argv[i]) + 1);
	}
	char **new_argv =
	    (char **)malloc((total_argc * sizeof(char *)) + length);
	if (!new_argv) {
		return NULL;
	}
	/* Copy argv into the contiguous buffer. */
	length = 0;
	for (i = 0; i < argc; i++) {
		new_argv[i] = &(
		    ((char *)new_argv)[(total_argc * sizeof(char *)) + length]);
		strcpy(new_argv[i], argv[i]);
		length += (strlen(argv[i]) + 1);
	}
	/* Insert NULL terminating pointer at the end of the pointer array. */
	new_argv[total_argc - 1] = NULL;
	return (new_argv);
}

static void start_uwb_uci_bridge(struct k_work *work)
{
	struct start_uwb_uci_bridge_data *start_data = CONTAINER_OF(
	    work, struct start_uwb_uci_bridge_data, work_uwb_uci_bridge);

	/* Wait the end of the shell. The system work queue thread has higher
	 * priority than the shell thread. So main_uci_bridge_app() may be called
	 * before the shell uninit function that disables uart irq. A workaround is
	 * waiting the end of the shell. */
	while (shell_ready(start_data->shell)) {
		k_msleep(1);
	}
	LOG_INF("The shell is stopped and the uci bridge is started on the "
		"zephyr_shell_uart. You can close your serial terminal and use "
		"the Cherry utilities on the same serial device.");
	/* Wait the log flush. */
	k_msleep(10);
	/* Start the uci bridge. */
	main_uci_bridge_app(start_data->argc, start_data->argv);
	free(start_data->argv);
	free(start_data);
}

int uwb_uci_bridge_cmd(const struct shell *shell, size_t argc, char **argv)
{
	struct start_uwb_uci_bridge_data *start_data = NULL;

	start_data = calloc(1, sizeof(*start_data));
	if (!start_data) {
		return -ENOMEM;
	}
	/* The initialization of the uci bridge must be called outside the shell
	 * thread and when the shell thread is stopped due to a call to
	 * uart_irq_callback_user_data_set(). A k_work is used. */
	k_work_init(&start_data->work_uwb_uci_bridge, start_uwb_uci_bridge);
	start_data->argc = argc;
	/* argv must be copied because the original one is on the shell thread stack.
	 * And this stack will be squashed when main_uci_bridge_app is called. */
	start_data->argv = copy_argv(argc, argv);
	if (!start_data->argv) {
		return -ENOMEM;
	}
	start_data->shell = shell;
	/* Stop the shell. */
	shell_stop_and_uart_acquire(shell);
	/* Start the uci bridge. */
	k_work_submit(&start_data->work_uwb_uci_bridge);
	return 0;
}
