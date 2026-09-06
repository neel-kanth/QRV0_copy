/*
 * This file is part of the UWB stack for linux.
 *
 * Copyright (c) 2020-2022 Qorvo US, Inc.
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
#ifndef __QM3X_H
#define __QM3X_H

#include <linux/types.h>
#include <linux/wait.h>

#include "qm3x_core.h"
#include "qm3x_transport.h"
#include "qm3x_bypass.h"

/* Max number of handler in the table. */
#define MAX_RX_HANDLERS (QM3X_TRANSPORT_MSG_MAX * QM3X_TRANSPORT_PRIO_COUNT)

/* Forward declarations to avoid lot of includes here. */
struct dentry;
struct device;

/**
 * struct qm3x - Core QM35 device structure.
 * @dev: Pointer to generic device holding sysfs attributes.
 * @transport_ops: Transport specific operations.
 * @transport_flags: Transport specific flags.
 * @rx_handlers: Handlers for event type received from the transport layer.
 * @bypass_data: Bypass API related data.
 * @lock: Spin-lock to protect `rx_handlers` access.
 * @dev_id: Logical id of the current device.
 * @worker_pid: Driver worker thread PID if any.
 * @transport_pid: Local transport thread PID if any.
 * @wait_state: Wait queue to synchronise with QM35's state.
 * @state: QM35's state.
 */
struct qm3x {
	struct device *dev;
	const struct qm3x_transport_ops *transport_ops;
	enum qm3x_transport_flags transport_flags;
	struct qm3x_transport_recv_handler rx_handlers[MAX_RX_HANDLERS];
	struct qm3x_bypass bypass_data;
	spinlock_t lock;
	int dev_id;
	pid_t worker_pid, transport_pid;
	wait_queue_head_t wait_state;
	enum qm3x_state state;
};

/**
 * struct qm3x_fw_version - QM35 firmware version.
 * @major: Major number of the version.
 * @minor: Minor number of the version.
 * @patch: Patch number of the version.
 * @rc: Release candidate number of the version.
 * @build_id: Build id (or timestamp) of the version.
 * @oem_major: Major number of the release version.
 * @oem_minor: Minor number of the release version.
 * @oem_patch: Patch number of the release version.
 */
struct qm3x_fw_version {
	u8 major;
	u8 minor;
	u8 patch;
	u8 rc;
	u64 build_id;
	u8 oem_major;
	u8 oem_minor;
	u8 oem_patch;
};

#define qm3x_fw_version_cmp(a, b)                                \
	((a)->major != (b)->major || (a)->minor != (b)->minor || \
	 (a)->patch != (b)->patch || (a)->rc != (b)->rc ||       \
	 (a)->build_id != (b)->build_id)

#define qm3x_fw_version_print(print_func, dev, header, v)                          \
	do {                                                                       \
		if ((v)->oem_major || (v)->oem_minor || (v)->oem_patch)            \
			print_func(                                                \
				dev,                                               \
				"%s: %u.%u.%urc%u_%llu (OEM version: %u.%u.%u)\n", \
				header, (v)->major, (v)->minor, (v)->patch,        \
				(v)->rc, (v)->build_id, (v)->oem_major,            \
				(v)->oem_minor, (v)->oem_patch);                   \
		else if ((v)->rc || (v)->build_id)                                 \
			print_func(dev, "%s: %u.%u.%urc%u_%llu\n", header,         \
				   (v)->major, (v)->minor, (v)->patch,             \
				   (v)->rc, (v)->build_id);                        \
		else                                                               \
			print_func(dev, "%s: %u.%u.%u\n", header, (v)->major,      \
				   (v)->minor, (v)->patch);                        \
	} while (0)

#endif /* __QM3X_H */
