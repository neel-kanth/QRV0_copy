/**
 * @file      hsspi.c
 *
 * @brief     Implementation for HSSPI driver
 *
 * @author    Qorvo Paris Applications
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */
#include "drivers/uwb/hsspi.h"

#define DT_DRV_COMPAT qorvo_hsspi

#include <drivers/uwb/hsspi_helpers.h>
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device_runtime.h>
LOG_MODULE_REGISTER(hsspi, 4);

// HOST
#define STC_WR (1 << 7)
#define STC_PRD (1 << 6)
#define STC_RD (1 << 5)

// SOC
#define STC_ODW (1 << 7) // Output Data Waiting. SoC -> Host data available.
#define STC_OA (1 << 6) // Output active. SoC -> Host transaction in progress.
#define STC_READY (1 << 5) // SoC Ready to for SPI transaction.
#define STC_ERR (1 << 4)

typedef struct stc_header_s {
	uint8_t flag;
	uint8_t ul;
	uint16_t length;
} STC_HEADER;

void hsspi_cs_gpio_set(const struct device *dev, bool val)
{
	const struct hsspi_driver_config *config = dev->config;
	const struct gpio_dt_spec *cs = &((config->bus).config.cs.gpio);
	gpio_pin_set_dt(cs, val);
}

void hsspi_reset_gpio_set(const struct device *dev, bool val)
{
	const struct hsspi_driver_config *config = dev->config;
	gpio_pin_set_dt(&config->reset, val);
}

void hsspi_reset(const struct device *dev, bool bootrom)
{
	if (bootrom) {
		hsspi_cs_gpio_set(dev, 1);
	}
	hsspi_reset_gpio_set(dev, 0);
	k_msleep(5);
	hsspi_reset_gpio_set(dev, 1);
	if (bootrom) {
		k_usleep(100);
		hsspi_cs_gpio_set(dev, 0);
	}
}

/* Awake chip by lowering the CS pin for 500 us then waiting for 3.5 ms. */
static int awake_chip(const struct device *dev)
{
	hsspi_cs_gpio_set(dev, true);
	k_usleep(WAKEUP_DURATION_US);
	hsspi_cs_gpio_set(dev, false);
	return 0;
}

static inline int bus_get(const struct device *dev)
{
	int ret = pm_device_runtime_get(dev);
	if (ret < 0)
		LOG_ERR("Could not PM resume on %s: %d", dev->name, ret);
	return ret;
}

static inline int bus_put_return(const struct device *dev, int result)
{
	int ret = pm_device_runtime_put(dev);
	if (ret < 0)
		LOG_ERR("Could not PM suspend on %s: %d", dev->name, ret);
	return ret ? ret : result;
}

int hsspi_preread(const struct device *dev)
{
	const struct hsspi_driver_config *config = dev->config;
	STC_HEADER stc_header_host, stc_header_soc;
	int ret;

	/* Check if there is something to read */
	if (gpio_pin_get_dt(&config->ss_irq) == 0) {
		return -EWOULDBLOCK;
	}

	struct spi_buf wr_prd[1] = {
	    {.buf = &stc_header_host, .len = sizeof(stc_header_host)}};
	struct spi_buf_set to_wr = {.buffers = wr_prd, .count = 1};
	struct spi_buf rd_prd[1] = {
	    {.buf = &stc_header_soc, .len = sizeof(stc_header_soc)}};
	struct spi_buf_set to_rd = {.buffers = rd_prd, .count = 1};

	/* Perform pre-read. */
	stc_header_host.flag = STC_PRD;
	stc_header_host.ul = 0;
	stc_header_host.length = 0;

	/* PM resume the SPI controller. */
	ret = bus_get(config->bus.bus);
	if (ret < 0)
		return ret;

	ret = spi_transceive_dt(&config->bus, &to_wr, &to_rd);
	if (ret < 0) {
		LOG_ERR("Unable to issue PRD transaction (%d)", ret);
		goto dev_put;
	}
	if (!(stc_header_soc.flag & STC_ODW)) {
		/* SOC was not ready to send data! Retry! */
		ret = -EAGAIN;
	} else {
		ret = stc_header_soc.length;
	}

dev_put:
	/* PM suspend the SPI controller. */
	return bus_put_return(config->bus.bus, ret);
}

int hsspi_read(const struct device *dev, enum qm3x_transport_msg_type *ul,
	       uint8_t *rx_buffer, uint16_t len)
{
	const struct hsspi_driver_config *config = dev->config;
	STC_HEADER stc_header_host, stc_header_soc;
	int ret;

	struct spi_buf wr_packets[2] = {{.buf = &stc_header_host,
					 .len = sizeof(stc_header_host)},
					{.buf = NULL, .len = len}};
	struct spi_buf_set to_wr = {.buffers = wr_packets, .count = 2};
	struct spi_buf rd_packets[2] = {{.buf = &stc_header_soc,
					 .len = sizeof(stc_header_soc)},
					{.buf = rx_buffer, .len = len}};
	struct spi_buf_set to_rd = {.buffers = rd_packets, .count = 2};

	/* Perform read. */
	stc_header_host.flag = STC_RD;
	stc_header_host.ul = 0;
	stc_header_host.length = 0;

	/* PM resume the SPI controller. */
	ret = bus_get(config->bus.bus);
	if (ret < 0)
		return ret;

	ret = spi_transceive_dt(&config->bus, &to_wr, &to_rd);
	if (ret < 0) {
		LOG_ERR("Unable to issue RD transaction (%d)", ret);
		goto dev_put;
	}
	*ul = stc_header_soc.ul;
	if (stc_header_soc.length != len) {
		/* PRD OK but length has changed for the RD!
		   Let caller retry itself with prd_done reset. */
		ret = -EPROTO;
	} else if (!(stc_header_soc.flag & STC_OA)) {
		/* PRD OK but bad SOC state during read!
		   Let caller retry itself without resetting prd_done so only
		   the RD is redo. */
		ret = -EAGAIN;
	}

dev_put:
	/* PM suspend the SPI controller. */
	return bus_put_return(config->bus.bus, ret);
}

int hsspi_write(const struct device *dev, enum qm3x_transport_msg_type ul,
		uint8_t *tx_buffer, uint16_t len)
{
	const struct hsspi_driver_config *config = dev->config;
	STC_HEADER stc_header_host, stc_header_soc;
	int ret;

	struct spi_buf wr_packets[2] = {{.buf = &stc_header_host,
					 .len = sizeof(stc_header_host)},
					{.buf = tx_buffer, .len = len}};
	struct spi_buf_set to_wr = {.buffers = wr_packets, .count = 2};
	struct spi_buf rd_packets[3] = {{.buf = &stc_header_soc,
					 .len = sizeof(stc_header_soc)},
					{.buf = NULL, .len = len}};
	struct spi_buf_set to_rd = {.buffers = rd_packets, .count = 2};

	stc_header_host.flag = STC_WR;
	stc_header_host.ul = ul;
	stc_header_host.length = len;

	/* PM resume the SPI controller. */
	ret = bus_get(config->bus.bus);
	if (ret < 0)
		return ret;

	ret = spi_transceive_dt(&config->bus, &to_wr, &to_rd);
	if (ret < 0) {
		LOG_ERR("Unable to issue WR transaction (%d)", ret);
		goto dev_put;
	}
	if (stc_header_soc.flag == 0x00 || stc_header_soc.flag == 0xff) {
		/* SOC may be in sleep mode. */
		awake_chip(dev);
		ret = -EBUSY;
	} else if (!(stc_header_soc.flag & STC_READY)) {
		/* SOC wasn't ready to receive! */
		ret = -EAGAIN;
	}

dev_put:
	/* PM suspend the SPI controller. */
	return bus_put_return(config->bus.bus, ret);
}

int hsspi_disable_irq(const struct device *dev)
{
	const struct hsspi_driver_config *config = dev->config;
	return gpio_pin_interrupt_configure_dt(&config->ss_irq,
					       GPIO_INT_DISABLE);
}

int hsspi_enable_irq(const struct device *dev)
{
	const struct hsspi_driver_config *config = dev->config;
	return gpio_pin_interrupt_configure_dt(&config->ss_irq,
					       GPIO_INT_LEVEL_ACTIVE);
}

void hsspi_setup_irq(const struct device *dev, hsspi_irq_callback_t cb,
		     void *user_data)
{
	struct hsspi_driver_data *data = dev->data;
	unsigned int key = irq_lock();
	data->irq_cb = cb;
	data->irq_cb_user_data = user_data;
	irq_unlock(key);
}

static void hsspi_isr(const struct device *dev, struct gpio_callback *cb,
		      uint32_t pins)
{
	struct hsspi_driver_data *data =
	    CONTAINER_OF(cb, struct hsspi_driver_data, ss_irq_cb_data);
	if (data->irq_cb) {
		hsspi_disable_irq(data->dev);
		data->irq_cb(dev, data->irq_cb_user_data);
	}
}

static int hsspi_init(const struct device *dev)
{
	const struct hsspi_driver_config *config = dev->config;
	struct hsspi_driver_data *data = dev->data;
	int ret, retpm;

	data->dev = dev;

	if (!device_is_ready(config->ss_irq.port)) {
		LOG_ERR("SS_IRQ GPIO not ready");
		return -ENODEV;
	}
	if (!device_is_ready(config->reset.port)) {
		LOG_ERR("RESET GPIO not ready");
		return -ENODEV;
	}
	if (!spi_is_ready_dt(&config->bus)) {
		LOG_ERR("SPI bus %s not ready", config->bus.bus->name);
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&config->ss_irq, GPIO_INPUT);
	if (ret < 0) {
		LOG_ERR("Could not configure SS_IRQ GPIO (%d)", ret);
		return ret;
	}
	ret = gpio_pin_configure_dt(&config->reset, GPIO_OUTPUT | GPIO_PULL_UP);
	if (ret < 0) {
		LOG_ERR("Could not configure RESET GPIO (%d)", ret);
		return ret;
	}

	/* Setup IRQ callback. */
	gpio_init_callback(&data->ss_irq_cb_data, hsspi_isr,
			   BIT(config->ss_irq.pin));
	gpio_add_callback(config->ss_irq.port, &data->ss_irq_cb_data);
	/* Other fields initialisation. */
	k_mutex_init(&data->awake_mutex);
	k_condvar_init(&data->awake_condvar);
	atomic_set(&data->irq_count, 0);
	/* Assert RESET gpio. It will be deasserted by hsspi_helpers_init(). */
	hsspi_reset_gpio_set(dev, 0);
	hsspi_helpers_init(data);
	/* Enable IRQ. */
	ret = hsspi_enable_irq(dev);
	if (ret) {
		LOG_ERR("Error %d: failed to configure interrupt on %s pin %d",
			ret, config->ss_irq.port->name, config->ss_irq.pin);
		return ret;
	}
	/* Enable runtime PM on the SPI controller. */
	retpm = pm_device_runtime_enable(config->bus.bus);
	if (retpm < 0) {
		LOG_ERR("Could not enable PM on %s: %d", config->bus.bus->name,
			retpm);
		return retpm;
	}
	return ret;
}

#define hsspi_INIT(i)                                                       \
	static struct hsspi_driver_data hsspi_data_##i = {                  \
	    .irq_cb = NULL,                                                 \
	};                                                                  \
                                                                            \
	static const struct hsspi_driver_config hsspi_config_##i = {        \
	    .bus = SPI_DT_SPEC_INST_GET(                                    \
		i, SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8), \
		0),                                                         \
	    .ss_irq = GPIO_DT_SPEC_INST_GET(i, ss_irq_gpios),               \
	    .reset = GPIO_DT_SPEC_INST_GET(i, reset_gpios),                 \
	};                                                                  \
                                                                            \
	DEVICE_DT_INST_DEFINE(i, hsspi_init, NULL, &hsspi_data_##i,         \
			      &hsspi_config_##i, POST_KERNEL, 90, NULL);

DT_INST_FOREACH_STATUS_OKAY(hsspi_INIT)
