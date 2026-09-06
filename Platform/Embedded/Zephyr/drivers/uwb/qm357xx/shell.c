/**
 * @file      shell.c
 *
 * @brief     Implementation for HSSPI driver shell test command
 *
 * @author    Qorvo Paris Applications
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 */
#include "drivers/uwb/shell.h"

#include <getopt.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/unistd.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

LOG_MODULE_REGISTER(hsspi_shell, 4);

#include "drivers/uwb/hsspi.h"
#include "drivers/uwb/hsspi_helpers.h"

static int usage(const struct shell *shell, char *arg0)
{
	const char usage_str[] = "%s: launch HSSPI driver tests\n\n"
				 "Options:\n"
				 "\t-h\tShow help\n"
				 "\t-d\tShow debug messages\n"
				 "\t-c count\tChange number of writes\n"
				 "\t-t ms\tChange delay between writes\n";
	shell_fprintf(shell, SHELL_NORMAL, usage_str, arg0);
	return 0;
}

int uwb_hsspi_tests_cmd(const struct shell *shell, size_t argc, char **argv)
{
	uint64_t total = 0;
	uint32_t minns = UINT32_MAX;
	uint32_t maxns = 0;
	uint32_t fails = 0;
	int count = 1000;
	long irqcount = 0;
	int opt, debug = 0, delay = 5;

	/* On zephyr, getopt must be reseted between multiple calls. */
	getopt_init();

	while ((opt = getopt(argc, argv, "c:dht:")) != -1) {
		switch (opt) {
		case 'c':
			count = atoi(optarg);
			break;
		case 'd':
			debug = 1;
			break;
		case 'h':
			return usage(shell, argv[0]);
		case 't':
			delay = atoi(optarg);
			break;
		default:
			shell_print(shell, "Invalid argument '-%c'\n", opt);
			usage(shell, argv[0]);
			return -1;
		}
	}
	irqcount = atomic_get(&hsspi_data->irq_count);
	if (debug) {
		shell_print(shell, "AWK supported: %s",
			    hsspi_data->awake_supported ? "true" : "false");
		shell_print(shell, "IRQ count before: %ld", irqcount);
		shell_print(shell, "Delay: %d", delay);
	}

	for (int i = 0; i < count; i++) {
		uint32_t start = k_cycle_get_32();
		int ret = hsspi_sync_write(QM3X_TRANSPORT_MSG_MAX,
					   "\001\002\003\004", 4);
		if (ret < 0) {
			fails++;
		} else {
			uint32_t curns =
			    k_cyc_to_ns_floor32(k_cycle_get_32() - start);
			total += curns;
			if (curns < minns)
				minns = curns;
			if (curns > maxns)
				maxns = curns;
		}
		k_msleep(delay); /* Ensure soc is sleeping. */
	}

	if (debug) {
		shell_print(shell, "AWK supported: %s",
			    hsspi_data->awake_supported ? "true" : "false");
		shell_print(shell, "IRQ count after: %ld",
			    atomic_get(&hsspi_data->irq_count));
	}
	count -= fails;
	shell_print(shell, "Succeeds: %u", count);
	shell_print(shell, "Fails: %u", fails);
	shell_print(shell, "Average ns: %llu", total / count);
	shell_print(shell, "Min ns: %u", minns);
	shell_print(shell, "Max ns: %u", maxns);
	shell_print(shell, "IRQ handled: %ld",
		    atomic_get(&hsspi_data->irq_count) - irqcount);
	return 0;
}
