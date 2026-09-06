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
#ifndef __QM3X_TRANSPORT_H
#define __QM3X_TRANSPORT_H

#include <linux/types.h>

/* QM3X_TRANSPORT_MSG_LOG packet exceed UCI_MAX_PACKET_SIZE */
#define QM35_MAX_PACKET_SIZE 1024

/* Forward declaration to avoid need to include qm3x.h here. */
struct qm3x;
struct qm3x_fw_version;

struct sk_buff;
struct spi_transfer;

/**
 * enum qm3x_transport_msg_type - Supported message types by transport ops
 * @QM3X_TRANSPORT_MSG_RESERVED_HSSPI: Special type for HSSPI transport
 * @QM3X_TRANSPORT_MSG_BOOTLOADER: Bootloader message
 * @QM3X_TRANSPORT_MSG_UCI: UCI message
 * @QM3X_TRANSPORT_MSG_COREDUMP: Coredump message
 * @QM3X_TRANSPORT_MSG_LOG: Log message
 * @QM3X_TRANSPORT_MSG_QTRACE: Qtrace message
 * @QM3X_TRANSPORT_MSG_AWAKE: Awake message
 * @QM3X_TRANSPORT_MSG_MAX: Number of message types
 *
 * Values of this enum match the ul_value of HSSPI protocol used by QM35
 * when connected through SPI.
 *
 * Note: When adding values to this enum, make sure to edit integer literals in
 * qm3x_transport_enum_success() accordingly.
 */
enum qm3x_transport_msg_type {
	QM3X_TRANSPORT_MSG_RESERVED_HSSPI,
	QM3X_TRANSPORT_MSG_BOOTLOADER,
	QM3X_TRANSPORT_MSG_UCI,
	QM3X_TRANSPORT_MSG_COREDUMP,
	QM3X_TRANSPORT_MSG_LOG,
	QM3X_TRANSPORT_MSG_QTRACE,
	QM3X_TRANSPORT_MSG_AWAKE,
	QM3X_TRANSPORT_MSG_MAX
} __attribute__((packed));

/**
 * enum qm3x_transport_priority - Supported handler priorities.
 * @QM3X_TRANSPORT_PRIO_HIGH: High priority handler.
 * @QM3X_TRANSPORT_PRIO_NORMAL: Normal priority handler.
 * @QM3X_TRANSPORT_PRIO_COUNT: Number of priorities.
 */
enum qm3x_transport_priority {
	QM3X_TRANSPORT_PRIO_HIGH,
	QM3X_TRANSPORT_PRIO_NORMAL,
	QM3X_TRANSPORT_PRIO_COUNT
} __attribute__((packed));

/**
 * enum qm3x_transport_events - Type of event received from lower level.
 * @QM3X_TRANSPORT_EVENT_IRQ: A QM35 interruption occured.
 * @QM3X_TRANSPORT_EVENT_NOTIFICATION: A UCI notification is available.
 * @QM3X_TRANSPORT_EVENT_RESPONSE: A UCI response is available.
 * @QM3X_TRANSPORT_EVENT_MAX: Number of event types.
 *
 * Note: no event is defined yet for the other message types.
 */
enum qm3x_transport_events {
	QM3X_TRANSPORT_EVENT_IRQ,
	QM3X_TRANSPORT_EVENT_NOTIFICATION,
	QM3X_TRANSPORT_EVENT_RESPONSE,
	QM3X_TRANSPORT_EVENT_MAX,
} __attribute__((packed));

/**
 * struct qm3x_transport_ops - QM35 transport operation.
 * @start: Activate the device (optional).
 * @stop: Deactivate the device (optional).
 * @send: Send a frame of a specific type to device.
 * @recv: Recv a frame from device, with type and flags.
 * @reset: Force a device reset.
 * @power: Force a power down/up.
 * @fw_update: Update the QM35 firmware.
 * @set_irq: Enable or disable IRQ.
 * @wait_irq: Wait for the IRQ line to be asserted.
 * @spi_transfer: Perform a SPI transfer.
 * @probe: Probe the QM35 device.
 *
 * The result of the recv function is the actual size of the buffer (positive)
 * or an error code (negative).
 */
struct qm3x_transport_ops {
	int (*start)(struct qm3x *qm35);
	void (*stop)(struct qm3x *qm35);
	int (*send)(struct qm3x *qm35, enum qm3x_transport_msg_type type,
		    const void *data, size_t size);
	ssize_t (*recv)(struct qm3x *qm35, void *data, size_t size,
			enum qm3x_transport_msg_type *type, int *flags);
	int (*reset)(struct qm3x *qm35, bool bootrom);
	int (*power)(struct qm3x *qm35, int on);
	int (*fw_update)(struct qm3x *qm35, struct qm3x_fw_version *current_ver,
			 u16 device_id, const char *fw_name);
	void (*set_irq)(struct qm3x *qm35, int on);
	int (*wait_irq)(struct qm3x *qm35, int timeout_ms);
	int (*spi_transfer)(struct qm3x *qm35, struct spi_transfer *xfer);
	int (*probe)(struct qm3x *qm35, char *infobuf, size_t len);
};

/**
 * enum qm3x_transport_flags - QM35 transport flags.
 * @QM3X_TRANSPORT_NO_PROBING: When set, no probing is allowed during register.
 * @QM3X_TRANSPORT_ASYNC_PROBING: When set, probing is performed asynchronously.
 */
enum qm3x_transport_flags {
	QM3X_TRANSPORT_NO_PROBING = 0b00000001,
	QM3X_TRANSPORT_ASYNC_PROBING = 0b00000010,
};

/**
 * struct qm3x_transport - QM35 transport information.
 * @name: Transport name used to generate device name.
 * @ops: Transport operations to use.
 * @flags: Transport specific flags.
 */
struct qm3x_transport {
	const char *name;
	const struct qm3x_transport_ops *ops;
	enum qm3x_transport_flags flags;
};

/**
 * typedef qm3x_transport_recv_cb - Callback type to for RX events.
 * @data: Private data of the callback handler.
 * @skb: The received packet to process.
 *
 * The provided @skb uses the @cb array field to provide:
 * * The transport packet type (in `skb->cb[0]`)
 * * The low-level transport reception flags (in `skb->cb[1]`)
 * * The low-level transport event value (in `skb->cb[2]`)
 */
typedef void (*qm3x_transport_recv_cb)(void *data, struct sk_buff *skb);

/**
 * struct qm3x_transport_recv_handler - QM35 transport receive handler info.
 * @data: Private data passed to callback.
 * @cb: Callback function.
 */
struct qm3x_transport_recv_handler {
	void *data;
	qm3x_transport_recv_cb cb;
};

/* Private transport API */
int qm3x_transport_start(struct qm3x *qm35);
void qm3x_transport_stop(struct qm3x *qm35);
int qm3x_transport_reset(struct qm3x *qm35, bool bootrom);
int qm3x_transport_power(struct qm3x *qm35, int on);

int qm3x_transport_send(struct qm3x *qm35, enum qm3x_transport_msg_type type,
			const void *data, size_t length);

int qm3x_transport_fw_update(struct qm3x *qm35,
			     struct qm3x_fw_version *current_ver, u16 device_id,
			     const char *fw_name);

void qm3x_transport_set_irq(struct qm3x *qm35, int on);
int qm3x_transport_wait_irq(struct qm3x *qm35, int timeout_ms);
int qm3x_transport_spi_transfer(struct qm3x *qm35, struct spi_transfer *xfer);
int qm3x_transport_probe(struct qm3x *qm35, char *infobuf, size_t len);

/* Public exported API for transport modules */
int qm3x_transport_event(struct qm3x *qm35, enum qm3x_transport_events event);

int qm3x_transport_register(struct qm3x *qm35,
			    enum qm3x_transport_msg_type type,
			    enum qm3x_transport_priority prio,
			    qm3x_transport_recv_cb callback, void *data);
int qm3x_transport_unregister(struct qm3x *qm35,
			      enum qm3x_transport_msg_type type,
			      enum qm3x_transport_priority prio,
			      qm3x_transport_recv_cb callback);

#define qm3x_transport_send_direct(q, t, d, l) \
	((q)->transport_ops->send)((q), (t), (d), (l))

#ifdef QM3X_TRANSPORT_TESTS

#include "mocks/ku_alloc_free_skb.h"

/* Ensure modified functions aren't exported! */
#undef EXPORT_SYMBOL
#define EXPORT_SYMBOL(x)

#endif /* QM3X_TRANSPORT_TESTS */

#endif /* __QM3X_TRANSPORT_H */
