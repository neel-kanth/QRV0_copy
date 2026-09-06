/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#ifndef CHERRY_PROXY_H
#define CHERRY_PROXY_H

#include <cherry/cherry.h>
#include <stdbool.h>
#include <stdint.h>
#ifndef CONFIG_CHERRY_ZEPHYR
#include <termios.h>
#else
typedef int speed_t;
#endif

/**
 * DOC: struct cherryproxy
 *
 * Cherry proxy context object.
 *
 * It holds the Cherry proxy runtime context, created at the
 * initialization and released at the close of Cherry proxy.
 */
struct cherry_proxy;

/**
 * cherry_proxy_create() - Initialize Cherry proxy context.
 * @device: Path to UCI char dev in the file system.
 * @vcom: Path to the VCOM device.
 *
 * This function returns immediately.
 *
 * Returns: Newly allocated Cherry proxy context or NULL in case of error.
 */
struct cherry_proxy *cherry_proxy_create(const char *device, const char *vcom,
					 const speed_t baud_rate);

/**
 * cherry_proxy_destroy() - Release and free Cherry proxy context.
 * @ctx: Cherry proxy context.
 *
 * Close the Cherry proxy context and release any allocated memory.
 */
void cherry_proxy_destroy(struct cherry_proxy *ctx);

/**
 * cherry_proxy_start() - Set the given calibration keys in device memory and on
 * device (re)connection and start the proxy.
 * @ctx: Cherry proxy context.
 * @calib: Calibration keys and values. Pointer and content must remain valid for the
 * duration of the Cherry proxy context.
 * Calibration data can be accessed at any moment by cherry proxy.
 *
 * Returns:
 *  - &CHERRY_ERR_NONE if succeed
 *  - &CHERRY_ERR_INTERNAL for other errors
 */
enum cherry_err cherry_proxy_start(struct cherry_proxy *ctx,
				   const struct cherry_calib *calib);

/**
 * cherry_proxy_stop() - Stop the proxy.
 * @ctx: Cherry proxy context.
 *
 * Stop the Cherry proxy.
 */
enum cherry_err cherry_proxy_stop(struct cherry_proxy *ctx);

/**
 * cherry_proxy_set_log_level() - Change the log level.
 * @ctx: Cherry procy context.
 * @level: Level of log.
 * @module: Module on which the log level change has to be applied.
 *
 * Configure the log level on selected module. In case of issue, no error event will
 * be reported to the app.
 *
 * This function returns immediately.
 *
 * Returns:
 *  - &CHERRY_ERR_NONE if succeed
 *  - &CHERRY_ERR_INTERNAL for other errors
 */
enum cherry_err cherry_proxy_set_log_level(struct cherry_proxy *ctx,
					   enum cherry_log_level level,
					   enum cherry_log_module module);

#endif /* CHERRY_PROXY_H */
