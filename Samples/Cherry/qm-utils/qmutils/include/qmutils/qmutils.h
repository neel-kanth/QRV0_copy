/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "qmutils/qmchannel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* qm-utils APIs */
qmhandle qmu_qtrace_init(qmchannel_callback cb, void *cb_data);
int qmu_qtrace_setbuf(qmhandle hnd, void *buf, size_t len);
int qmu_qtrace_start(qmhandle hnd);
int qmu_qtrace_destroy(qmhandle hnd);
int qmu_qtrace_set_level(qmhandle hnd, int module_id, int level);

typedef int (*qmu_coredump_callback)(void *cb_data, bool status);
qmhandle qmu_coredump_init(qmchannel_callback read_cb,
			   qmu_coredump_callback end_cb, void *cb_data);
int qmu_coredump_setbuf(qmhandle hnd, void *buf, size_t len);
int qmu_coredump_start(qmhandle hnd);
int qmu_coredump_destroy(qmhandle hnd);
int qmu_coredump_force(qmhandle hnd);

qmhandle qmu_log_init(qmchannel_callback cb, void *cb_data);
int qmu_log_setbuf(qmhandle hnd, void *buf, size_t len);
int qmu_log_start(qmhandle hnd);
int qmu_log_destroy(qmhandle hnd);
int qmu_log_get_sources(qmhandle hnd);
int qmu_log_get_level(qmhandle hnd, int module_id);
int qmu_log_set_level(qmhandle hnd, int module_id, int level);

qmhandle qmu_uci_init(qmchannel_callback cb, void *cb_data);
int qmu_uci_start(qmhandle hnd);
int qmu_uci_destroy(qmhandle hnd);

qmhandle qmu_hsspi_init(qmchannel_callback cb, void *cb_data);
int qmu_hsspi_start(qmhandle hnd);
int qmu_hsspi_destroy(qmhandle hnd);

int qmu_reset(qmhandle hnd, bool bootrom);
int qmu_fwupdate(qmhandle hnd, char *name);
int qmu_disable_irq(qmhandle hnd);
int qmu_enable_irq(qmhandle hnd);
int qmu_wait_for_irq_line(qmhandle hnd, int timeout_ms);
int qmu_raw_transfer(qmhandle hnd, void *tx_buf, void *rx_buf, size_t rx_len);

#ifdef __cplusplus
}
#endif
