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
#ifndef __QM3X_BYPASS_H
#define __QM3X_BYPASS_H

#include <linux/kernel.h>
#include <linux/ratelimit.h>
#include <linux/spinlock_types.h>
#include <linux/types.h>

#include "qm3x_transport.h"

/**
 * enum qm3x_bypass_events - Bypass event types.
 * @QM3X_BYPASS_IRQ: A QM35 interruption occurred.
 * @QM3X_BYPASS_NOTIFICATION: Notification event from the low level device.
 * @QM3X_BYPASS_RESPONSE: Response event from the low level device.
 * @QM3X_BYPASS_MAX: Count of event types.
 */
enum qm3x_bypass_events {
	QM3X_BYPASS_IRQ,
	QM3X_BYPASS_NOTIFICATION,
	QM3X_BYPASS_RESPONSE,
	QM3X_BYPASS_MAX
};

/**
 * enum qm3x_bypass_actions - Bypass control actions.
 * @QM3X_BYPASS_ACTION_RESET: Force reset of the low level device.
 * @QM3X_BYPASS_ACTION_FWUPD: Force firmware update.
 * @QM3X_BYPASS_ACTION_POWER: Force power on or off.
 * @QM3X_BYPASS_ACTION_MSG_TYPE: Change/read expected message type.
 * @QM3X_BYPASS_ACTION_IRQ: Enable or disable IRQ.
 * @QM3X_BYPASS_ACTION_WAIT_IRQ: Wait for IRQ line to be asserted.
 * @QM3X_BYPASS_ACTION_SPI_TRANSFER: Perform SPI transfer.
 * @QM3X_BYPASS_ACTION_MAX: Count of action types.
 */
enum qm3x_bypass_actions {
	QM3X_BYPASS_ACTION_RESET,
	QM3X_BYPASS_ACTION_FWUPD,
	QM3X_BYPASS_ACTION_POWER,
	QM3X_BYPASS_ACTION_MSG_TYPE,
	QM3X_BYPASS_ACTION_IRQ,
	QM3X_BYPASS_ACTION_WAIT_IRQ,
	QM3X_BYPASS_ACTION_SPI_TRANSFER,
	QM3X_BYPASS_ACTION_MAX
};

/**
 * typedef qm3x_bypass_listener_cb - Callback function type for qm3x_bypass_open()
 * @data: private data of the callback, set on qm3x_bypass_open() call
 * @event: the event to forward to the callback owner
 *
 * Return: Zero on success, else a negative error code.
 */
typedef int (*qm3x_bypass_listener_cb)(void *data,
				       enum qm3x_bypass_events event);

/**
 * struct qm3x_bypass_channel - Bypass channel structure.
 * @listener: Callback function called on bypass event.
 * @listener_data: Argument passed to @listener.
 * @packets: List of received packets.
 * @lock: Lock to protect packets list.
 * @count: Number of stored packets in the list.
 * @expected_type: Type of received packet to forward to bypass channel.
 * @irq_disabled: True if IRQ has been disabled via QM3X_BYPASS_ACTION_IRQ.
 * @max_pkt_rls: Rate-limiter for maximum stored packet warning.
 * @bypass: Back-pointer to associated struct qm3x_bypass.
 * @list: List of opened bypass channels.
 */
struct qm3x_bypass_channel {
	qm3x_bypass_listener_cb listener;
	void *listener_data;
	enum qm3x_transport_msg_type expected_type;
	bool irq_disabled;
	atomic_t count;
	struct list_head packets;
	spinlock_t lock;
	struct ratelimit_state max_pkt_rls;
	struct qm3x_bypass *bypass;
	struct list_head list;
};

static_assert(offsetof(struct qm3x_bypass_channel, expected_type) == 16);
/* expected_type is only one byte because packed enum. */
static_assert(offsetof(struct qm3x_bypass_channel, irq_disabled) == 17);
static_assert(offsetof(struct qm3x_bypass_channel, count) == 20);
static_assert(offsetof(struct qm3x_bypass_channel, packets) == 24);
/* I want to be sure lock start in first cache-line to avoid cache-miss
 * for all fields before and including lock. Other fields after lock
 * have offset depending on CONFIG_LOCK_DEBUG and cannot be tested.
 */
static_assert(offsetof(struct qm3x_bypass_channel, lock) < 64);

/**
 * typedef qm3x_bypass_handle - Bypass channel handle.
 *
 * A pointer to &struct qm3x_bypass_channel.
 */
typedef struct qm3x_bypass_channel *qm3x_bypass_handle;

/**
 * struct qm3x_bypass - Bypass data structure.
 * @lock: Lock to protect open and close operations.
 * @channels: List of opened channels.
 * @opened: Number of opened channels.
 */
struct qm3x_bypass {
	spinlock_t lock;
	struct list_head channels;
	atomic_t opened;
};

/**
 * qm3x_bypass_bound() - Check bound status of the bypass channel.
 * @hnd: The bypass channel handle to check.
 * @out: Optional pointer to buffer to write current expected packet type.
 *
 * Return: True if the bypass channel is bound to a transport packet type,
 *  else false.
 */
static inline int qm3x_bypass_bound(qm3x_bypass_handle hnd,
				    enum qm3x_transport_msg_type *out)
{
	if (hnd->expected_type != QM3X_TRANSPORT_MSG_MAX) {
		if (out)
			*out = hnd->expected_type;
		return true;
	}
	return false;
}

qm3x_bypass_handle qm3x_bypass_open(struct qm3x *qm35,
				    qm3x_bypass_listener_cb cb,
				    void *priv_data);

int qm3x_bypass_queue_check(qm3x_bypass_handle hnd);
int qm3x_bypass_close(qm3x_bypass_handle hnd);
int qm3x_bypass_send(qm3x_bypass_handle hnd, void *buffer, size_t len);
int qm3x_bypass_recv(qm3x_bypass_handle hnd, void __user *buffer, size_t len,
		     enum qm3x_transport_msg_type *type, int *flags);
int qm3x_bypass_control(qm3x_bypass_handle hnd, enum qm3x_bypass_actions action,
			long *param);

#ifdef QM3X_BYPASS_TESTS
#include "mocks/ku_base.h"
#define KU_NO_KZALLOC_MOCK
#include "mocks/ku_alloc_free.h"
#define KU_NO_ALLOC_SKB_MOCK
#include "mocks/ku_alloc_free_skb.h"
#define KU_NO_COPY_FROM_USER_MOCK
#include "mocks/ku_copy_user.h"
#include "mocks/ku_module_get_put.h"
#define KU_NO_SEND_MOCK
#include "mocks/ku_transport.h"

/* Ensure modified functions aren't exported! */
#undef EXPORT_SYMBOL
#define EXPORT_SYMBOL(x)

#endif /* QM3X_BYPASS_TESTS */

#endif /* __QM3X_BYPASS_H */
