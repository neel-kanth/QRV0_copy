/**
 * @file      qsoc_flash_bridge.c
 *
 * @brief     Example for mcu host to act as a bridge between the QSoC-Flash
 *            tool and the QM35 boot ROM.
 *
 * @author    Qorvo Paris Applications
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */

#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/pm/device_runtime.h>
#include <drivers/uwb/hsspi.h>

LOG_MODULE_REGISTER(qsoc_flash_bridge, CONFIG_APP_LOG_LEVEL);

#define FRAME_DELIMITER 0x7e
#define ESCAPE_CHARACTER 0x7d
#define ESCAPED_FRAME_DELIMITER 0x5e
#define ESCAPED_ESCAPE_CHARACTER 0x5d

#define CMD_NOP 0x01
#define CMD_EXIT_FLASH_MODE 0x02
#define CMD_TRANSFER 0x03
#define CMD_SET_CS 0x04
#define CMD_RESET 0x05
#define CMD_READ_IRQ 0x06

#define SERIAL_BUF_SIZE 4096

struct qsoc_flash_bridge_ctx {
	struct k_work bridge_init_work;
	struct k_work shell_init_work;
	struct k_work rx_process_work;
	const struct device *uart_dev;
	const struct device *hsspi_dev;
	const struct hsspi_driver_config *hsspi_config;
	const struct shell *shell;
	uint8_t *uart_rx_buf;
	uint8_t *uart_tx_buf;
	int uart_rx_buf_pos;
	int uart_rx_buf_len;
	int uart_tx_buf_pos;
	int uart_tx_buf_len;
	bool delimiter_received;
	int empty_cmd_count;
};

static void shell_init_from_work(struct k_work *shell_init_work)
{
	struct qsoc_flash_bridge_ctx *ctx = CONTAINER_OF(
	    shell_init_work, struct qsoc_flash_bridge_ctx, shell_init_work);
	bool log_backend = CONFIG_SHELL_BACKEND_SERIAL_LOG_LEVEL > 0;
	uint32_t level =
	    (CONFIG_SHELL_BACKEND_SERIAL_LOG_LEVEL > LOG_LEVEL_DBG) ?
		CONFIG_LOG_MAX_LEVEL :
		CONFIG_SHELL_BACKEND_SERIAL_LOG_LEVEL;
	int ret;

	/* Disable the QSOC-Flash bridge UART interrupts. */
	uart_irq_rx_disable(ctx->uart_dev);

	/* Suspend QM35 and re-enable interrupts. */
	ret = pm_device_runtime_put(ctx->hsspi_config->bus.bus);
	if (ret < 0)
		LOG_ERR("Failed to suspend QM35 device: %d", ret);
	ret = hsspi_enable_irq(ctx->hsspi_dev);
	if (ret < 0)
		LOG_ERR("Failed to enable QM35 interrupts: %d", ret);

	/* Restart the shell. */
	ret = shell_init(shell_backend_uart_get_ptr(), ctx->uart_dev,
			 shell_backend_uart_get_ptr()->ctx->cfg.flags,
			 log_backend, level);
	if (ret < 0)
		LOG_ERR("Failed to reinitialize shell: %d", ret);

	/* Clean up the QSoC-Flash bridge context. */
	k_free(ctx->uart_rx_buf);
	k_free(ctx->uart_tx_buf);
	k_free(ctx);
}

static void shell_reinit_trigger(struct qsoc_flash_bridge_ctx *ctx)
{
	int ret;

	ret = k_work_submit(&ctx->shell_init_work);
	if (ret < 0) {
		LOG_ERR("Failed to submit shell init work: %d", ret);
	}
}

/**
 * @brief Escape data for transmission over an UART.
 *
 * @param out_buf Pointer to the buffer to store the escaped data.
 * @param out_size Size of the output buffer.
 * @param out_offset Start position in the output buffer.
 * @param buf Pointer to the data to be escaped.
 * @param len Length of the data.
 *
 * @return Number of bytes written on success, negative error code on failure.
 */
static int uart_escape(uint8_t *out_buf, unsigned int out_size,
		       unsigned int out_offset, const uint8_t *buf,
		       unsigned int len)
{
	bool in_place = buf >= out_buf && buf < out_buf + out_size;
	unsigned int in_offset = buf - out_buf;
	int i, o;

	/*
	 * When escaping data in place, make sure that the output cursor does
	 * not catch up with the input cursor.
	 */
	for (i = 0, o = out_offset;
	     i < len && o < out_size && (!in_place || o <= i + in_offset);
	     i++, o++) {
		if (buf[i] == FRAME_DELIMITER) {
			out_buf[o++] = ESCAPE_CHARACTER;
			if (o >= out_size || (in_place && o > i + in_offset))
				return -ENOMEM;
			out_buf[o] = ESCAPED_FRAME_DELIMITER;
		} else if (buf[i] == ESCAPE_CHARACTER) {
			out_buf[o++] = ESCAPE_CHARACTER;
			if (o >= out_size || (in_place && o > i + in_offset))
				return -ENOMEM;
			out_buf[o] = ESCAPED_ESCAPE_CHARACTER;
		} else {
			out_buf[o] = buf[i];
		}
	}
	/*
	 * If the loop exits before reaching the end of the data, it means that
	 * the output buffer is too small.
	 */
	if (i < len)
		return -ENOMEM;

	return o - out_offset;
}

/**
 * @brief Unescape data received from an UART.
 *
 * @param buf Pointer to the data to be unescaped.
 * @param len Length of the data.
 *
 * @return Number of bytes written on success, negative error code on failure.
 */
static int uart_unescape(uint8_t *buf, unsigned int len)
{
	int i, j;

	for (i = 0, j = 0; i < len; i++, j++) {
		if (buf[i] == ESCAPE_CHARACTER) {
			i++;
			if (buf[i] == ESCAPED_FRAME_DELIMITER)
				buf[j] = FRAME_DELIMITER;
			else if (buf[i] == ESCAPED_ESCAPE_CHARACTER)
				buf[j] = ESCAPE_CHARACTER;
			else
				return -EPROTO;
		} else if (buf[i] == FRAME_DELIMITER) {
			return -EPROTO;
		} else {
			buf[j] = buf[i];
		}
	}

	return j;
}

static int uart_send_resp(struct qsoc_flash_bridge_ctx *ctx, const uint8_t cmd,
			  const uint8_t status, const uint8_t *payload,
			  const int payload_len)
{
	uint8_t header[4] = {cmd, (payload_len + 1) & 0xff,
			     (payload_len + 1) >> 8 & 0xff, status};
	int len = 0, ret;

	/* Start with a frame delimiter. */
	if (len >= SERIAL_BUF_SIZE) {
		LOG_ERR("Buffer overflow: %d", len);
		return -1;
	}
	ctx->uart_tx_buf[len++] = FRAME_DELIMITER;

	/* Add the frame header next. */
	ret = uart_escape(ctx->uart_tx_buf, SERIAL_BUF_SIZE, len, header,
			  sizeof(header));
	if (ret < 0) {
		LOG_ERR("Failed to escape header: %d", ret);
		return ret;
	}
	len += ret;

	/* Add the payload, if any. */
	if (payload && payload_len) {
		ret = uart_escape(ctx->uart_tx_buf, SERIAL_BUF_SIZE, len,
				  payload, payload_len);
		if (ret < 0) {
			LOG_ERR("Failed to escape payload: %d", ret);
			return ret;
		}
		len += ret;
	}

	/* End with a frame delimiter. */
	if (len >= SERIAL_BUF_SIZE) {
		LOG_ERR("Buffer overflow: %d", len);
		return -1;
	}
	ctx->uart_tx_buf[len++] = FRAME_DELIMITER;

	/* Send the response buffer. */
	ctx->uart_tx_buf_pos = 0;
	ctx->uart_tx_buf_len = len;
	uart_irq_tx_enable(ctx->uart_dev);

	return 0;
}

static void
qsoc_flash_bridge_rx_process_from_work(struct k_work *rx_process_work)
{
	struct qsoc_flash_bridge_ctx *ctx = CONTAINER_OF(
	    rx_process_work, struct qsoc_flash_bridge_ctx, rx_process_work);
	uint8_t cmd, status, response;
	uint16_t cmd_len;
	int ret;

	/* Unescape the received frame. */
	ret = uart_unescape(ctx->uart_rx_buf, ctx->uart_rx_buf_len);
	if (ret < 0) {
		LOG_ERR("Error unescaping frame: %d", ret);
		return;
	}
	cmd = ctx->uart_rx_buf[0];
	cmd_len = ctx->uart_rx_buf[1] | ctx->uart_rx_buf[2] << 8;

	/* Process the received frame and send the response. */
	switch (cmd) {
	case CMD_NOP:
		ret = uart_send_resp(ctx, cmd, 0, NULL, 0);
		if (ret < 0) {
			LOG_ERR("Error sending response: %d", ret);
			return;
		}
		break;
	case CMD_EXIT_FLASH_MODE:
		ret = uart_send_resp(ctx, cmd, 0, NULL, 0);
		if (ret < 0) {
			LOG_ERR("Error sending response: %d", ret);
		}

		shell_reinit_trigger(ctx);
		break;
	case CMD_TRANSFER: {
		const struct spi_buf tx = {.buf = ctx->uart_rx_buf + 3,
					   .len = cmd_len};
		const struct spi_buf_set tx_set = {.buffers = &tx, .count = 1};

		/*
		 * Put the data read from the QM35 device at the end of the
		 * uart_tx_buf.
		 * It will then be copied at the beginning of the buffer by the
		 * uart_escape() function before being sent over the UART.
		 */
		uint8_t *spi_rx_buf =
		    ctx->uart_tx_buf + SERIAL_BUF_SIZE - cmd_len;

		const struct spi_buf rx = {.buf = spi_rx_buf, .len = cmd_len};
		const struct spi_buf_set rx_set = {.buffers = &rx, .count = 1};

		ret = spi_transceive_dt(&ctx->hsspi_config->bus, &tx_set,
					&rx_set);
		status = ret < 0 ? ret : 0;

		ret = uart_send_resp(ctx, cmd, status, spi_rx_buf, cmd_len);
		if (ret < 0) {
			LOG_ERR("Error sending response: %d", ret);
			return;
		}
		break;
	}
	case CMD_SET_CS:
		hsspi_cs_gpio_set(ctx->hsspi_dev, ctx->uart_rx_buf[3] ? 0 : 1);

		ret = uart_send_resp(ctx, cmd, 0, NULL, 0);
		if (ret < 0) {
			LOG_ERR("Error sending response: %d", ret);
			return;
		}
		break;
	case CMD_RESET:
		hsspi_reset_gpio_set(ctx->hsspi_dev, 0);
		k_msleep(2);
		hsspi_reset_gpio_set(ctx->hsspi_dev, 1);

		ret = uart_send_resp(ctx, cmd, 0, NULL, 0);
		if (ret < 0) {
			LOG_ERR("Error sending response: %d", ret);
			return;
		}
		break;
	case CMD_READ_IRQ:
		ret = gpio_pin_get_dt(&ctx->hsspi_config->ss_irq);

		status = ret < 0 ? ret : 0;
		response = ret ? 1 : 0;
		ret = uart_send_resp(ctx, cmd, status, &response, 1);
		if (ret < 0) {
			LOG_ERR("Error sending response: %d", ret);
			return;
		}
		break;
	default:
		/* Bad frame received, prevent further processing. */
		LOG_ERR("Unknown command: %d", cmd);
		ctx->delimiter_received = false;
	}
}

static void qsoc_flash_bridge_uart_callback(const struct device *dev,
					    void *user_data)
{
	struct qsoc_flash_bridge_ctx *ctx = user_data;
	uint8_t recv[32];
	int i, recv_len;

	if (!uart_irq_update(dev)) {
		LOG_ERR("UART IRQ update failed");
		return;
	}

	if (uart_irq_rx_ready(dev) == 1) {
		recv_len = uart_fifo_read(dev, recv, sizeof(recv));
		for (i = 0; i < recv_len; i++) {
			if (recv[i] == FRAME_DELIMITER) {
				if (!ctx->uart_rx_buf_pos) {
					/* Start of frame. */
					ctx->delimiter_received = true;
					if (++ctx->empty_cmd_count >= 5) {
						/* Received 5 empty commands, exit. */
						shell_reinit_trigger(ctx);
						return;
					}
					continue;
				}

				/* End of frame, process the received data. */
				ctx->uart_rx_buf_len = ctx->uart_rx_buf_pos;
				k_work_submit(&ctx->rx_process_work);
				/* Reset position for next frame. */
				ctx->uart_rx_buf_pos = 0;
				ctx->empty_cmd_count = 0;
				continue;
			}

			if (ctx->delimiter_received) {
				if (ctx->uart_rx_buf_pos >= SERIAL_BUF_SIZE) {
					LOG_ERR("Buffer overflow: %d",
						ctx->uart_rx_buf_pos);
					return;
				}
				ctx->uart_rx_buf[ctx->uart_rx_buf_pos++] =
				    recv[i];
			}
		}
	}

	if (uart_irq_tx_ready(dev) == 1) {
		/* Transmission is ready, send the next bytes if available. */
		if (ctx->uart_tx_buf_pos < ctx->uart_tx_buf_len) {
			int ret = uart_fifo_fill(
			    dev, &ctx->uart_tx_buf[ctx->uart_tx_buf_pos],
			    ctx->uart_tx_buf_len - ctx->uart_tx_buf_pos);
			if (ret < 0) {
				LOG_ERR("Error sending data: %d", ret);
				return;
			}
			ctx->uart_tx_buf_pos += ret;
		} else {
			uart_irq_tx_disable(dev);
		}
	}
}

static void qsoc_flash_bridge_init_from_work(struct k_work *bridge_init_work)
{
	struct qsoc_flash_bridge_ctx *ctx = CONTAINER_OF(
	    bridge_init_work, struct qsoc_flash_bridge_ctx, bridge_init_work);
	int ret;

	/* Wait for the shell to be stopped. */
	while (shell_ready(ctx->shell))
		k_msleep(1);

	LOG_INF("The shell is stopped and the QSoC-Flash bridge is started "
		"on the UART.");
	LOG_INF("The shell will be restarted when CMD_EXIT_FLASH_MODE or "
		"'~~~~~' (5 tildes) are received.");

	/* Disable QM35 interrupts and resume the device. */
	ret = hsspi_disable_irq(ctx->hsspi_dev);
	if (ret < 0)
		LOG_ERR("Failed to disable QM35 interrupts: %d", ret);
	ret = pm_device_runtime_get(ctx->hsspi_config->bus.bus);
	if (ret < 0)
		LOG_ERR("Failed to resume QM35 device: %d", ret);

	/* Initialize the UART device. */
	ret = uart_irq_callback_user_data_set(
	    ctx->uart_dev, qsoc_flash_bridge_uart_callback, ctx);
	if (ret < 0)
		LOG_ERR("Failed to set UART callback: %d", ret);
	uart_irq_rx_enable(ctx->uart_dev);
}

static void shell_uninit_cb(const struct shell *sh, int res)
{
}

int qsoc_flash_bridge_cmd(const struct shell *shell, size_t argc, char **argv)
{
	struct qsoc_flash_bridge_ctx *ctx;
	int ret;

	/* Set up the QSoC-Flash bridge context. */
	ctx = k_calloc(1, sizeof(struct qsoc_flash_bridge_ctx));
	if (!ctx) {
		LOG_ERR("Failed to allocate memory for context");
		return -ENOMEM;
	}

	ctx->uart_rx_buf = k_malloc(SERIAL_BUF_SIZE);
	ctx->uart_tx_buf = k_malloc(SERIAL_BUF_SIZE);
	if (!ctx->uart_rx_buf || !ctx->uart_tx_buf) {
		LOG_ERR("Failed to allocate memory for buffers");
		ret = -ENOMEM;
		goto error;
	}

	k_work_init(&ctx->bridge_init_work, qsoc_flash_bridge_init_from_work);
	k_work_init(&ctx->shell_init_work, shell_init_from_work);
	k_work_init(&ctx->rx_process_work,
		    qsoc_flash_bridge_rx_process_from_work);
	ctx->uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
	ctx->hsspi_dev = DEVICE_DT_GET(DT_NODELABEL(qm35));
	ctx->hsspi_config = ctx->hsspi_dev->config;
	ctx->shell = shell;

	/* Set the prompt to the empty string to avoid the last shell prompt. */
	shell_prompt_change(shell, "");
	/* Stop the shell. */
	shell_uninit(shell, shell_uninit_cb);
	/* Start the QSoC-Flash bridge. */
	k_work_submit(&ctx->bridge_init_work);

	return 0;

error:
	k_free(ctx->uart_rx_buf);
	k_free(ctx->uart_tx_buf);
	k_free(ctx);
	return ret;
}
