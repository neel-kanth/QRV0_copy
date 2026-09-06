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
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/types.h>
#include <linux/version.h>

#include "qm3x_core.h"
#include "qm3x_logs.h"
#include "qm3x_notifier.h"
#include "qm3x_transport.h"

#define LOG_PRINTK 0 /* Set to 1 if you want log on printk when they arrive. */

/* Be very conservative by default. Only store up to 2000 skb. */
static u32 qm3x_qtraces_stored_max = 2000;
module_param_named(qtraces_stored_max, qm3x_qtraces_stored_max, uint, 0664);
MODULE_PARM_DESC(qtraces_stored_max,
		 "Maximum number of stored qtrace packets (unlimited if 0). "
		 "Default to 2000.");

static LIST_HEAD(logs_list);

/* Firmware source code located here :
 * qm-firmware/lib/log_backend_hsspi/src/log_backend_hsspi_ctrl.c
 *
 * Each packet got a header with packet id, length and an optional payload.
 *
 * There is 4 Commands:
 * QM3X_LOGS_DATA (Read only):
 * 	Payload contains only a string, length is given in header.
 * QM3X_LOGS_SET_LEVEL (Read / Write):
 * 	Set a log sources level, payload contains source id and level (2 bytes).
 * 	Firmware will answer the same message with value currently set.
 * QM3X_LOGS_GET_LEVEL (Read / Write):
 * 	Ask a log sources level, payload contains sources id (and can be longer)
 * 	Firmware will answer the logs source id and level.
 * QM3X_LOGS_GET_SOURCES (Read / Write)
 * 	Ask to get sources name and id, packet header is enough
 * 	Firmware will ansswer the number of sources, and a loop containing
 * 	source id, source level, source name (null terminated)
 */

/* Sysfs tree structure used:
 *
 * /sys/bus/spi/devices/spiX.Y: Base directory from QM35 device instance.
 * └─fwlogs: Main log entries (export all logs from all FW log modules).
 *
 * Notes:
 * - Reading fwlogs will cleanup memory and remove all stored logs.
 */

/**
 * struct qm3x_logs_header - Header use in transport logs (R/W).
 * @cmd_id: Command id (listed in qm3x_logs_cmd_id).
 * @body_size: Payload length (after the header).
 */
struct qm3x_logs_header {
	uint16_t cmd_id;
	uint16_t body_size;
};

/**
 * struct qm3x_logs_cmd - Command used in transport logs (write side).
 * @hdr: See struct qm3x_logs_header.
 * @source_id: Sources logs identifier given by QM3X_LOGS_GET_SOURCES.
 * @level: Target level of log asked to the firmware.
 *
 * When @hdr.cmd_id is
 * * QM3X_LOGS_GET_SOURCES, source_id and level are ignored
 * * QM3X_LOGS_GET_LEVEL, level is ignored
 */
struct qm3x_logs_cmd {
	struct qm3x_logs_header hdr;
	uint8_t source_id;
	uint8_t level;
};

/**
 * struct qm3x_logs_rsp - Reception structure used in transport logs.
 * @hdr: See struct qm3x_logs_header.
 * @data: Raw data sent by the firmware (size given in header).
 */
struct qm3x_logs_rsp {
	struct qm3x_logs_header hdr;
	char data[];
};

/**
 * struct qm3x_qtraces_header - Header use in transport qtrace (R/W).
 * @body_size: Packet length (including the header).
 * @type_id: Packet type id (listed in enum qtrace_dump_packet_type).
 * @module_id: Module id.
 *
 * For info packet type, the module_id is the first record module_id. It is
 * followed by level, module name string and another records starting with
 * module_id.
 *
 * For data packet type, the module_is is the module_id for all dumped qtraces
 * in that packet.
 *
 * Keep sync with qtrace_dump.h.
 */
struct qm3x_qtraces_header {
	uint16_t body_size;
	uint8_t type_id;
	uint8_t module_id;
};

#define QTRACE_PKT_HDR_SIZE sizeof(struct qm3x_qtraces_header)

/**
 * qm3x_logs_list_clear() - Remove all stored data.
 * @ll: Pointer to struct qm3x_logs_list to work on.
 * @qm35: QM35 instance used to free packets.
 */
static void qm3x_logs_list_clear(struct qm3x_logs_list *ll, struct qm3x *qm35)
{
	struct list_head *l = &ll->list;
	struct sk_buff *s;
	unsigned long flags;

	spin_lock_irqsave(&ll->lock, flags);
	while ((s = list_first_entry_or_null(l, struct sk_buff, list))) {
		list_del(&s->list);
		ll->count--;
		spin_unlock_irqrestore(&ll->lock, flags);
		kfree_skb(s);
		spin_lock_irqsave(&ll->lock, flags);
	}
	spin_unlock_irqrestore(&ll->lock, flags);
}

/**
 * qm3x_logs_list_remove_first() - Remove first packed stored.
 * @ll: Pointer to struct qm3x_logs_list to work on.
 * @qm35: QM35 instance used to free packets.
 * @max: Maximum number of packets to store.
 *
 * Remove the first packets stored in the list IF list is full (if count >=
 * @max). Called BEFORE a packet is added to the list.
 */
static void qm3x_logs_list_remove_first(struct qm3x_logs_list *ll,
					struct qm3x *qm35, unsigned max)
{
	struct sk_buff *first = NULL, *prev = NULL;
	unsigned long flags;

	spin_lock_irqsave(&ll->lock, flags);
	while (ll->count >= max) {
		prev = first;
		first = list_first_entry(&ll->list, struct sk_buff, list);
		list_del(&first->list);
		ll->count--;
		/* If more than one skb need to be freed, do it inside loop. */
		if (prev)
			kfree_skb(prev);
	}
	spin_unlock_irqrestore(&ll->lock, flags);
	/* If only one skb need to be freed, do it outside loop. */
	if (first)
		kfree_skb(first);
}

/**
 * qm3x_logs_list_append() - Add a new log at the end of the queue.
 * @ll: Pointer to struct qm3x_logs_list to work on.
 * @skb: packet to store
 *
 * We store directly the provided packet to avoid data copy. It will be freed
 * later, when an application open and read the log file.
 *
 * Returns: Zero on success or negative error.
 */
static void qm3x_logs_list_append(struct qm3x_logs_list *ll,
				  struct sk_buff *skb)
{
	unsigned long flags;

	spin_lock_irqsave(&ll->lock, flags);
	list_add_tail(&skb->list, &ll->list);
	ll->count++;
	spin_unlock_irqrestore(&ll->lock, flags);
}

/**
 * qm3x_qtraces_read() - Read qtraces.
 * @filp: The struct file instance.
 * @kobp: Device kernel object associated.
 * @bin_attr: Pointer to written binary attribute.
 * @buf: Pointer to application buffer.
 * @count: Buffer size.
 * @pos: Current offset.
 *
 * Copy all available qtrace packets to provided buffer.
 *
 * Returns: read size on success, else a negative error code.
 */
static ssize_t qm3x_qtraces_read(struct file *filp, struct kobject *kobp,
				 struct bin_attribute *bin_attr, char *buf,
				 loff_t pos, size_t count)
{
	struct qm3x_logs *qml =
		container_of(bin_attr, struct qm3x_logs, qtraces.bin_attr);
	struct qm3x_logs_list *ll = &qml->qtraces;
	struct list_head *l = &ll->list;
	struct sk_buff *skb;
	unsigned long flags;
	unsigned copied = 0;
	int error = 0;
	size_t remain = count;
	char __user *cur = buf;

	((void)pos); /* unused */

	spin_lock_irqsave(&ll->lock, flags);
	while ((skb = list_first_entry_or_null(l, struct sk_buff, list))) {
		if (remain < skb->len) {
			error = copied ? 0 : -ENOSPC;
			break; /* no space in buffer. */
		}
		/* Early delete from list. */
		list_del(&skb->list);
		ll->count--;
		/* Ensure producer thread can add new entries in list during
		 * copy and free. */
		spin_unlock_irqrestore(&ll->lock, flags);

		/* Copy this qtrace packet. */
		memcpy(cur, skb->data, skb->len);
		remain -= skb->len;
		cur += skb->len;
		copied++;

		/* Free packet. */
		kfree_skb(skb);

		/* Re-lock to continue for list reading. */
		spin_lock_irqsave(&ll->lock, flags);
	}
	spin_unlock_irqrestore(&ll->lock, flags);
	if (error)
		return error;
	return cur - buf;
}

/**
 * qm3x_qtraces_write() - Write qtraces.
 * @filp: The struct file instance.
 * @kobp: Device kernel object associated.
 * @bin_attr: Pointer to written binary attribute.
 * @buf: Pointer to application buffer.
 * @count: Buffer size.
 * @pos: Current offset.
 *
 * Send provided buffer directly to FW using the QTRACE message type.
 *
 * Returns: write size on success, else a negative error code.
 */
static ssize_t qm3x_qtraces_write(struct file *filp, struct kobject *kobp,
				  struct bin_attribute *bin_attr, char *buf,
				  loff_t pos, size_t count)
{
	struct qm3x_logs *qml =
		container_of(bin_attr, struct qm3x_logs, qtraces.bin_attr);
	ssize_t rc;

	mutex_lock(&qml->file_mutex);
	rc = qm3x_transport_send(qml->qm35, QM3X_TRANSPORT_MSG_QTRACE, buf,
				 count);
	mutex_unlock(&qml->file_mutex);
	if (rc < 0)
		return rc;
	return count;
}

/**
 * qm3x_qtraces_packet_recv() - Packet handler for QTRACE messages.
 * @data: Pointer to qm3x_logs structure.
 * @skb: QTRACE packet received.
 *
 * This is the registered QTRACE packet handler. It stores the received packet
 * in a packet list attached to the ``qtraces`` file in device sysfs.
 *
 * The ``qtraces`` file is also notified with sysfs_notify() to allow user-space
 * application to be informed when new QTRACE packet is available.
 */
static void qm3x_qtraces_packet_recv(void *data, struct sk_buff *skb)
{
	struct qm3x_logs *qml = (struct qm3x_logs *)data;
	struct qm3x_qtraces_header *qtrace_pkt;

	/* Need to be error prone for packet coming from FW. */
	if (skb->len < QTRACE_PKT_HDR_SIZE)
		goto freepkt;
	qtrace_pkt = (struct qm3x_qtraces_header *)skb->data;
	if (skb->len < qtrace_pkt->body_size)
		goto freepkt;
	/* If a limit is set through module parameter, keep only last received
	 * packets and remove oldest ones. */
	if (qm3x_qtraces_stored_max) {
		qm3x_logs_list_remove_first(&qml->qtraces, qml->qm35,
					    qm3x_qtraces_stored_max);
	}
	/* All packet will be processed by user-space application.
	 * Save newly received packet in the right list. */
	qm3x_logs_list_append(&qml->qtraces, skb);
	if (qml->qtraces.bin_attr.attr.name) {
		sysfs_notify(&qml->dev->kobj, NULL,
			     qml->qtraces.bin_attr.attr.name);
	}
	return;

freepkt:
	kfree_skb(skb);
	return;
}

/**
 * qm3x_logs_read_common() - Common function for log reading.
 * @qml: The instance pointer.
 * @buf: Userspace buffer where to put logs.
 * @count: Size of buffer.
 * @ppos: Current offset.
 *
 * If @parent is NULL, it is called by qm3x_logs_read(), the read callback of
 * "fwlogs" binary attribute file in device sysfs. So the provided buffer is
 * kernel space, not in user-space.
 *
 * Returns: Number of bytes added to buffer or negative error.
 */
static ssize_t qm3x_logs_read_common(struct qm3x_logs *qml, char *buf,
				     size_t count, loff_t *ppos)
{
	struct qm3x_logs_list *ll = &qml->logs;
	struct list_head *l = &ll->list;
	struct sk_buff *skb;
	unsigned long flags;
	unsigned copied = 0;
	int error = 0;
	size_t remain = count;
	char __user *cur = buf;

	spin_lock_irqsave(&ll->lock, flags);
	while ((skb = list_first_entry_or_null(l, struct sk_buff, list))) {
		struct qm3x_logs_rsp *log_pkt;
		log_pkt = (struct qm3x_logs_rsp *)skb->data;
		if (remain < log_pkt->hdr.body_size) {
			error = copied ? 0 : -ENOSPC;
			break; /* no space in buffer. */
		}

		/* Early delete from list. */
		list_del(&skb->list);
		ll->count--;
		/* Ensure producer thread can add new entries in list during
		 * copy and free. */
		spin_unlock_irqrestore(&ll->lock, flags);

		/* Copy this log entry. */
		memcpy(cur, log_pkt->data, log_pkt->hdr.body_size);
		/* Update sizes and position. */
		remain -= log_pkt->hdr.body_size;
		cur += log_pkt->hdr.body_size;
		copied++;

		/* Free packet. */
		kfree_skb(skb);

		/* Re-lock to continue for list reading. */
		spin_lock_irqsave(&ll->lock, flags);
	}
	spin_unlock_irqrestore(&ll->lock, flags);
	if (error)
		return error;
	*ppos += cur - buf;
	return cur - buf;
}

/**
 * qm3x_logs_read() - Read logs.
 * @filp: The struct file instance.
 * @kobp: Device kernel object associated.
 * @bin_attr: Pointer to written binary attribute.
 * @buf: Pointer to application buffer.
 * @count: Buffer size.
 * @pos: Current offset.
 *
 * Just call qm3x_logs_read_common() with NULL parent to read all available
 * logs.
 *
 * Returns: read size on success, else a negative error code.
 */
static ssize_t qm3x_logs_read(struct file *filp, struct kobject *kobp,
			      struct bin_attribute *bin_attr, char *buf,
			      loff_t pos, size_t count)
{
	struct qm3x_logs *qml =
		container_of(bin_attr, struct qm3x_logs, logs.bin_attr);
	return qm3x_logs_read_common(qml, buf, count, &pos);
}

/**
 * qm3x_logs_packet_recv() - Packet handler for QM3X_TRANSPORT_MSG_LOG messages.
 * @data: Pointer to qm3x_logs structure.
 * @skb: LOG packet received.
 */
static void qm3x_logs_packet_recv(void *data, struct sk_buff *skb)
{
	struct qm3x_logs *qml = (struct qm3x_logs *)data;
	struct device *dev = qml->dev;
	struct qm3x_logs_rsp *log_pkt = (struct qm3x_logs_rsp *)skb->data;

	/* Need to be error prone for packet coming from FW. */
	if (skb->len < sizeof(struct qm3x_logs_header))
		goto freepkt;
	if (skb->len <
	    (sizeof(struct qm3x_logs_header) + log_pkt->hdr.body_size))
		goto freepkt;

	switch (log_pkt->hdr.cmd_id) {
	case QM3X_LOGS_DATA:
		qm3x_logs_list_remove_first(&qml->logs, qml->qm35,
					    QM3X_LOGS_MAX_PACKETS_STORED);
		qm3x_logs_list_append(&qml->logs, skb);
#if LOG_PRINTK != 0
		dev_info(dev, "FW: %.*s\n", log_pkt->hdr.body_size,
			 log_pkt->data);
#endif
		if (qml->logs.bin_attr.attr.name) {
			sysfs_notify(&qml->dev->kobj, NULL,
				     qml->logs.bin_attr.attr.name);
		}
		return; /* Pkt will be freed on read */
	case QM3X_LOGS_GET_SOURCES:
	case QM3X_LOGS_SET_LEVEL:
	case QM3X_LOGS_GET_LEVEL:
		dev_warn(dev, "log response received %d without command sent\n",
			 log_pkt->hdr.cmd_id);
		break;
	default:
		dev_warn(dev, "unknown log command received %d\n",
			 log_pkt->hdr.cmd_id);
		break;
	}

freepkt:
	kfree_skb(skb);
	return;
}

static struct qm3x_logs *qm3x_logs_search(struct qm3x *qm35)
{
	/* We cannot rely anymore on the registered packet handler since it
	 * may be removed by qm3x_logs_online() if it fails. Need to lookup
	 * the logs_list instead now.
	 */
	struct qm3x_logs *qml;
	/* Search corresponding qm3x_logs structure. */
	list_for_each_entry (qml, &logs_list, dev_list) {
		if (qml->qm35 == qm35)
			break;
	}
	if (list_entry_is_head(qml, &logs_list, dev_list))
		return NULL; /* Not found. */
	return qml;
}

/**
 * qm3x_logs_init() - Initialise logs for specified QM35 core device.
 * @qm35: QM35 device instance
 *
 * Init local structure to handle incoming packets and register LOG and QTRACE
 * packets handlers using qm3x_transport_register() so all received packets of
 * these type will be handled by this sub-module.
 *
 * The newly allocated struct qm3x_logs is finally put in the logs_list to allow
 * instance management. After this function is called, the new instance is ready
 * to receive LOG and QTRACE packets.
 *
 * Returns: Zero or a negative error
 */
static int qm3x_logs_init(struct qm3x *qm35)
{
	struct device *dev = qm3x_get_device(qm35);
	struct qm3x_logs *qml;
	int rc = -ENOMEM;

	qml = kzalloc(sizeof(*qml), GFP_KERNEL);
	if (!qml) {
		dev_err(dev, "%s: Cannot allocate memory\n", THIS_MODULE->name);
		goto error;
	}
	qml->qm35 = qm35;
	qml->dev = dev;
	mutex_init(&qml->file_mutex);

	INIT_LIST_HEAD(&qml->qtraces.list);
	spin_lock_init(&qml->qtraces.lock);

	rc = qm3x_transport_register(qm35, QM3X_TRANSPORT_MSG_QTRACE,
				     QM3X_TRANSPORT_PRIO_NORMAL,
				     qm3x_qtraces_packet_recv, qml);
	if (rc) {
		dev_err(dev, "%s: Fail to register QTRACE transport handler\n",
			THIS_MODULE->name);
		goto err_qtraces;
	}

	INIT_LIST_HEAD(&qml->logs.list);
	spin_lock_init(&qml->logs.lock);

	rc = qm3x_transport_register(qm35, QM3X_TRANSPORT_MSG_LOG,
				     QM3X_TRANSPORT_PRIO_NORMAL,
				     qm3x_logs_packet_recv, qml);
	if (rc) {
		dev_err(dev, "%s: Fail to register LOG transport handler\n",
			THIS_MODULE->name);
		goto err_logs;
	}

	/* Save in local list */
	list_add_tail(&qml->dev_list, &logs_list);
	return 0;

err_logs:
	qm3x_transport_unregister(qm35, QM3X_TRANSPORT_MSG_QTRACE,
				  QM3X_TRANSPORT_PRIO_NORMAL,
				  qm3x_qtraces_packet_recv);
err_qtraces:
	kfree(qml);
error:
	dev_err(dev, "%s: Failed to initialize (%d)\n", THIS_MODULE->name, rc);
	return rc;
}

/**
 * qm3x_logs_online() - Initialise fwlogs and qtraces files for specified QM35 device.
 * @qm35: QM35 device instance
 *
 * Finish initialization of the after QM35 device is online. It creates ``fwlogs` and
 * ``qtraces`` in device root directory in sysfs.
 *
 * Then, it sends a LOG command to retrieve QM35 FW log modules and constructs
 * files tree in ``/sys/kernel/debug/uwb/DEVNAME/`` according the received LOG
 * INFO response.
 *
 * Returns: Zero or a negative error
 */
static int qm3x_logs_online(struct qm3x *qm35)
{
	struct qm3x_logs *qml = qm3x_logs_search(qm35);
	struct device *dev;
	int rc = -EINVAL;

	if (!qml) {
		/* Another module have registered this message type.
		 * Do nothing for this QM35 instance. */
		dev = qm3x_get_device(qm35);
		dev_err(dev, "%s: Cannot get associated qm3x_logs instance!\n",
			THIS_MODULE->name);
		return rc;
	}
	dev = qml->dev;

	/* Create qtraces file. */
	sysfs_bin_attr_init(&qml->qtraces.bin_attr);
	qml->qtraces.bin_attr.size = 0;
	qml->qtraces.bin_attr.read = qm3x_qtraces_read;
	qml->qtraces.bin_attr.write = qm3x_qtraces_write;
	qml->qtraces.bin_attr.private = qml;
	qml->qtraces.bin_attr.attr.mode = 0644; /* RO */
	qml->qtraces.bin_attr.attr.name = "qtraces";
	rc = sysfs_create_bin_file(&dev->kobj, &qml->qtraces.bin_attr);
	if (rc) {
		dev_err(dev,
			"%s: Unable to create 'qtraces' file in device sysfs\n",
			THIS_MODULE->name);
		goto err_qtraces;
	}

	/* Create main log file. */
	sysfs_bin_attr_init(&qml->logs.bin_attr);
	qml->logs.bin_attr.size = 0;
	qml->logs.bin_attr.read = qm3x_logs_read;
	qml->logs.bin_attr.private = qml;
	qml->logs.bin_attr.attr.mode = 0444; /* RO */
	qml->logs.bin_attr.attr.name = "fwlogs";
	rc = sysfs_create_bin_file(&dev->kobj, &qml->logs.bin_attr);
	if (rc) {
		dev_err(dev,
			"%s: Unable to create 'fwlogs' file in device sysfs\n",
			THIS_MODULE->name);
		goto err_main_log;
	}

	return 0;

err_main_log:
	qml->logs.bin_attr.attr.name = NULL;
	sysfs_remove_bin_file(&dev->kobj, &qml->qtraces.bin_attr);
err_qtraces:
	qml->qtraces.bin_attr.attr.name = NULL;
	/* Ensure no more packets can be received (no file to read them) */
	qm3x_transport_unregister(qm35, QM3X_TRANSPORT_MSG_LOG,
				  QM3X_TRANSPORT_PRIO_NORMAL,
				  qm3x_logs_packet_recv);
	qm3x_transport_unregister(qm35, QM3X_TRANSPORT_MSG_QTRACE,
				  QM3X_TRANSPORT_PRIO_NORMAL,
				  qm3x_qtraces_packet_recv);
	dev_err(dev, "%s: Failed to finish initialization (%d)\n",
		THIS_MODULE->name, rc);
	return rc;
}

/**
 * qm3x_logs_deinit() - Cleanup logs for specified QM35 core device.
 * @qm35: QM35 device instance
 *
 * It removes all the files from the ``/sys/kernel/debug/uwb/DEVNAME`` tree,
 * removes the ``fwlogs`` and ``qtraces`` files from device root sysfs directory,
 * and frees all unconsumed LOG and QTRACE packets.
 *
 * It will also call ``qm3x_transport_unregister()`` to unregister the LOG and
 * QTRACE message handlers in transport API.
 */
static void qm3x_logs_deinit(struct qm3x *qm35)
{
	struct device *dev = qm3x_get_device(qm35);
	struct qm3x_logs *qml = qm3x_logs_search(qm35);

	if (!qml) {
		/* Another module have registered this message type.
		 * Do nothing for this QM35 instance. */
		dev_err(dev, "%s: Cannot get associated LOG instance!\n",
			THIS_MODULE->name);
		return;
	}

	/* Remove from list. */
	list_del(&qml->dev_list);

	/* Remove all files. */
	if (qml->logs.bin_attr.attr.name)
		sysfs_remove_bin_file(&dev->kobj, &qml->logs.bin_attr);
	if (qml->qtraces.bin_attr.attr.name)
		sysfs_remove_bin_file(&dev->kobj, &qml->qtraces.bin_attr);

	/* Remove transport callback */
	qm3x_transport_unregister(qm35, QM3X_TRANSPORT_MSG_LOG,
				  QM3X_TRANSPORT_PRIO_NORMAL,
				  qm3x_logs_packet_recv);
	qm3x_transport_unregister(qm35, QM3X_TRANSPORT_MSG_QTRACE,
				  QM3X_TRANSPORT_PRIO_NORMAL,
				  qm3x_qtraces_packet_recv);

	/* Free all remaining packets. */
	mutex_lock(&qml->file_mutex);
	qm3x_logs_list_clear(&qml->logs, qml->qm35);
	qm3x_logs_list_clear(&qml->qtraces, qml->qm35);
	mutex_unlock(&qml->file_mutex);
	mutex_destroy(&qml->file_mutex);

	/* Free instance. */
	kfree(qml);
}

/**
 * qm3x_logs_notifier() - Callback function for struct notifier_block.
 * @nb: The notifier_block.
 * @action: The notifier event.
 * @data: The data provided by the notifier.
 *
 * This notifier callback function handles the create/destroy of the sysfs files
 * according the new/delete events from QM35 core for all QM35 device instances.
 *
 * It calls ``qm3x_logs_init()`` to instantiate the required logs and qtraces
 * structure when a NEW event is received.
 *
 * It calls ``qm3x_logs_online()`` to create all required sysfs files when an
 * ONLINE event is received.
 *
 * If calls ``qm3x_logs_deinit()`` to revert what previous function
 * made when a DELETE event is received.
 *
 * Context: Kernel thread context.
 * Return: 0 on success, else -EINVAL if @data is NULL or if the @action is
 * unknown, else the qm3x_logs_init() error code.
 */
static int qm3x_logs_notifier(struct notifier_block *nb, unsigned long action,
			      void *data)
{
	enum qm3x_notifier_events event = action;
	struct qm3x *qm35 = data;
	int rc = 0;

	if (!qm35)
		return notifier_from_errno(-EINVAL);

	switch (event) {
	case QM3X_NOTIFIER_EVENT_NEW:
		/* Ignore the return value. We don't want to stop notifier chain. */
		qm3x_logs_init(qm35);
		break;
	case QM3X_NOTIFIER_EVENT_ONLINE:
		/* Ignore the return value. We don't want to stop notifier chain. */
		qm3x_logs_online(qm35);
		break;
	case QM3X_NOTIFIER_EVENT_DELETE:
		qm3x_logs_deinit(qm35);
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return notifier_from_errno(rc);
}

static struct notifier_block nb = {
	.notifier_call = qm3x_logs_notifier,
	.next = NULL,
	.priority = 0,
};

/**
 * qm3x_logs_register_notifier() - Install QM35 notifier callback.
 *
 * Called when module is loaded and calls qm3x_register_notifier() to get
 * notified when QM35 device instance is modified.
 *
 * Context: User context.
 * Return: 0 on success, else qm3x_register_notifier() error code.
 */
static int qm3x_logs_register_notifier(void)
{
	return qm3x_register_notifier(&nb);
}

/**
 * qm3x_logs_unregister_notifier() - Remove logs and QM35 notifier callback.
 *
 * Called when module is unloaded and calls qm3x_unregister_notifier() to
 * remove the notifier callback. It also calls qm3x_logs_deinit() for all
 * known QM35 instances to remove all files and free allocated structures.
 *
 * Context: User context.
 * Return: 0 on success, else qm3x_unregister_notifier() error code.
 */
static int qm3x_logs_unregister_notifier(void)
{
	struct qm3x_logs *cur, *n;

	list_for_each_entry_safe (cur, n, &logs_list, dev_list) {
		qm3x_logs_deinit(cur->qm35);
	}
	return qm3x_unregister_notifier(&nb);
}

#ifndef QM3X_LOGS_TESTS

static int __init qm3x_logs_module_init(void)
{
	return qm3x_logs_register_notifier();
}

static void __exit qm3x_logs_module_exit(void)
{
	qm3x_logs_unregister_notifier();
}
module_init(qm3x_logs_module_init);
module_exit(qm3x_logs_module_exit);

#ifdef GITVERSION
MODULE_VERSION(GITVERSION);
#endif
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Romuald Després <romuald.despres@qorvo.com>");
MODULE_DESCRIPTION("Qorvo QM35 Logs driver");
MODULE_ALIAS("qm35_logs");

#endif /* !QM3X_LOGS_TESTS */
