/**
 * @file      hsspi_helpers.c
 *
 * @brief     Asynchronous implementation for HSSPI driver
 *
 * @author    Qorvo Paris Applications
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */
#include "drivers/uwb/hsspi_helpers.h"

#include <drivers/uwb/bypass.h>
#include <drivers/uwb/hsspi.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(hsspi_helpers, CONFIG_APP_LOG_LEVEL);

// =========================================================================
// ====================== below are static declarations / vars =============
// =========================================================================

struct hsspi_driver_data *hsspi_data = NULL;

// =========================================================================
// ===================== below are wakeup related funcs ====================
// =========================================================================

static inline void awake_signal(void)
{
	if (hsspi_data->awake_supported) {
		k_mutex_lock(&hsspi_data->awake_mutex, K_FOREVER);
		k_condvar_signal(&hsspi_data->awake_condvar);
		k_mutex_unlock(&hsspi_data->awake_mutex);
	} else {
		hsspi_data->awake_supported = true;
		LOG_INF("Awake frame sending supported by FW");
	}
}

static inline void awake_wait(void)
{
	if (hsspi_data->awake_supported) {
		int ret;
		/* Wait for the spi AWAKE packet from the QM35. */
		k_mutex_lock(&hsspi_data->awake_mutex, K_FOREVER);
		ret = k_condvar_wait(&hsspi_data->awake_condvar,
				     &hsspi_data->awake_mutex,
				     K_MSEC((WAKEUP_DELAY_US * 2) / 1000));
		k_mutex_unlock(&hsspi_data->awake_mutex);
		if (ret == -EAGAIN) {
			LOG_ERR("Awake frame not received");
		}
	} else {
		k_usleep(WAKEUP_DELAY_US);
	}
}

// =========================================================================
// ====================== below are read related funcs =====================
// =========================================================================

static void do_hsspi_read(struct k_work *work)
{
	int len, ret, retry = 2;
	int retry_udelay = HSSPI_DELAY_US;
	uint8_t *rx_buffer;
	enum qm3x_transport_msg_type ul = 0;

	len = hsspi_preread(hsspi_data->dev);
	if (len <= 0)
		/* Issue while pre-reading exit and rely on SS_IRQ line to
		   trigger again this interrupt. */
		goto enable_irq;

	rx_buffer = malloc(len);
	if (rx_buffer == NULL)
		goto enable_irq;

	ret = hsspi_read(hsspi_data->dev, &ul, rx_buffer, len);
	while (ret == -EAGAIN && retry--) {
		/* Something went wrong while reading, but there is a chance if we retry. */
		k_usleep(retry_udelay);
		retry_udelay *= 2;
		/* Retry. */
		ret = hsspi_read(hsspi_data->dev, &ul, rx_buffer, len);
	}
	if (ret < 0)
		goto free_enable_irq;

	/* Early enable IRQ. */
	hsspi_enable_irq(hsspi_data->dev);
	/* Give the received buffer to the main qm3x_bypass event handler. */
	if (ul < QM3X_TRANSPORT_MSG_MAX && ret >= 0) {
		if (ul == QM3X_TRANSPORT_MSG_AWAKE)
			awake_signal();
		qm3x_bypass_event(&hsspi_data->qminstance, ul, rx_buffer, len);
	} else {
		free(rx_buffer);
	}
	return;

free_enable_irq:
	free(rx_buffer);
enable_irq:
	hsspi_enable_irq(hsspi_data->dev);
}

// =========================================================================
// ====================== below are write related funcs ====================
// =========================================================================

static void setup_node(hsspi_data_t *new_node, enum qm3x_transport_msg_type ul,
		       uint8_t *tx_buffer, uint16_t len)
{
	new_node->data =
	    tx_buffer; // only pass pointer, apps layer apply for heap?
	new_node->ul = ul;
	new_node->len = len;
	new_node->next = NULL;
}

/* Add node to end of TX list */
static void add_node(hsspi_data_t *new_node)
{
	k_mutex_lock(&hsspi_data->tx_list_lock, K_FOREVER);
	if (hsspi_data->tx_list_tail) {
		/* List not empty, add to tail. */
		hsspi_data->tx_list_tail->next = new_node;
	} else {
		/* List empty, set head. */
		hsspi_data->tx_list_head = new_node;
	}
	/* Always set tail to added node. */
	hsspi_data->tx_list_tail = new_node;
	k_mutex_unlock(&hsspi_data->tx_list_lock);
}

static void do_hsspi_write(struct k_work *work)
{
	int ret = 0;

	k_mutex_lock(&hsspi_data->tx_list_lock, K_FOREVER);
	while (hsspi_data->tx_list_head) {
		hsspi_data_t *temp = hsspi_data->tx_list_head;
		hsspi_data->tx_list_head = temp->next;
		if (!hsspi_data->tx_list_head)
			hsspi_data->tx_list_tail = NULL;
		k_mutex_unlock(&hsspi_data->tx_list_lock);

		ret = hsspi_write(hsspi_data->dev, temp->ul, temp->data,
				  temp->len);

		*temp->result = ret;
		k_sem_give(temp->wait_sent);
		k_mutex_lock(&hsspi_data->tx_list_lock, K_FOREVER);
	}
	k_mutex_unlock(&hsspi_data->tx_list_lock);
}

// =========================================================================
// ====================== below are work item funcs    =====================
// =========================================================================

static K_WORK_DEFINE(tx_work, do_hsspi_write);
static K_WORK_DEFINE(rx_work, do_hsspi_read);

static void ss_irq_cb(const struct device *dev, void *user_data)
{
	struct hsspi_driver_data *data = (struct hsspi_driver_data *)user_data;
	atomic_add(&data->irq_count, 1);
	k_work_submit(&rx_work);
}

void reset_uwbs(void)
{
	LOG_INF("UWB chip reset hard");
	hsspi_reset(hsspi_data->dev, false);
}

static int run_do_hsspi_write(hsspi_data_t *node)
{
	int ret;

	if (node->wait_sent) {
		/* Ensure execution in sysworkq. */
		add_node(node);
		k_work_submit(&tx_work);
		k_sem_take(node->wait_sent, K_FOREVER);
		ret = *node->result;

	} else {
		/* Already in sysworkq, Do it immediately. */
		ret = hsspi_write(hsspi_data->dev, node->ul, node->data,
				  node->len);
	}
	return ret;
}

int hsspi_sync_write(enum qm3x_transport_msg_type ul, uint8_t *tx_buffer,
		     uint16_t len)
{
	bool in_sysworkq = k_work_queue_thread_get(&k_sys_work_q) ==
			   k_current_get();
	struct k_sem wait_sem; /* local in-stack */
	hsspi_data_t node; /* local in-stack */
	int retry = 3;
	int retry_udelay = HSSPI_DELAY_US;
	int ret = 0;
	if (!hsspi_data)
		return -EINVAL;
	setup_node(&node, ul, tx_buffer, len);
	node.result = &ret;

	if (in_sysworkq) {
		node.wait_sent = NULL;
	} else {
		k_sem_init(&wait_sem, 0, 1);
		node.wait_sent = &wait_sem;
	}
	/* We retry outside sysworkq, allowing reception between retries. */
	ret = run_do_hsspi_write(&node);
	while ((ret == -EBUSY || ret == -EAGAIN) && retry--) {
		if (ret == -EBUSY && !in_sysworkq) {
			/* Chip was just awaken. Wait 3.5ms (or awake frame). */
			awake_wait();
		} else {
			/* Wait before retry (outside sysworkq). */
			k_usleep(retry_udelay);
			retry_udelay *= 2;
		}
		/* Retry. */
		ret = run_do_hsspi_write(&node);
	};
	return ret;
}

int hsspi_helpers_init(struct hsspi_driver_data *data)
{
	int ret = qm3x_bypass_init(&data->qminstance);
	if (ret)
		return ret;
	data->tx_list_head = NULL;
	data->tx_list_tail = NULL;
	k_mutex_init(&data->tx_list_lock);
	/* Save the instance, needed by helpers. */
	hsspi_data = data;
	/* And reset chip. */
	reset_uwbs();
	/* Finally, setup IRQ callback. */
	hsspi_setup_irq(data->dev, ss_irq_cb, data);
	return 0;
}
