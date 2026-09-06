/*
 * This file is part of the UWB stack for linux.
 *
 * Copyright (c) 2020-2021 Qorvo US, Inc.
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
#ifndef __QM3X_CORE_H
#define __QM3X_CORE_H

#include <linux/types.h>

/* Forward declarations to avoid lot of includes here. */
struct qm3x;
struct qm3x_transport;
struct device;

/**
 * enum qm3x_state - State of QM35 device.
 * @QM3X_STATE_UNKNOWN:
 * 	QM35 state is unknow, when starting the probe.
 * @QM3X_STATE_READY:
 * 	QM35 is ready.
 * @QM3X_STATE_ACTIVE:
 * 	QM35 is performing a communication or ranging.
 * @QM3X_STATE_ERROR:
 * 	QM35 needs to be reset.
 * @QM3X_STATE_BOOTED:
 * 	QM35 boot notification was received.
 */
enum qm3x_state {
	QM3X_STATE_UNKNOWN,
	QM3X_STATE_READY,
	QM3X_STATE_ACTIVE,
	QM3X_STATE_ERROR,
	QM3X_STATE_BOOTED,
};

/**
 * struct qm3x_uci_device_info - QM35 device information.
 * @uci_version: UCI API version.
 * @mac_version: UWB-MAC API version.
 * @phy_version: PHY version.
 * @uci_test_version: UCI TEST API version.
 * @vendor_length: Length of additional vendor information.
 * @vendor_data: Vendor specific data following this struct.
 *
 * If vendor data is expected, a buffer bigger than this structure must be used.
 */
struct qm3x_uci_device_info {
	u16 uci_version;
	u16 mac_version;
	u16 phy_version;
	u16 uci_test_version;
	u8 vendor_length;
	u8 vendor_data[];
} __packed;

struct qm3x *qm3x_alloc_device(struct device *dev, size_t priv_size,
			       const struct qm3x_transport *transport);
void qm3x_free_device(struct qm3x *qm35);

int qm3x_register_device(struct qm3x *qm35);
int qm3x_unregister_device(struct qm3x *qm35);
int qm3x_get_dev_id(struct qm3x *qm35);
struct device *qm3x_get_device(struct qm3x *qm35);

void qm3x_state_set(struct qm3x *qm35, enum qm3x_state device_state);
int qm3x_state_wait(struct qm3x *qm35, enum qm3x_state device_state);

#ifdef QM3X_CORE_TESTS
#define KU_NO_KMALLOC_MOCK
#include "mocks/ku_alloc_free.h"
#define KU_NO_POWER_MOCK
#define KU_NO_FREE_PACKET_MOCK
#define KU_NO_REGISTER_MOCK
#define KU_NO_UNREGISTER_MOCK
#define KU_NO_SEND_MOCK
#define KU_NO_SET_IRQ_MOCK
#define KU_NO_WAIT_IRQ_MOCK
#define KU_NO_SPI_TRANSFER_MOCK
#include "mocks/ku_transport.h"
#include "mocks/ku_wait_event.h"

/* The following wrapper MUST be set to ensure the core API don't use
 * functions that have their own tests suite. This will result in
 * conflicting type for test->priv structure. */

/* These ones will be declared in qm3x_ids.h, included AFTER qm3x_core.h
   in qm3x_core.c */
#define qm3x_new_id ku_qm3x_new_id
#define qm3x_remove_id ku_qm3x_remove_id

/* This one will be declared in qm3x_notifier.h, included AFTER qm3x_core.h
   in qm3x_core.c */
#define qm3x_notifier_notify ku_qm3x_notifier_notify

/* Ensure modified functions aren't exported! */
#undef EXPORT_SYMBOL
#define EXPORT_SYMBOL(x)

#endif /* QM3X_CORE_TESTS */

#endif /* __QM3X_CORE_H */
