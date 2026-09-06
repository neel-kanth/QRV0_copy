/**
 * @file      hsspi.h
 *
 * @brief     Synchronous HSSPI API
 *
 * @author    Qorvo Paris Applications
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */

#pragma once

#include <drivers/uwb/bypass.h>
#include <stddef.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/sys/atomic.h>

/* Timings*/
#define HSSPI_DELAY_US 100
#define HSSPI_RETRY_COUNT 3
#define WAKEUP_DURATION_US 500
#define WAKEUP_DELAY_US 3000

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback function type hsspi_irq_callback_t
 *
 * Callback called when the UWBS has a packet to send to the HOST.
 * This callback is called in interrupt context.
 *
 * @param dev The hsspi device to use.
 * @param user_data Pointer to the user_data previously registered with
 *  hsspi_setup_irq().
 */
typedef void (*hsspi_irq_callback_t)(const struct device *dev, void *user_data);

/* Tx data queue element definition */
typedef struct hsspi_data {
	struct hsspi_data *next;
	uint8_t *data;
	uint16_t len;
	enum qm3x_transport_msg_type ul;
	struct k_sem *wait_sent;
	int *result;
} hsspi_data_t;

struct hsspi_driver_data {
	const struct device *dev;
	struct gpio_callback ss_irq_cb_data;
	hsspi_irq_callback_t irq_cb;
	void *irq_cb_user_data;
	int state;
	unsigned int speed_hz;
	/* TX queue */
	hsspi_data_t *tx_list_head;
	hsspi_data_t *tx_list_tail;
	struct k_mutex tx_list_lock;
	/* Awake support */
	atomic_t irq_count;
	struct k_mutex awake_mutex;
	struct k_condvar awake_condvar;
	bool awake_supported;
	/* Include required qm3x instance for bypass implem. */
	struct qm3x qminstance;
};

extern struct hsspi_driver_data *hsspi_data;

struct hsspi_driver_config {
	const struct spi_dt_spec bus;
	const struct gpio_dt_spec ss_irq;
	const struct gpio_dt_spec reset;
};

/**
 * @brief Manually control the HSSPI Chip Select pin.
 *
 * This pin is also controlled by the driver, this function
 * should be used only for specific purpose, for instance, to reset the UWBS
 * and make it entering into the bootrom code (for FW upgrade).
 *
 * @param dev The hsspi device to use.
 * @param val `true` to assert the CS pin, `false` to deassert it.
 */
void hsspi_cs_gpio_set(const struct device *dev, bool val);

/**
 * @brief Control the UWB reset pin.
 *
 * @param dev The hsspi device to use.
 * @param val `true` to assert the RESET pin, `false` to deassert it.
 */
void hsspi_reset_gpio_set(const struct device *dev, bool val);

/**
 * @brief Reset the UWB chip.
 *
 * @param dev The hsspi device to use.
 * @param bootrom True to reset and enter bootrom code.
 */
void hsspi_reset(const struct device *dev, bool bootrom);

/**
 * @brief Issue a pre-read command. Each-pre-read shall be followed by a read.
 *
 * To read data from UWBS to HOST, the HSSPI requires 2 transactions. The first
 * one is a pre-read transaction. This transaction retrieves the amount of data to read.
 *
 * This function immediatelly return if there is nothing to read.
 *
 * @param dev The hsspi device to use.
 * @returns Length in bytes of the data to read in case of success.
 * @returns negative errno error code in case of failure.
 */
int hsspi_preread(const struct device *dev);

/**
 * @brief Read a total of len data from the HSSPI and fill rx_buffer
 * buffer with the read data.
 *
 * Provided rx_buffer size MUST be greater or equal to len.
 *
 * This Read command shall be preceded by a pre-read command.
 *
 * @param dev The hsspi device to use.
 * @param ul The UL protocol value read from the transaction
 * @param rx_buffer Buffer to fill with the read data.
 * This buffer shall be previously allocated by the caller.
 * @param len Total amount in bytes of data to read from the HSSPI.
 * This value shall correspond to the return value of the hsspi_preread call.
 * @returns Length in bytes of the read data in case of success.
 * @returns negative errno error code in case of failure.
 */
int hsspi_read(const struct device *dev, enum qm3x_transport_msg_type *ul,
	       uint8_t *rx_buffer, uint16_t len);

/**
 * @brief write a total of len bytes from the HSSPI and fill rx_buffer
 * buffer with the read data.
 *
 * This function blocks for at most 10ms if the UWBS is not ready to receive a packet.
 * Most of the time, the UWBS is ready to receive a packet.
 *
 * @param dev The hsspi device to use.
 * @param ul The UL protocol value to use for the transaction
 * @param tx_buffer Buffer to send to the UWBS.
 * @param len Size in bytes of the allocated buffer.
 * @returns 0 If successful.
 * @returns negative errno error code in case of failure.
 */
int hsspi_write(const struct device *dev, enum qm3x_transport_msg_type ul,
		uint8_t *tx_buffer, uint16_t len);

/**
 * @brief Register the IRQ read callback.
 *
 * Register a callback which is called when the UWBS has a buffer to send to the HOST.
 *
 * @param dev The hsspi device to use.
 * @param cb The callback to register. The callback will be called with the provided user_data as
 * parameter.
 * @param user_data user data pointer provided in the callback.
 */
void hsspi_setup_irq(const struct device *dev, hsspi_irq_callback_t cb,
		     void *user_data);

/**
 * @brief Enable the IRQ.
 * @param dev The hsspi device to use.
 * @returns 0 in case of success.
 * @returns errno error code in case of failure.
 */
int hsspi_enable_irq(const struct device *dev);

/**
 * @brief Disable the IRQ.
 * @param dev The hsspi device to use.
 * @returns 0 in case of success.
 * @returns errno error code in case of failure.
 */
int hsspi_disable_irq(const struct device *dev);

#ifdef __cplusplus
}
#endif
