/*
 * This file is part of the UWB stack for linux.
 *
 * Copyright (c) 2022 Qorvo US, Inc.
 *
 * This software is provided under the GNU General Public License, version 2
 * (GPLv2), as well as under a Qorvo commercial license.
 *
 * You may choose to use this software under the terms of the GPLv2 License,
 * version 2 ("GPLv2"), as published by the Free Software Foundation.
 * You should have received a copy of the GPLv2 along with this program.  If
 * not, see <http://www.gnu.org/licenses/>.
 *
 * This program is distributed under the GPLv2 in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GPLv2 for more
 * details.
 *
 * If you cannot meet the requirements of the GPLv2, you may not use this
 * software for any purpose without first obtaining a commercial license from
 * Qorvo. Please contact Qorvo to inquire about licensing terms.
 */
#ifndef QM3X_LOGS_H
#define QM3X_LOGS_H

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/sysfs.h>
#include <linux/types.h>

/* Forward declaration to avoid need to include qm3x.h here. */
struct qm3x;

#define QM3X_LOGS_MAX_PACKETS_STORED 1024

/**
 * enum qm3x_logs_cmd_id - Log command ids shared with firmware.
 * @QM3X_LOGS_DATA: A log trace, contains a string (Read only).
 * @QM3X_LOGS_SET_LEVEL: Ability to set a sources log level, returns log level set.
 * @QM3X_LOGS_GET_LEVEL: Ability to get a sources log level.
 * @QM3X_LOGS_GET_SOURCES: Get all sources of logs with their level.
 * @QM3X_LOGS_MAX: Count of log command types.
 */
enum qm3x_logs_cmd_id {
	QM3X_LOGS_DATA = 0,
	QM3X_LOGS_SET_LEVEL = 1,
	QM3X_LOGS_GET_LEVEL = 2,
	QM3X_LOGS_GET_SOURCES = 3,
	QM3X_LOGS_MAX
};

/**
 * struct qm3x_logs_list - List to store received logs or qtrace packets.
 * @bin_attr: Binary attribute to expose this list in sysfs.
 * @list: List storing received struct sk_buff.
 * @lock: Spin-lock to protect `list` access.
 * @count: Number of objects in list (max is QM3X_LOGS_MAX_PACKET_STORED).
 */
struct qm3x_logs_list {
	struct bin_attribute bin_attr;
	struct list_head list;
	spinlock_t lock;
	uint32_t count;
};

/**
 * struct qm3x_logs - QM35 Logs extension structure.
 * @qm35: Back-pointer to core QM35 instance structure.
 * @dev: Back-pointer to underlying device structure.
 * @logs: List to store logs (see struct qm3x_logs_data) and attribute.
 * @qtraces: List to store qtraces (see struct qm3x_logs_data) and attribute.
 * @file_mutex: Mutex protecting concurrent accesses.
 * @dev_list: Linked-list of struct qm3x_logs instances.
 *
 * This structure is allocated by notifier callback when a new QM35 device
 * was created. It maintains a back-pointer to the main device structure.
 */
struct qm3x_logs {
	struct qm3x *qm35;
	struct device *dev;
	struct qm3x_logs_list logs;
	struct qm3x_logs_list qtraces;
	struct mutex file_mutex;
	struct list_head dev_list;
};

#ifdef QM3X_LOGS_TESTS

#include "mocks/ku_base.h"
#define KU_NO_KMALLOC_MOCK
#include "mocks/ku_alloc_free.h"
#define KU_NO_ALLOC_SKB_MOCK
#include "mocks/ku_alloc_free_skb.h"
#include "mocks/ku_get_device.h"
#include "mocks/ku_notifier.h"
#define KU_NO_START_MOCK
#define KU_NO_STOP_MOCK
#define KU_NO_RESET_MOCK
#define KU_NO_POWER_MOCK
#define KU_NO_FW_UPD_MOCK
#define KU_NO_PROBE_MOCK
#define KU_NO_SET_IRQ_MOCK
#define KU_NO_WAIT_IRQ_MOCK
#define KU_NO_SPI_TRANSFER_MOCK
#include "mocks/ku_transport.h"

int ku_sysfs_create_bin_file(struct kobject *kobj,
			     struct bin_attribute *bin_attr);
#define sysfs_create_bin_file ku_sysfs_create_bin_file

/* Ensure modified functions aren't exported! */
#undef EXPORT_SYMBOL
#define EXPORT_SYMBOL(x)

#endif /* QM3X_LOGS_TESTS */

#endif /* QM3X_LOGS_H */
